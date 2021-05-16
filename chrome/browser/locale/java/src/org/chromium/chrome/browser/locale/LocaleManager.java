// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.search_engines.DefaultSearchEngineDialogHelper;
import org.chromium.chrome.browser.search_engines.DefaultSearchEnginePromoDialog;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoState;
import org.chromium.chrome.browser.search_engines.SearchEnginePromoType;
import org.chromium.chrome.browser.search_engines.SogouPromoDialog;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.widget.PromoDialog;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.ui.base.PageTransition;

import java.lang.ref.WeakReference;
import java.util.List;

/**
 * Manager for some locale specific logics.
 * TODO(https://crbug.com/1198923) Turn this into a per-activity object.
 */
public class LocaleManager implements DefaultSearchEngineDialogHelper.Delegate {
    private static final String SPECIAL_LOCALE_ID = "US";

    private static final int SNACKBAR_DURATION_MS = 6000;

    private static LocaleManager sInstance;

    private boolean mSearchEnginePromoCompleted;
    private boolean mSearchEnginePromoShownThisSession;
    private boolean mSearchEnginePromoCheckedThisSession;

    // LocaleManager is a singleton and it should not have strong reference to UI objects.
    // SnackbarManager is owned by ChromeActivity and is not null as long as the activity is alive.
    private WeakReference<SnackbarManager> mSnackbarManager = new WeakReference<>(null);
    private LocaleTemplateUrlLoader mLocaleTemplateUrlLoader;
    @Nullable
    private SettingsLauncher mSettingsLauncher;

    private SnackbarController mSnackbarController = new SnackbarController() {
        @Override
        public void onDismissNoAction(Object actionData) {}

        @Override
        public void onAction(Object actionData) {
            assert mSettingsLauncher != null;
            Context context = ContextUtils.getApplicationContext();
            mSettingsLauncher.launchSettingsActivity(context, SearchEngineSettings.class);
        }
    };

    /**
     * @return An instance of the {@link LocaleManager}. This should only be called on UI thread.
     */
    @CalledByNative
    public static LocaleManager getInstance() {
        assert ThreadUtils.runningOnUiThread();
        return sInstance;
    }

