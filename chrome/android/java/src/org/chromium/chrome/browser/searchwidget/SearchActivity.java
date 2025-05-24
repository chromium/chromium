// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.net.Uri;
import android.provider.Browser;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.app.ActivityOptionsCompat;
import androidx.core.content.ContextCompat;

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
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.metrics.StartupMetricsTracker;
import org.chromium.chrome.browser.metrics.UmaActivityObserver;
import org.chromium.chrome.browser.omnibox.BackKeyBehaviorDelegate;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedderUiOverrides;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.CachedZeroSuggestionsManager;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.rlz.RevenueStats;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.VoiceToolbarButtonController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.ResolutionType;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.SearchType;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityWindowAndroid;
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
    /* package */ static final String USED_TEXT_FROM_HUB_WIDGET = "HubSearchWidget.Query";

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
    // LINT.IfChange(TerminationReason)
    @IntDef({
        TerminationReason.NAVIGATION,
        TerminationReason.UNSPECIFIED,
        TerminationReason.TAP_OUTSIDE,
        TerminationReason.BACK_KEY_PRESSED,
        TerminationReason.OMNIBOX_FOCUS_LOST,
        TerminationReason.ACTIVITY_FOCUS_LOST,
        TerminationReason.FRE_NOT_COMPLETED,
        TerminationReason.CUSTOM_BACK_ARROW,
        TerminationReason.BRING_TAB_TO_FRONT,
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
        int CUSTOM_BACK_ARROW = 7;
        int BRING_TAB_TO_FRONT = 8;
        int COUNT = 9;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/android/enums.xml:SearchActivityTerminationReason)

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

    private final StartupMetricsTracker mStartupMetricsTracker;
    private LocationBarCoordinator mLocationBarCoordinator;
    private SearchActivityLocationBarLayout mSearchBox;
    private View mAnchorView;

    private SnackbarManager mSnackbarManager;
    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<TabModelSelector> mTabModelSelectorSupplier =
            new ObservableSupplierImpl<>();

    // SearchBoxDataProvider and LocationBarEmbedderUiOverrides are passed to several child
    // components upon construction. Ensure we don't accidentally introduce disconnection by
    // keeping only a single live instance here.
    private final SearchBoxDataProvider mSearchBoxDataProvider = new SearchBoxDataProvider();
    private final LocationBarEmbedderUiOverrides mLocationBarUiOverrides =
            new LocationBarEmbedderUiOverrides();
    private UmaActivityObserver mUmaActivityObserver;

    public SearchActivity() {
        mUmaActivityObserver = new UmaActivityObserver(this);
        mStartupMetricsTracker = new StartupMetricsTracker(mTabModelSelectorSupplier);
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
                /* activityTopResumedSupported= */ false,
                getIntentRequestTracker(),
                getInsetObserver(),
                /* trackOcclusion= */ true) {
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
        boolean isIncognito = SearchActivityUtils.getIntentIncognitoStatus(getIntent());
        mSearchBoxDataProvider.initialize(this, isIncognito);

        ViewGroup rootView = (ViewGroup) getWindow().getDecorView().getRootView();
        // Setting fitsSystemWindows to false ensures that the root view doesn't consume the
        // insets.
        rootView.setFitsSystemWindows(false);

        var contentView = createContentView();
        setContentView(contentView);
        mStartupMetricsTracker.registerSearchActivityViewObserver(contentView);
        mSnackbarManager = new SnackbarManager(this, contentView, null);

        // Build the search box.
        mSearchBox = contentView.findViewById(R.id.search_location_bar);
        mAnchorView = contentView.findViewById(R.id.toolbar);

        // Update the status bar's color based on the toolbar color.
        setStatusAndNavBarColors();

        BackPressManager backPressManager = new BackPressManager();
        getOnBackPressedDispatcher().addCallback(this, backPressManager.getCallback());

        mLocationBarCoordinator =
                new LocationBarCoordinator(
                        mSearchBox,
                        mAnchorView,
                        mProfileSupplier,
                        mSearchBoxDataProvider,
                        null,
                        getWindowAndroid(),
                        /* activityTabSupplier= */ () -> null,
                        getModalDialogManagerSupplier(),
                        /* shareDelegateSupplier= */ null,
                        /* incognitoStateProvider= */ null,
                        getLifecycleDispatcher(),
                        this::loadUrl,
                        /* backKeyBehavior= */ this,
                        /* pageInfoAction= */ (tab, pageInfoHighlight) -> {},
                        this::bringTabToFront,
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
                        /* omniboxSuggestionsDropdownScrollListener= */ null,
                        /* tabModelSelectorSupplier= */ null,
                        mLocationBarUiOverrides,
                        findViewById(R.id.control_container),
                        /* bottomWindowPaddingSupplier */ () -> 0,
                        /* onLongClickListener= */ null,
                        /* browserControlsStateProvider= */ null,
                        /* isToolbarPositionCustomizationEnabled= */ false,
                        (context, tab, fromAppMenu) -> {});
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
        setIntent(intent);
        mIntentOrigin = SearchActivityUtils.getIntentOrigin(intent);
        mSearchType = SearchActivityUtils.getIntentSearchType(intent);

        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_INTENT_ORIGIN, mIntentOrigin, IntentOrigin.COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                HISTOGRAM_REQUESTED_SEARCH_TYPE, mSearchType, SearchType.COUNT);
        RecordHistogram.recordBooleanHistogram(HISTOGRAM_INTENT_ACTIVITY_PRESENT, activityPresent);

        recordUsage(mIntentOrigin, mSearchType);

        mSearchBoxDataProvider.setCurrentUrl(SearchActivityUtils.getIntentUrl(intent));

        if (OmniboxFeatures.sAndroidHubSearch.isEnabled()) {
            setColorScheme(mSearchBoxDataProvider.isIncognitoBranded());
        }

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

            case IntentOrigin.LAUNCHER:
                mLocationBarUiOverrides
                        .setLensEntrypointAllowed(true)
                        .setVoiceEntrypointAllowed(true);
                var jumpStartContext = CachedZeroSuggestionsManager.readJumpStartContext();
                mSearchBoxDataProvider.setCurrentUrl(jumpStartContext.url);
                mSearchBoxDataProvider.setPageClassification(jumpStartContext.pageClass);
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
        boolean isIncognito = SearchActivityUtils.getIntentIncognitoStatus(getIntent());
        ActivityProfileProvider profileProvider =
                new ActivityProfileProvider(getLifecycleDispatcher());
        profileProvider.onAvailable(
                (provider) -> {
                    mProfileSupplier.set(ProfileProvider.getOrCreateProfile(provider, isIncognito));
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

        if (OmniboxFeatures.sAndroidHubSearch.isEnabled() && mIntentOrigin == IntentOrigin.HUB) {
            setHubSearchBoxUrlBarElements();
        }

        // Force the user to choose a search engine if they have to.
        final Callback<Boolean> onSearchEngineFinalizedCallback =
                (result) -> {
                    if (isActivityFinishingOrDestroyed()) return;

                    if (result == null || !result.booleanValue()) {
                        Log.e(TAG, "User failed to select a default search engine.");
                        finish(TerminationReason.FRE_NOT_COMPLETED, /* loadUrlParams= */ null);
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
        finish(TerminationReason.BACK_KEY_PRESSED, /* loadUrlParams= */ null);
        return true;
    }

    @VisibleForTesting
    void finishDeferredInitialization() {
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
                && ChromeFeatureList.sSearchinCctApplyReferrerId.getValue()) {
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

        mSearchBox.beginQuery(mIntentOrigin, mSearchType, query, getWindowAndroid());
    }

    @Override
    protected void onDestroy() {
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
        mLocationBarCoordinator
                .getUrlBarCoordinator()
                .setUrlBarHintText(getResources().getString(hintTextRes));
    }

    /* package */ boolean loadUrl(@NonNull OmniboxLoadUrlParams params, boolean isIncognito) {
        finish(TerminationReason.NAVIGATION, params);
        return true;
    }

    private void openChromeBrowser(@Nullable OmniboxLoadUrlParams params) {
        Intent intent = SearchActivityUtils.createIntentForStartActivity(params);
        if (intent == null) return;

        if (mIntentOrigin == IntentOrigin.SEARCH_WIDGET) {
            intent.putExtra(SearchWidgetProvider.EXTRA_FROM_SEARCH_WIDGET, true);
        }

        if (OmniboxFeatures.sAndroidHubSearch.isEnabled()
                && mSearchBoxDataProvider.isIncognitoBranded()) {
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, getApplicationContext().getPackageName());
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
            IntentUtils.addTrustedIntentExtras(intent);
        }

        IntentUtils.safeStartActivity(
                this,
                intent,
                ActivityOptionsCompat.makeCustomAnimation(
                                this, android.R.anim.fade_in, android.R.anim.fade_out)
                        .toBundle());

        if (params != null) {
            RecordUserAction.record("SearchWidget.SearchMade");
            LocaleManager.getInstance()
                    .recordLocaleBasedSearchMetrics(true, params.url, params.transitionType);
        }
    }

    private void setHubSearchBoxVisualElements() {
        mLocationBarCoordinator
                .getStatusCoordinator()
                .setOnStatusIconNavigateBackButtonPress(
                        (View v) -> {
                            finish(TerminationReason.CUSTOM_BACK_ARROW, /* loadUrlParams= */ null);
                        });
    }

    // Set the color scheme of the search box background and anchor view background based on the
    // current incognito state. In the non incognito state, the color scheme is the same as what is
    // defined on initialize in {@link SearchActivityLocationBarLayout}.
    private void setColorScheme(boolean isIncognito) {
        @ColorRes int anchorViewBackgroundColorRes = R.color.omnibox_suggestion_dropdown_bg;
        GradientDrawable searchBoxBackground =
                (GradientDrawable) ((LayerDrawable) mSearchBox.getBackground()).getDrawable(0);

        if (isIncognito) {
            anchorViewBackgroundColorRes = R.color.omnibox_dropdown_bg_incognito;
            searchBoxBackground.setTintList(
                    AppCompatResources.getColorStateList(
                            this, R.color.toolbar_text_box_background_incognito));
        } else {
            searchBoxBackground.setTintList(null);
            searchBoxBackground.setTint(
                    ContextCompat.getColor(this, R.color.omnibox_suggestion_bg));
        }

        @ColorInt int anchorViewBackgroundColor = getColor(anchorViewBackgroundColorRes);
        GradientDrawable anchorViewBackground = (GradientDrawable) mAnchorView.getBackground();
        anchorViewBackground.setColor(anchorViewBackgroundColor);
        setStatusAndNavBarColors();
    }

    /**
     * Sets the status and nav bar colors to match the background color of mAnchorView.
     *
     * <p>Make sure that mAnchorView has the desired background color before you call this method.
     */
    private void setStatusAndNavBarColors() {
        Drawable anchorViewBackground = mAnchorView.getBackground();
        assert anchorViewBackground instanceof GradientDrawable
                : "Unsupported background drawable.";
        int anchorViewColor =
                ((GradientDrawable) anchorViewBackground).getColor().getDefaultColor();
        EdgeToEdgeSystemBarColorHelper helper =
                getEdgeToEdgeManager() != null
                        ? getEdgeToEdgeManager().getEdgeToEdgeSystemBarColorHelper()
                        : null;
        StatusBarColorController.setStatusBarColor(helper, getWindow(), anchorViewColor);
        if (helper != null) {
            helper.setNavigationBarColor(anchorViewColor);
        }
    }

    @VisibleForTesting
    /* package */ ViewGroup createContentView() {
        var contentView =
                (ViewGroup) getLayoutInflater().inflate(R.layout.search_activity, null, false);
        contentView.setOnClickListener(
                v -> finish(TerminationReason.TAP_OUTSIDE, /* loadUrlParams= */ null));
        return contentView;
    }

    /**
     * Terminate search session, invoking animations appropriate for the session type, and recording
     * session termination reason.
     *
     * <p>This method should be called instead of {@link finish()}.
     *
     * @param reason the reason session was terminated
     * @param loadUrlParams parameters specifying what page to load, when reason is NAVIGATION.
     */
    @VisibleForTesting
    /* package */ void finish(
            @TerminationReason int reason, @Nullable OmniboxLoadUrlParams loadUrlParams) {
        if (isFinishing()) return;

        if (loadUrlParams != null) {
            recordNavigationTargetType(new GURL(loadUrlParams.url));
        }

        var exitAnimationRes = 0;
        switch (SearchActivityUtils.getResolutionType(getIntent())) {
            case ResolutionType.SEND_TO_CALLER:
                // Return loadUrlParams to sender. Null value will report canceled action.
                SearchActivityUtils.resolveOmniboxRequestForResult(this, loadUrlParams);
                exitAnimationRes = android.R.anim.fade_out;
                break;

            case ResolutionType.OPEN_IN_CHROME:
                // Open Chrome and load page only when selection was made.
                if (loadUrlParams != null) openChromeBrowser(loadUrlParams);
                exitAnimationRes = R.anim.activity_close_exit;
                break;

            case ResolutionType.OPEN_OR_LAUNCH_CHROME:
                // Always open Chrome. When loadUrlParams is null, we will simply resume where the
                // user left off.
                openChromeBrowser(loadUrlParams);
                exitAnimationRes = android.R.anim.fade_out;
                break;
        }

        recordEnumeratedHistogramWithIntentOriginBreakdown(
                HISTOGRAM_SESSION_TERMINATION_REASON, reason, TerminationReason.COUNT);

        super.finish();
        overridePendingTransition(0, exitAnimationRes);
    }

    @Override
    public void finish() {
        finish(TerminationReason.UNSPECIFIED, /* loadUrlParams= */ null);
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
                    case IntentOrigin.HUB -> USED_TEXT_FROM_HUB_WIDGET;
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
                        url, /* isIncognito= */ false, /* hasPdfDownload= */ false);

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
                        case IntentOrigin.LAUNCHER -> ".Launcher";
                        case IntentOrigin.HUB -> ".Hub";
                        case IntentOrigin.WEB_SEARCH -> ".WebSearch";
                        default -> ".SearchWidget";
                    };
            RecordHistogram.recordEnumeratedHistogram(histogramName + suffix, sample, max);
        }
    }

    private void bringTabToFront(Tab tab) {
        finish(TerminationReason.BRING_TAB_TO_FRONT, /* loadUrlParams= */ null);
        IntentHandler.bringTabToFront(tab);
    }

    /* package */ void setLocationBarCoordinatorForTesting(LocationBarCoordinator coordinator) {
        mLocationBarCoordinator = coordinator;
    }

    /* package */ LocationBarCoordinator getLocationBarCoordinatorForTesting() {
        return mLocationBarCoordinator;
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

    /* package */ void setAnchorViewForTesting(View anchorView) {
        mAnchorView = anchorView;
    }

    @Override
    @SuppressWarnings("MissingSuperCall")
    public void onTopResumedActivityChanged(boolean isTopResumedActivity) {
        super_onTopResumedActivityChanged(isTopResumedActivity);

        // For hub search use in split screen and multi window mode, search activity should be
        // dismissed when focus is lost to prevent focus from causing the suggestion list to flicker
        // on window toggling.
        if (!isTopResumedActivity && mIntentOrigin == IntentOrigin.HUB) {
            finish(TerminationReason.ACTIVITY_FOCUS_LOST, null);
            return;
        }

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
