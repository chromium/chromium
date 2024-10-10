// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.app.ActivityOptionsCompat;

import org.jni_zero.CheckDiscard;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.metrics.UmaActivityObserver;
import org.chromium.chrome.browser.omnibox.BackKeyBehaviorDelegate;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedderUiOverrides;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.toolbar.VoiceToolbarButtonController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;

/** Queries the user's default search engine and shows autocomplete suggestions. */
public class SearchActivity extends AsyncInitializationActivity
        implements SnackbarManageable, BackKeyBehaviorDelegate, UrlFocusChangeListener {
    // Shared with other org.chromium.chrome.browser.searchwidget classes.
    protected static final String TAG = "searchwidget";

    public static final String EXTRA_FROM_SEARCH_ACTIVITY =
            "org.chromium.chrome.browser.searchwidget.FROM_SEARCH_ACTIVITY";

    @VisibleForTesting
    /* package */ static final String USED_ANY_FROM_SEARCH_WIDGET = "SearchWidget.WidgetSelected";

    @VisibleForTesting
    /* package */ static final String USED_TEXT_FROM_SHORTCUTS_WIDGET =
            "QuickActionSearchWidget.TextQuery";

    @VisibleForTesting
    /* package */ static final String USED_VOICE_FROM_SHORTCUTS_WIDGET =
            "QuickActionSearchWidget.VoiceQuery";

    @VisibleForTesting
    /* package */ static final String USED_LENS_FROM_SHORTCUTS_WIDGET =
            "QuickActionSearchWidget.LensQuery";

    @VisibleForTesting
    /* package */ static final String HISTOGRAM_LAUNCHED_WITH_QUERY =
            "Android.Omnibox.SearchActivity.LaunchedWithQuery";

    @VisibleForTesting
    /* package */ static final String HISTOGRAM_INTENT_ORIGIN =
            "Android.Omnibox.SearchActivity.IntentOrigin";

    private static final String HISTOGRAM_REQUESTED_SEARCH_TYPE = //
            "Android.Omnibox.SearchActivity.RequestedSearchType";
    private static final String HISTOGRAM_INTENT_ACTIVITY_PRESENT =
            "Android.Omnibox.SearchActivity.ActivityPresent";

    @VisibleForTesting
    /* package */ static final String HISTOGRAM_INTENT_REFERRER_VALID =
            "Android.Omnibox.SearchActivity.ReferrerValid";

    @VisibleForTesting
    /* package */ static final String HISTOGRAM_NAVIGATION_TARGET_TYPE =
            "Android.Omnibox.SearchActivity.NavigationTargetType";

    @VisibleForTesting
    /* package */ static final String HISTOGRAM_SESSION_TERMINATION_REASON =
            "Android.Omnibox.SearchActivity.SessionTerminationReason";

    /** Controls whether Referrer App ID is passed to Search Results Page via client= param. */
    public static final BooleanCachedFieldTrialParameter SEARCH_IN_CCT_APPLY_REFERRER_ID =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.SEARCH_IN_CCT, "apply_referrer_id", false);

    // NOTE: This is used to capture HISTOGRAM_NAVIGATION_TARGET_TYPE.
    // Do not shuffle or reassign values.
    @VisibleForTesting
    @IntDef({
        NavigationTargetType.URL,
        NavigationTargetType.SEARCH,
        NavigationTargetType.NATIVE_PAGE,
        NavigationTargetType.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface NavigationTargetType {
        int URL = 0;
        int SEARCH = 1;
        int NATIVE_PAGE = 2;
        int COUNT = 3;
    }

    // NOTE: This is used to capture HISTOGRAM_SESSION_TERMINATION_REASON.
    // Do not shuffle or reassign values.
    @IntDef({
        TerminationReason.NAVIGATION,
        TerminationReason.UNSPECIFIED,
        TerminationReason.TAP_OUTSIDE,
        TerminationReason.BACK_KEY_PRESSED,
        TerminationReason.OMNIBOX_FOCUS_LOST,
        TerminationReason.ACTIVITY_FOCUS_LOST,
        TerminationReason.FRE_NOT_COMPLETED,
        TerminationReason.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface TerminationReason {
        int NAVIGATION = 0;
        int UNSPECIFIED = 1;
        int TAP_OUTSIDE = 2;
        int BACK_KEY_PRESSED = 3;
        int OMNIBOX_FOCUS_LOST = 4;
        int ACTIVITY_FOCUS_LOST = 5;
        int FRE_NOT_COMPLETED = 6;
        int COUNT = 7;
    }

    @VisibleForTesting /* package */ static final String CCT_CLIENT_PACKAGE_PREFIX = "app-cct-";

    /** Notified about events happening inside a SearchActivity. */
    public static class SearchActivityDelegate {
        /**
         * Called when {@link SearchActivity#triggerLayoutInflation} is deciding whether to continue
         * loading the native library immediately.
         *
         * @return Whether or not native initialization should proceed immediately.
         */
        boolean shouldDelayNativeInitialization() {
            return false;
        }

        /**
         * Called to launch the search engine dialog if it's needed.
         *
         * @param activity Activity that is launching the dialog.
         * @param onSearchEngineFinalized Called when the dialog has been dismissed.
         */
        void showSearchEngineDialogIfNeeded(
                Activity activity, Callback<Boolean> onSearchEngineFinalized) {
            LocaleManager.getInstance()
                    .showSearchEnginePromoIfNeeded(activity, onSearchEngineFinalized);
        }

        /** Called when {@link SearchActivity#finishDeferredInitialization} is done. */
        void onFinishDeferredInitialization() {}
    }

    /** Notified about events happening for the SearchActivity. */
    private static SearchActivityDelegate sDelegate;

    // Incoming intent request type. See {@link SearchActivityUtils#IntentOrigin}.
    @IntentOrigin Integer mIntentOrigin;
    // Incoming intent search type. See {@link SearchActivityUtils#SearchType}.
    @SearchType Integer mSearchType;

    /** Whether the user is now allowed to perform searches. */
    private boolean mIsActivityUsable;

    /** Input submitted before before the native library was loaded. */
    private OmniboxLoadUrlParams mQueuedParams;

    private LocationBarCoordinator mLocationBarCoordinator;
    private SearchActivityLocationBarLayout mSearchBox;

    private SnackbarManager mSnackbarManager;
    private Tab mTab;
    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();

    // SearchBoxDataProvider and LocationBarEmbedderUiOverrides are passed to several child
    // components upon construction. Ensure we don't accidentally introduce disconnection by
    // keeping only a single live instance here.
    private final SearchBoxDataProvider mSearchBoxDataProvider = new SearchBoxDataProvider();
    private final LocationBarEmbedderUiOverrides mLocationBarUiOverrides =
            new LocationBarEmbedderUiOverrides();
    private UmaActivityObserver mUmaActivityObserver;

    public SearchActivity() {
        mUmaActivityObserver = new UmaActivityObserver(this);
        mLocationBarUiOverrides.setForcedPhoneStyleOmnibox();
    }

    @Override
    protected boolean shouldDelayBrowserStartup() {
        return true;
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ActivityWindowAndroid(
                this,
                /* listenToActivityState= */ true,
                new ActivityKeyboardVisibilityDelegate(new WeakReference(this)),
                getIntentRequestTracker()) {
            @Override
            public ModalDialogManager getModalDialogManager() {
                return SearchActivity.this.getModalDialogManager();
            }
        };
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(
                new AppModalPresenter(this), ModalDialogManager.ModalDialogType.APP);
    }

    @Override
    protected void triggerLayoutInflation() {
        enableHardwareAcceleration();
        mSnackbarManager = new SnackbarManager(this, findViewById(android.R.id.content), null);
        boolean isIncognito = SearchActivityUtils.getIntentIncognitoStatus(getIntent());
        mSearchBoxDataProvider.initialize(this, isIncognito);

        ViewGroup rootView = (ViewGroup) getWindow().getDecorView().getRootView();
        // Setting fitsSystemWindows to false ensures that the root view doesn't consume the
        // insets.
        rootView.setFitsSystemWindows(false);

        var contentView = createContentView();
        setContentView(contentView);

        // Build the search box.
        mSearchBox = contentView.findViewById(R.id.search_location_bar);
        View anchorView = contentView.findViewById(R.id.toolbar);

        // Update the status bar's color based on the toolbar color.
        Drawable anchorViewBackground = anchorView.getBackground();
        assert anchorViewBackground instanceof GradientDrawable
                : "Unsupported background drawable.";
        if (anchorViewBackground instanceof GradientDrawable) {
            int anchorViewColor =
                    ((GradientDrawable) anchorViewBackground).getColor().getDefaultColor();
            StatusBarColorController.setStatusBarColor(this.getWindow(), anchorViewColor);
        }

        BackPressManager backPressManager = new BackPressManager();
        getOnBackPressedDispatcher().addCallback(this, backPressManager.getCallback());

        mLocationBarCoordinator =
                new LocationBarCoordinator(
                        mSearchBox,
                        anchorView,
                        mProfileSupplier,
                        mSearchBoxDataProvider,
                        null,
                        new WindowDelegate(getWindow()),
                        getWindowAndroid(),
                        /* activityTabSupplier= */ () -> null,
                        getModalDialogManagerSupplier(),
                        /* shareDelegateSupplier= */ null,
                        /* incognitoStateProvider= */ null,
                        getLifecycleDispatcher(),
                        this::loadUrl,
                        /* backKeyBehavior= */ this,
                        /* pageInfoAction= */ (tab, pageInfoHighlight) -> {},
                        IntentHandler::bringTabToFront,
                        /* saveOfflineButtonState= */ (tab) -> false,
                        /*omniboxUma*/ (url, transition, isNtp) -> {},
                        TabWindowManagerSingleton::getInstance,
                        /* bookmarkState= */ (url) -> false,
                        VoiceToolbarButtonController::isToolbarMicEnabled,
                        /* merchantTrustSignalsCoordinatorSupplier= */ null,
                        new OmniboxActionDelegateImpl(
                                this,
                                () -> mSearchBoxDataProvider.getTab(),
                                // TODO(ender): phase out callbacks when the modules below are
                                // components.
                                // Open URL in an existing, else new regular tab.
                                url -> {
                                    Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
                                    intent.setComponent(
                                            new ComponentName(
                                                    getApplicationContext(),
                                                    ChromeLauncherActivity.class));
                                    intent.putExtra(
                                            WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB,
                                            true);
                                    startActivity(intent);
                                },
                                // Open Incognito Tab callback:
                                () ->
                                        startActivity(
                                                IntentHandler.createTrustedOpenNewTabIntent(
                                                        this, true)),
                                // Open Password Settings callback:
                                () ->
                                        PasswordManagerLauncher.showPasswordSettings(
                                                this,
                                                getProfileProviderSupplier()
                                                        .get()
                                                        .getOriginalProfile(),
                                                ManagePasswordsReferrer.CHROME_SETTINGS,
                                                () -> getModalDialogManager(),
                                                /* managePasskeys= */ false),
                                // Open Quick Delete Dialog callback:
                                null),
                        null,
                        backPressManager,
                        /* OmniboxSuggestionsDropdownScrollListener= */ null,
                        /* tabModelSelectorSupplier= */ null,
                        mLocationBarUiOverrides,
                        null,
                        /* bottomWindowPaddingSupplier */ () -> 0,
                        /* onLongClickListener= */ null);
        mLocationBarCoordinator.setUrlBarFocusable(true);
        mLocationBarCoordinator.setShouldShowMicButtonWhenUnfocused(true);
        mLocationBarCoordinator.getOmniboxStub().addUrlFocusChangeListener(this);

        // Kick off everything needed for the user to type into the box.
        handleNewIntent(getIntent(), false);

        // Kick off loading of the native library.
        if (!getActivityDelegate().shouldDelayNativeInitialization()) {
            mHandler.post(this::startDelayedNativeInitialization);
        }

        onInitialLayoutInflationComplete();
    }

    /**
     * Process newly received intent.
     *
     * @param intent the intent to be processed
     * @param activityPresent whether activity was already showing when the intent was received
     */
    @VisibleForTesting
    /* package */ void handleNewIntent(Intent intent, boolean activityPresent) {
        mIntentOrigin = SearchActivityUtils.getIntentOrigin(intent);
        mSearchType = SearchActivityUtils.getIntentSearchType(intent);

        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_INTENT_ORIGIN, mIntentOrigin, IntentOrigin.COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_REQUESTED_SEARCH_TYPE, mSearchType, SearchType.COUNT);
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_INTENT_ACTIVITY_PRESENT, activityPresent);

        recordUsage(mIntentOrigin, mSearchType);

        switch (mIntentOrigin) {
            case IntentOrigin.CUSTOM_TAB:
                // Note: this may be refined by refinePageClassWithProfile().
                mSearchBoxDataProvider.setPageClassification(PageClassification.OTHER_ON_CCT_VALUE);
                mLocationBarUiOverrides
                        .setLensEntrypointAllowed(false)
                        .setVoiceEntrypointAllowed(false);
                break;

            case IntentOrigin.QUICK_ACTION_SEARCH_WIDGET:
                mLocationBarUiOverrides
                        .setLensEntrypointAllowed(true)
                        .setVoiceEntrypointAllowed(true);
                mSearchBoxDataProvider.setPageClassification(
                        PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE);
                break;

            case IntentOrigin.HUB:
                // Lens/voice input aren't supported for hub search.
                mLocationBarUiOverrides
                        .setLensEntrypointAllowed(false)
                        .setVoiceEntrypointAllowed(false);
                mSearchBoxDataProvider.setPageClassification(PageClassification.ANDROID_HUB_VALUE);
                setHubSearchBoxVisualElements();
                break;

            case IntentOrigin.SEARCH_WIDGET:
                // fallthrough

            default:
                mLocationBarUiOverrides
                        .setLensEntrypointAllowed(false)
                        .setVoiceEntrypointAllowed(true);
                mSearchBoxDataProvider.setPageClassification(
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE);
                break;
        }

        var profile = mProfileSupplier.get();
        if (profile != null) refinePageClassWithProfile(profile);

        mSearchBoxDataProvider.setCurrentUrl(SearchActivityUtils.getIntentUrl(intent));
        beginQuery();
    }

    /** Translate current intent origin and extras to a PageClassification. */
    @VisibleForTesting
    /* package */ void refinePageClassWithProfile(@NonNull Profile profile) {
        int pageClass = mSearchBoxDataProvider.getPageClassification(false);

        // Verify if the PageClassification can be refined.
        var url = SearchActivityUtils.getIntentUrl(getIntent());
        if (pageClass != PageClassification.OTHER_ON_CCT_VALUE || GURL.isEmptyOrInvalid(url)) {
            return;
        }

        var templateSvc = TemplateUrlServiceFactory.getForProfile(profile);
        if (templateSvc != null && templateSvc.isSearchResultsPageFromDefaultSearchProvider(url)) {
            mSearchBoxDataProvider.setPageClassification(
                    PageClassification.SEARCH_RESULT_PAGE_ON_CCT_VALUE);
        } else {
            mSearchBoxDataProvider.setPageClassification(PageClassification.OTHER_ON_CCT_VALUE);
        }
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        ActivityProfileProvider profileProvider =
                new ActivityProfileProvider(getLifecycleDispatcher()) {
                    @Nullable
                    @Override
                    protected OTRProfileID createOffTheRecordProfileID() {
                        throw new IllegalStateException(
                                "Attempting to access incognito from the search activity");
                    }
                };
        profileProvider.onAvailable(
                (provider) -> {
                    mProfileSupplier.set(profileProvider.get().getOriginalProfile());
                });
        return profileProvider;
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        if (mProfileSupplier.hasValue()) {
            finishNativeInitializationWithProfile(mProfileSupplier.get());
        } else {
            new OneShotCallback<>(
                    mProfileSupplier,
                    (profile) -> {
                        if (isDestroyed()) return;
                        finishNativeInitializationWithProfile(profile);
                    });
        }
    }

    private void finishNativeInitializationWithProfile(Profile profile) {
        refinePageClassWithProfile(profile);
        WebContents webContents = WebContentsFactory.createWebContents(profile, false, false);
        mTab =
                new TabBuilder(profile)
                        .setWindow(getWindowAndroid())
                        .setLaunchType(TabLaunchType.FROM_EXTERNAL_APP)
                        .setWebContents(webContents)
                        .setDelegateFactory(new SearchActivityTabDelegateFactory())
                        .build();
        mTab.loadUrl(new LoadUrlParams(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL));

        if (ChromeFeatureList.sAndroidHubSearch.isEnabled() && mIntentOrigin == IntentOrigin.HUB) {
            setHubSearchBoxUrlBarElements();
        }

        mSearchBoxDataProvider.onNativeLibraryReady(mTab);

        // Force the user to choose a search engine if they have to.
        final Callback<Boolean> onSearchEngineFinalizedCallback =
                (result) -> {
                    if (isActivityFinishingOrDestroyed()) return;

                    if (result == null || !result.booleanValue()) {
                        Log.e(TAG, "User failed to select a default search engine.");
                        finish(TerminationReason.FRE_NOT_COMPLETED);
                        return;
                    }

                    mHandler.post(this::finishDeferredInitialization);
                };
        getActivityDelegate()
                .showSearchEngineDialogIfNeeded(
                        SearchActivity.this, onSearchEngineFinalizedCallback);
    }

    // OverrideBackKeyBehaviorDelegate implementation.
    @Override
    public boolean handleBackKeyPressed() {
        finish(TerminationReason.BACK_KEY_PRESSED);
        return true;
    }

    @VisibleForTesting
    void finishDeferredInitialization() {
        assert !mIsActivityUsable
                : "finishDeferredInitialization() incorrectly called multiple times";
        mIsActivityUsable = true;
        if (mQueuedParams != null) {
            // SearchActivity does not support incognito operation.
            loadUrl(mQueuedParams, /* isIncognito= */ false);
        }

        // TODO(tedchoc): Warmup triggers the CustomTab layout to be inflated, but this widget
        //                will navigate to Tabbed mode.  Investigate whether this can inflate
        //                the tabbed mode layout in the background instead of CCTs.
        CustomTabsConnection.getInstance().warmup(0);
        mSearchBox.onDeferredStartup(mSearchType, getWindowAndroid());

        getActivityDelegate().onFinishDeferredInitialization();
    }

    @Override
    protected View getViewToBeDrawnBeforeInitializingNative() {
        return mSearchBox;
    }

    @Override
    public void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleNewIntent(intent, true);
    }

    @Override
    public void onPauseWithNative() {
        umaSessionEnd();
        RevenueStats.setCustomTabSearchClient(null);
        super.onPauseWithNative();
    }

    @Override
    public void onResumeWithNative() {
        // Start a new UMA session for the new activity.
        umaSessionResume();
        if (mIntentOrigin == IntentOrigin.CUSTOM_TAB
                && SEARCH_IN_CCT_APPLY_REFERRER_ID.getValue()) {
            var referrer = SearchActivityUtils.getReferrer(getIntent());
            var referrerValid = !TextUtils.isEmpty(referrer);
            RecordHistogram.recordBooleanHistogram(HISTOGRAM_INTENT_REFERRER_VALID, referrerValid);
            RevenueStats.setCustomTabSearchClient(
                    referrerValid ? CCT_CLIENT_PACKAGE_PREFIX + referrer : null);
        }

        // Inform the actity lifecycle observers. Among other things, the observers record
        // metrics pertaining to the "resumed" activity. This needs to happens after
        // umaSessionResume has closed the old UMA record, pertaining to the previous
        // (backgrounded) activity, and opened a new one pertaining to the "resumed" activity.
        super.onResumeWithNative();
    }

    /** Initiate new UMA session, associating metrics with appropriate Activity type. */
    private void umaSessionResume() {
        mUmaActivityObserver.startUmaSession(
                mIntentOrigin == IntentOrigin.CUSTOM_TAB
                        ? ActivityType.CUSTOM_TAB
                        : ActivityType.TABBED,
                null,
                getWindowAndroid());
    }

    /** Mark that the UMA session has ended. */
    private void umaSessionEnd() {
        mUmaActivityObserver.endUmaSession();
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    private void beginQuery() {
        var query = SearchActivityUtils.getIntentQuery(getIntent());

        RecordHistogram.recordBooleanHistogram(
                HISTOGRAM_LAUNCHED_WITH_QUERY, !TextUtils.isEmpty(query));

        mSearchBox.beginQuery(
                mIntentOrigin,
                mSearchType,
                SearchActivityUtils.getIntentQuery(getIntent()),
                getWindowAndroid());
    }

    @Override
    protected void onDestroy() {
        if (mTab != null && mTab.isInitialized()) mTab.destroy();
        if (mLocationBarCoordinator != null && mLocationBarCoordinator.getOmniboxStub() != null) {
            mLocationBarCoordinator.getOmniboxStub().removeUrlFocusChangeListener(this);
            mLocationBarCoordinator.destroy();
            mLocationBarCoordinator = null;
        }
        mHandler.removeCallbacksAndMessages(null);
        super.onDestroy();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) {
            mLocationBarCoordinator.setUrlFocusChangeInProgress(false);
        }
    }

    private void setHubSearchBoxUrlBarElements() {
        boolean isIncognito = mSearchBoxDataProvider.isIncognitoBranded();
        @StringRes
        int hintTextRes =
                isIncognito
                        ? R.string.hub_search_empty_hint_incognito
                        : R.string.hub_search_empty_hint;
        mLocationBarCoordinator.getUrlBarCoordinator().setUrlBarHintText(hintTextRes);
    }

    /* package */ boolean loadUrl(OmniboxLoadUrlParams params, boolean isIncognito) {
        recordNavigationTargetType(new GURL(params.url));

        if (mIntentOrigin == IntentOrigin.CUSTOM_TAB) {
            SearchActivityUtils.resolveOmniboxRequestForResult(this, params);
        } else {
            loadUrlInChromeBrowser(params);
        }

        finish(TerminationReason.NAVIGATION);
        return true;
    }

    private void loadUrlInChromeBrowser(@NonNull OmniboxLoadUrlParams params) {
        if (!mIsActivityUsable) {
            // Wait until native has loaded.
            mQueuedParams = params;
            return;
        }

        Intent intent = SearchActivityUtils.createIntentForStartActivity(this, params);
        if (intent == null) return;

        if (mIntentOrigin == IntentOrigin.SEARCH_WIDGET) {
            intent.putExtra(SearchWidgetProvider.EXTRA_FROM_SEARCH_WIDGET, true);
        }

        IntentUtils.safeStartActivity(
                this,
                intent,
                ActivityOptionsCompat.makeCustomAnimation(
                                this, android.R.anim.fade_in, android.R.anim.fade_out)
                        .toBundle());
        RecordUserAction.record("SearchWidget.SearchMade");
        LocaleManager.getInstance()
                .recordLocaleBasedSearchMetrics(true, params.url, params.transitionType);
    }

    private void setHubSearchBoxVisualElements() {
        mLocationBarCoordinator.getStatusCoordinator().setShowStatusView(false);
    }

    @VisibleForTesting
    /* package */ ViewGroup createContentView() {
        var contentView =
                (ViewGroup) getLayoutInflater().inflate(R.layout.search_activity, null, false);
        contentView.setOnClickListener(v -> finish(TerminationReason.TAP_OUTSIDE));
        return contentView;
    }

    /**
     * Terminate search session, invoking animations appropriate for the session type, and recording
     * session termination reason.
     *
     * <p>This method should be called instead of {@link finish()}.
     *
     * @param reason the reason session was terminated
     */
    @VisibleForTesting
    /* package */ void finish(@TerminationReason int reason) {
        if (isFinishing()) return;

        var exitAnimationRes = 0;
        if (mIntentOrigin != null && mIntentOrigin == IntentOrigin.CUSTOM_TAB) {
            if (reason != TerminationReason.NAVIGATION) {
                SearchActivityUtils.resolveOmniboxRequestForResult(this, null);
            }
            exitAnimationRes = android.R.anim.fade_out;
        } else {
            exitAnimationRes = R.anim.activity_close_exit;
        }

        recordEnumeratedHistogramWithIntentOriginBreakdown(
                HISTOGRAM_SESSION_TERMINATION_REASON, reason, TerminationReason.COUNT);

        super.finish();
        overridePendingTransition(0, exitAnimationRes);
    }

    @Override
    public void finish() {
        finish(TerminationReason.UNSPECIFIED);
    }

    @VisibleForTesting
    /* package */ static void recordUsage(@IntentOrigin int origin, @SearchType int searchType) {
        var name =
                switch (origin) {
                    case IntentOrigin.SEARCH_WIDGET -> USED_ANY_FROM_SEARCH_WIDGET;

                    case IntentOrigin.QUICK_ACTION_SEARCH_WIDGET -> switch (searchType) {
                        case SearchType.TEXT -> USED_TEXT_FROM_SHORTCUTS_WIDGET;
                        case SearchType.VOICE -> USED_VOICE_FROM_SHORTCUTS_WIDGET;
                        case SearchType.LENS -> USED_LENS_FROM_SHORTCUTS_WIDGET;
                        default -> null;
                    };

                        // Tracked by Custom Tabs.
                    case IntentOrigin.CUSTOM_TAB -> null;
                    default -> null;
                };

        if (name != null) RecordUserAction.record(name);
    }

    private static SearchActivityDelegate getActivityDelegate() {
        ThreadUtils.checkUiThread();
        if (sDelegate == null) sDelegate = new SearchActivityDelegate();
        return sDelegate;
    }

    /** See {@link #sDelegate}. */
    static void setDelegateForTests(SearchActivityDelegate delegate) {
        var oldValue = sDelegate;
        sDelegate = delegate;
        ResettersForTesting.register(() -> sDelegate = oldValue);
    }

    @VisibleForTesting
    void recordNavigationTargetType(@NonNull GURL url) {
        var templateSvc = TemplateUrlServiceFactory.getForProfile(mProfileSupplier.get());
        boolean isSearch =
                templateSvc != null
                        && templateSvc.isSearchResultsPageFromDefaultSearchProvider(url);
        boolean isNative =
                NativePage.isNativePageUrl(
                        url, /* incognito= */ false, /* hasPdfDownload= */ false);

        int targetType =
                isNative
                        ? NavigationTargetType.NATIVE_PAGE
                        : isSearch ? NavigationTargetType.SEARCH : NavigationTargetType.URL;

        recordEnumeratedHistogramWithIntentOriginBreakdown(
                HISTOGRAM_NAVIGATION_TARGET_TYPE, targetType, NavigationTargetType.COUNT);
    }

    /**
     * An extension of {@link RecordHistogram.recordEnumeratedHistogram} that captures the value
     * with an additional breakdown by {@link IntentOrigin}.
     *
     * <p>The break down may not be captured if the histogram is recorded ahead of origin being
     * identified (e.g. the activity was terminated before it was able to process the intent).
     *
     * @param histogramName the name of histogram to record
     * @param sample the sample value to record
     * @param max the maximum value the histogram can take
     */
    private void recordEnumeratedHistogramWithIntentOriginBreakdown(
            String histogramName, int sample, int max) {
        RecordHistogram.recordEnumeratedHistogram(histogramName, sample, max);

        if (mIntentOrigin != null) {
            String suffix =
                    switch (mIntentOrigin) {
                        case IntentOrigin.CUSTOM_TAB -> ".CustomTab";
                        case IntentOrigin.QUICK_ACTION_SEARCH_WIDGET -> ".ShortcutsWidget";
                        default -> ".SearchWidget";
                    };
            RecordHistogram.recordEnumeratedHistogram(histogramName + suffix, sample, max);
        }
    }

    /* package */ void setLocationBarCoordinatorForTesting(LocationBarCoordinator coordinator) {
        mLocationBarCoordinator = coordinator;
    }

    /* package */ LocationBarCoordinator getLocationBarCoordinatorForTesting() {
        return mLocationBarCoordinator;
    }

    /* package */ void setActivityUsableForTesting(boolean isUsable) {
        mIsActivityUsable = isUsable;
    }

    /* package */ SearchBoxDataProvider getSearchBoxDataProviderForTesting() {
        return mSearchBoxDataProvider;
    }

    /* package */ LocationBarEmbedderUiOverrides getEmbedderUiOverridesForTesting() {
        return mLocationBarUiOverrides;
    }

    /* package */ ObservableSupplier<Profile> getProfileSupplierForTesting() {
        return mProfileSupplier;
    }

    /* package */ void setLocationBarLayoutForTesting(SearchActivityLocationBarLayout layout) {
        mSearchBox = layout;
    }

    /* package */ void setUmaActivityObserverForTesting(UmaActivityObserver observer) {
        mUmaActivityObserver = observer;
    }

    @Override
    @SuppressWarnings("MissingSuperCall")
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        super_onTopResumedActivityChanged(isTopResumedActivity);

        // TODO(crbug.com/329702834): Ensure showing Suggestions when activity resumes.
        // This may only happen when user enters tab switcher, and immediately returns to the
        // SearchActivity.
        if (!isTopResumedActivity) {
            mSearchBox.clearOmniboxFocus();
        } else {
            mSearchBox.requestOmniboxFocus();
        }
    }

    @CheckDiscard("Isolated for testing; should be inlined by Proguard")
    /* package */ void super_onTopResumedActivityChanged(boolean isTopResumedActivity) {
        super.onTopResumedActivityChanged(isTopResumedActivity);
    }
}