    /**
     * Default constructor.
     */
    public LocaleManager() {
        @SearchEnginePromoState
        int state = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.LOCALE_MANAGER_SEARCH_ENGINE_PROMO_SHOW_STATE,
                SearchEnginePromoState.SHOULD_CHECK);
        mSearchEnginePromoCompleted = state == SearchEnginePromoState.CHECKED_AND_SHOWN;
    }

    /**
     * Starts listening to state changes of the phone.
     */
    public void startObservingPhoneChanges() {
        maybeAutoSwitchSearchEngine();
    }

    /**
     * Stops listening to state changes of the phone.
     */
    public void stopObservingPhoneChanges() {}

    /**
     * Starts recording metrics in deferred startup.
     */
    public void recordStartupMetrics() {}

    /**
     * @return Whether the Chrome instance is running in a special locale.
     */
    public boolean isSpecialLocaleEnabled() {
        return false;
    }

    /**
     * @return The country id of the special locale.
     */
    public String getSpecialLocaleId() {
        return SPECIAL_LOCALE_ID;
    }

    /**
     * Adds local search engines for special locale.
     */
    public void addSpecialSearchEngines() {
        if (!isSpecialLocaleEnabled()) return;
        getLocaleTemplateUrlLoader().loadTemplateUrls();
    }

    /**
     * Removes local search engines for special locale.
     */
    public void removeSpecialSearchEngines() {
        if (isSpecialLocaleEnabled()) return;
        getLocaleTemplateUrlLoader().removeTemplateUrls();
    }

    /**
     * Overrides the default search engine to a different search engine we designate. This is a
     * no-op if the user has manually changed DSP settings.
     */
    void overrideDefaultSearchEngine() {
        if (!isSearchEngineAutoSwitchEnabled() || !isSpecialLocaleEnabled()) return;
        getLocaleTemplateUrlLoader().overrideDefaultSearchProvider();
        showSnackbar(ContextUtils.getApplicationContext().getString(R.string.using_sogou));
    }

    /**
     * Reverts the temporary change made in {@link #overrideDefaultSearchEngine()}. This is a no-op
     * if the user has manually changed DSP settings.
     */
    private void revertDefaultSearchEngineOverride() {
        if (!isSearchEngineAutoSwitchEnabled() || isSpecialLocaleEnabled()) return;
        getLocaleTemplateUrlLoader().setGoogleAsDefaultSearch();
        showSnackbar(ContextUtils.getApplicationContext().getString(R.string.using_google));
    }

    /**
     * Switches the default search engine based on the current locale, if the user has delegated
     * Chrome to do so. This method also adds some special engines to user's search engine list, as
     * long as the user is in this locale.
     */
    protected void maybeAutoSwitchSearchEngine() {
        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        boolean wasInSpecialLocale = preferences.readBoolean(
                ChromePreferenceKeys.LOCALE_MANAGER_WAS_IN_SPECIAL_LOCALE, false);
        boolean isInSpecialLocale = isSpecialLocaleEnabled();
        if (wasInSpecialLocale && !isInSpecialLocale) {
            revertDefaultSearchEngineOverride();
            removeSpecialSearchEngines();
        } else if (isInSpecialLocale && !wasInSpecialLocale) {
            addSpecialSearchEngines();
            overrideDefaultSearchEngine();
        } else if (isInSpecialLocale) {
            // As long as the user is in the special locale, special engines should be in the list.
            addSpecialSearchEngines();
        }
        preferences.writeBoolean(
                ChromePreferenceKeys.LOCALE_MANAGER_WAS_IN_SPECIAL_LOCALE, isInSpecialLocale);
    }

    /**
     * Shows a promotion dialog about search engines depending on Locale and other conditions.
     * See {@link LocaleManager#getSearchEnginePromoShowType()} for possible types and logic.
     *
     * @param activity    Activity showing the dialog.
     * @param onSearchEngineFinalized Notified when the search engine has been finalized.  This can
     *                                either mean no dialog is needed, or the dialog was needed and
     *                                the user completed the dialog with a valid selection.
     */
    public void showSearchEnginePromoIfNeeded(
            final Activity activity, final @Nullable Callback<Boolean> onSearchEngineFinalized) {
        assert LibraryLoader.getInstance().isInitialized();
        TemplateUrlServiceFactory.get().runWhenLoaded(() -> {
            handleSearchEnginePromoWithTemplateUrlsLoaded(activity, onSearchEngineFinalized);
        });
    }

    private void handleSearchEnginePromoWithTemplateUrlsLoaded(
            final Activity activity, final @Nullable Callback<Boolean> onSearchEngineFinalized) {
        assert TemplateUrlServiceFactory.get().isLoaded();

        final Callback<Boolean> finalizeInternalCallback = (result) -> {
            if (result != null && result) {
                mSearchEnginePromoCheckedThisSession = true;
            } else {
                @SearchEnginePromoType
                int promoType = getSearchEnginePromoShowType();
                if (promoType == SearchEnginePromoType.SHOW_EXISTING
                        || promoType == SearchEnginePromoType.SHOW_NEW) {
                    onUserLeavePromoDialogWithNoConfirmedChoice(promoType);
                }
            }
            if (onSearchEngineFinalized != null) onSearchEngineFinalized.onResult(result);
        };
        if (TemplateUrlServiceFactory.get().isDefaultSearchManaged()
                || ApiCompatibilityUtils.isDemoUser()) {
            finalizeInternalCallback.onResult(true);
            return;
        }

        @SearchEnginePromoType
        final int shouldShow = getSearchEnginePromoShowType();
        Supplier<PromoDialog> dialogSupplier;

        switch (shouldShow) {
            case SearchEnginePromoType.DONT_SHOW:
                finalizeInternalCallback.onResult(true);
                return;
            case SearchEnginePromoType.SHOW_SOGOU:
                dialogSupplier = ()
                        -> new SogouPromoDialog(activity, this::onSelectSearchEngine,
                                finalizeInternalCallback, mSettingsLauncher);
                break;
            case SearchEnginePromoType.SHOW_EXISTING:
            case SearchEnginePromoType.SHOW_NEW:
                dialogSupplier = ()
                        -> new DefaultSearchEnginePromoDialog(
                                activity, LocaleManager.this, shouldShow, finalizeInternalCallback);
                break;
            default:
                assert false;
                finalizeInternalCallback.onResult(true);
                return;
        }

        // If the activity has been destroyed by the time the TemplateUrlService has
        // loaded, then do not attempt to show the dialog.
        if (ApplicationStatus.getStateForActivity(activity) == ActivityState.DESTROYED) {
            finalizeInternalCallback.onResult(false);
            return;
        }
        dialogSupplier.get().show();
        mSearchEnginePromoShownThisSession = true;
    }

    /**
     * Called when search engine to use is selected on SogouPromoDialog.
     * @param useSogou {@code true} if Sogou engine is chosen.
     */
    private void onSelectSearchEngine(boolean useSogou) {
        setSearchEngineAutoSwitch(useSogou);
        addSpecialSearchEngines();
        if (useSogou) overrideDefaultSearchEngine();
    }

    /**
     * @return Whether auto switch for search engine is enabled.
     */
    public boolean isSearchEngineAutoSwitchEnabled() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.LOCALE_MANAGER_AUTO_SWITCH, false);
    }

    /**
     * Sets whether auto switch for search engine is enabled.
     */
    public void setSearchEngineAutoSwitch(boolean isEnabled) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.LOCALE_MANAGER_AUTO_SWITCH, isEnabled);
    }

    /**
     * Sets the {@link SnackbarManager} used by this instance.
     * @param manager SnackbarManager instance.
     */
    public void setSnackbarManager(SnackbarManager manager) {
        mSnackbarManager = new WeakReference<SnackbarManager>(manager);
    }

    /**
     * Sets the settings launcher for search engines.
     * @param settingsLauncher Launcher to start search engine settings on the snackbar UI.
     */
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }

    private void showSnackbar(CharSequence title) {
        SnackbarManager manager = mSnackbarManager.get();
        if (manager == null) return;

        Context context = ContextUtils.getApplicationContext();
        Snackbar snackbar = Snackbar.make(title, mSnackbarController, Snackbar.TYPE_NOTIFICATION,
                Snackbar.UMA_SPECIAL_LOCALE);
        snackbar.setDuration(SNACKBAR_DURATION_MS);
        snackbar.setAction(context.getString(R.string.settings), null);
        manager.showSnackbar(snackbar);
    }

    /**
     * @return Whether and which search engine promo should be shown.
     */
    @SearchEnginePromoType
    public int getSearchEnginePromoShowType() {
        if (!isSpecialLocaleEnabled()) return SearchEnginePromoType.DONT_SHOW;
        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        if (preferences.readBoolean(ChromePreferenceKeys.LOCALE_MANAGER_PROMO_SHOWN, false)) {
            return SearchEnginePromoType.DONT_SHOW;
        }
        return SearchEnginePromoType.SHOW_SOGOU;
    }

    /**
     * @return The referral ID to be passed when searching with Yandex as the DSE.
     */
    @CalledByNative
    protected String getYandexReferralId() {
        return "";
    }

    /**
     * @return The referral ID to be passed when searching with Mail.RU as the DSE.
     */
    @CalledByNative
    protected String getMailRUReferralId() {
        return "";
    }

    /**
     * To be called after the user has made a selection from a search engine promo dialog.
     * @param type The type of search engine promo dialog that was shown.
     * @param keywords The keywords for all search engines listed in the order shown to the user.
     * @param keyword The keyword for the search engine chosen.
     */
    protected void onUserSearchEngineChoiceFromPromoDialog(
            @SearchEnginePromoType int type, List<String> keywords, String keyword) {
        TemplateUrlServiceFactory.get().setSearchEngine(keyword);
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.LOCALE_MANAGER_SEARCH_ENGINE_PROMO_SHOW_STATE,
                SearchEnginePromoState.CHECKED_AND_SHOWN);
        mSearchEnginePromoCompleted = true;
    }

    /**
     * To be called when the search engine promo dialog is dismissed without the user confirming
     * a valid search engine selection.
     */
    protected void onUserLeavePromoDialogWithNoConfirmedChoice(@SearchEnginePromoType int type) {}

    private LocaleTemplateUrlLoader getLocaleTemplateUrlLoader() {
        if (mLocaleTemplateUrlLoader == null) {
            mLocaleTemplateUrlLoader = new LocaleTemplateUrlLoader(getSpecialLocaleId());
        }
        return mLocaleTemplateUrlLoader;
    }

    @Override
    public List<TemplateUrl> getSearchEnginesForPromoDialog(@SearchEnginePromoType int promoType) {
        throw new IllegalStateException(
                "Not applicable unless existing or new promos are required");
    }

    @Override
    public void onUserSearchEngineChoice(
            @SearchEnginePromoType int type, List<String> keywords, String keyword) {
        onUserSearchEngineChoiceFromPromoDialog(type, keywords, keyword);
    }

    /** Set a LocaleManager instance. This is called only by AppHooks. */
    public static void setInstance(LocaleManager instance) {
        sInstance = instance;
    }

    /**
     * Record any locale based metrics related with the search widget. Recorded on initialization
     * only.
     * @param widgetPresent Whether there is at least one search widget on home screen.
     */
    public void recordLocaleBasedSearchWidgetMetrics(boolean widgetPresent) {}

    /**
     * @return Whether the search engine promo has been shown and the user selected a valid option
     *         and successfully completed the promo.
     */
    public boolean hasCompletedSearchEnginePromo() {
        return mSearchEnginePromoCompleted;
    }

    /**
     * @return Whether the search engine promo has been shown in this session.
     */
    public boolean hasShownSearchEnginePromoThisSession() {
        return mSearchEnginePromoShownThisSession;
    }

    /**
     * @return Whether we still have to check for whether search engine dialog is necessary.
     */
    public boolean needToCheckForSearchEnginePromo() {
        if (ChromeFeatureList.isInitialized()
                && !ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SEARCH_ENGINE_PROMO_EXISTING_DEVICE)) {
            return false;
        }
        @SearchEnginePromoState
        int state = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.LOCALE_MANAGER_SEARCH_ENGINE_PROMO_SHOW_STATE,
                SearchEnginePromoState.SHOULD_CHECK);
        return !mSearchEnginePromoCheckedThisSession
                && state == SearchEnginePromoState.SHOULD_CHECK;
    }

    /**
     * Record any locale based metrics related with search. Recorded per search.
     * @param isFromSearchWidget Whether the search was performed from the search widget.
     * @param url Url for the search made.
     * @param transition The transition type for the navigation.
     */
    public void recordLocaleBasedSearchMetrics(
            boolean isFromSearchWidget, String url, @PageTransition int transition) {}

    /**
     * @return Whether the user requires special handling.
     */
    public boolean isSpecialUser() {
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_ENABLE_SPECIAL_USER)) {
            return true;
        }
        return false;
    }

    /**
     * Record metrics related to user type.
     */
    @CalledByNative
    public void recordUserTypeMetrics() {}

    /** Set a LocaleManager to be used for testing. */
    @VisibleForTesting
    public static void setInstanceForTest(LocaleManager instance) {
        sInstance = instance;
    }
}
