// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.locale;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.Nullable;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.supplier.Supplier;
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
 * Base class for defining methods where different behavior is required by downstream targets.
 * The correct version of {@link LocaleManagerDelegateImpl} will be determined at compile time
 * via build rules.
 */
public class LocaleManagerDelegate {
    private static final String SPECIAL_LOCALE_ID = "US";

    private static final int SNACKBAR_DURATION_MS = 6000;

    private boolean mSearchEnginePromoCompleted;
    private boolean mSearchEnginePromoShownThisSession;
    private boolean mSearchEnginePromoCheckedThisSession;

    // LocaleManager is a singleton and it should not have strong reference to UI objects.
    // SnackbarManager is owned by ChromeActivity and is not null as long as the activity is alive.
    private WeakReference<SnackbarManager> mSnackbarManager = new WeakReference<>(null);
    private LocaleTemplateUrlLoader mLocaleTemplateUrlLoader;
    @Nullable
    private SettingsLauncher mSettingsLauncher;
    private DefaultSearchEngineDialogHelper.Delegate mSearchEngineHelperDelegate;

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
     * Default constructor.
     */
    public LocaleManagerDelegate() {
        @SearchEnginePromoState
        int state = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.LOCALE_MANAGER_SEARCH_ENGINE_PROMO_SHOW_STATE,
                SearchEnginePromoState.SHOULD_CHECK);
        mSearchEnginePromoCompleted = state == SearchEnginePromoState.CHECKED_AND_SHOWN;
    }

    /**
     * Sets the delegate for {@link DefaultSearchEngineDialogHelper}.
     * @param delegate Delegate used to select/notify the default search engine.
     */
    public void setDefaulSearchEngineDelegate(DefaultSearchEngineDialogHelper.Delegate delegate) {
        mSearchEngineHelperDelegate = delegate;
    }

    /**
     * @see {@link LocaleManager#startObservingPhoneChanges()}
     */
    public void startObservingPhoneChanges() {
        maybeAutoSwitchSearchEngine();
    }

    /**
     * @see {@link LocaleManager#stopObservingPhoneChanges()}
     */
    public void stopObservingPhoneChanges() {}

    /**
     * @see {@link LocaleManager#recordStartupMetrics()}
     */
    public void recordStartupMetrics() {}

    /**
     * @see {@link LocaleManager#isSpecialLocaleEnabled()}
     */
    public boolean isSpecialLocaleEnabled() {
        return false;
    }

    /**
     * @see {@link LocaleManager#getSpecialLocaleId()}
     */
    public String getSpecialLocaleId() {
        return SPECIAL_LOCALE_ID;
    }

    /**
     * @see {@link LocaleManager#addSpecialSearchEngines()}
     */
    public void addSpecialSearchEngines() {
        if (!isSpecialLocaleEnabled()) return;
        getLocaleTemplateUrlLoader().loadTemplateUrls();
    }

    /**
     * @see {@link LocaleManager#removeSpecialSearchEngines()}
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
     * @see {@link LocaleManager#maybeAutoSwitchSearchEngine()}
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
     * @see {@link LocaleManager#showSearchEnginePromoIfNeeded()}
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
                        -> new DefaultSearchEnginePromoDialog(activity, mSearchEngineHelperDelegate,
                                shouldShow, finalizeInternalCallback);
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
    private boolean isSearchEngineAutoSwitchEnabled() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.LOCALE_MANAGER_AUTO_SWITCH, false);
    }

    /**
     * @see {@link LocaleManager#setSearchEngineAutoSwitch()}
     */
    public void setSearchEngineAutoSwitch(boolean isEnabled) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.LOCALE_MANAGER_AUTO_SWITCH, isEnabled);
    }

    /**
     * @see {@link LocaleManager#setSnackbarManager()}
     */
    public void setSnackbarManager(SnackbarManager manager) {
        mSnackbarManager = new WeakReference<SnackbarManager>(manager);
    }

    /**
     * @see {@link LocaleManager#setSettingsLauncher()}
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
     * @see {@link LocaleManager#getSearchEnginePromoShowType()}
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
     * @see {@link LocaleManager#getYandexReferralId()}
     */
    public String getYandexReferralId() {
        return "";
    }

    /**
     * @see {@link LocaleManager#getMailRUReferralId()}
     */
    public String getMailRUReferralId() {
        return "";
    }

    public List<TemplateUrl> getSearchEnginesForPromoDialog(@SearchEnginePromoType int promoType) {
        throw new IllegalStateException(
                "Not applicable unless existing or new promos are required");
    }

    /**
     * @see {@link LocaleManager#onUserSearchEngineChoiceFromPromoDialog()}
     */
    public void onUserSearchEngineChoiceFromPromoDialog(
            @SearchEnginePromoType int type, List<String> keywords, String keyword) {
        TemplateUrlServiceFactory.get().setSearchEngine(keyword);
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.LOCALE_MANAGER_SEARCH_ENGINE_PROMO_SHOW_STATE,
                SearchEnginePromoState.CHECKED_AND_SHOWN);
        mSearchEnginePromoCompleted = true;
    }

    /**
     * @see {@link LocaleManager#onUserLeavePromoDialogWithNoConfirmedChoice()}
     */
    public void onUserLeavePromoDialogWithNoConfirmedChoice(@SearchEnginePromoType int type) {}

    private LocaleTemplateUrlLoader getLocaleTemplateUrlLoader() {
        if (mLocaleTemplateUrlLoader == null) {
            mLocaleTemplateUrlLoader = new LocaleTemplateUrlLoader(getSpecialLocaleId());
        }
        return mLocaleTemplateUrlLoader;
    }

    /**
     * @see {@link LocaleManager#recordLocaleBasedSearchWidgetMetrics()}
     */
    public void recordLocaleBasedSearchWidgetMetrics(boolean widgetPresent) {}

    /**
     * @see {@link LocaleManager#hasCompletedSearchEnginePromo()}
     */
    public boolean hasCompletedSearchEnginePromo() {
        return mSearchEnginePromoCompleted;
    }

    /**
     * @see {@link LocaleManager#hasShownSearchEnginePromoThisSession()}
     */
    public boolean hasShownSearchEnginePromoThisSession() {
        return mSearchEnginePromoShownThisSession;
    }

    /**
     * @see {@link LocaleManager#needToCheckForSearchEnginePromo()}
     */
    public boolean needToCheckForSearchEnginePromo() {
        @SearchEnginePromoState
        int state = SharedPreferencesManager.getInstance().readInt(
                ChromePreferenceKeys.LOCALE_MANAGER_SEARCH_ENGINE_PROMO_SHOW_STATE,
                SearchEnginePromoState.SHOULD_CHECK);
        return !mSearchEnginePromoCheckedThisSession
                && state == SearchEnginePromoState.SHOULD_CHECK;
    }

    /**
     * @see {@link LocaleManager#recordLocaleBasedSearchMetrics()}
     */
    public void recordLocaleBasedSearchMetrics(
            boolean isFromSearchWidget, String url, @PageTransition int transition) {}

    /**
     * @see {@link LocaleManager#isSpecialUser()}
     */
    public boolean isSpecialUser() {
        return CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_ENABLE_SPECIAL_USER);
    }

    /**
     * @see {@link LocaleManager#recordUserTypeMetrics()}
     */
    public void recordUserTypeMetrics() {}
}
