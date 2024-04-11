// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.app.ActivityOptionsCompat;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.init.SingleWindowKeyboardVisibilityDelegate;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.metrics.UmaActivityObserver;
import org.chromium.chrome.browser.omnibox.BackKeyBehaviorDelegate;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedderUiOverrides;
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.toolbar.VoiceToolbarButtonController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient.SearchType;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.widget.InsetObserver;
import org.chromium.components.browser_ui.widget.InsetObserverSupplier;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

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

    private static final Object DELEGATE_LOCK = new Object();

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
    private View mAnchorView;

    private SnackbarManager mSnackbarManager;
    private Tab mTab;
    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    protected final UnownedUserDataSupplier<InsetObserver> mInsetObserverViewSupplier =
            new InsetObserverSupplier();

    // SearchBoxDataProvider and LocationBarEmbedderUiOverrides are passed to several child
    // components upon construction. Ensure we don't accidentally introduce disconnection by
    // keeping only a single live instance here.
    private final SearchBoxDataProvider mSearchBoxDataProvider = new SearchBoxDataProvider();
    private final LocationBarEmbedderUiOverrides mLocationBarUiOverrides =
            new LocationBarEmbedderUiOverrides();
    private final UmaActivityObserver mUmaActivityObserver;

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
                new SingleWindowKeyboardVisibilityDelegate(new WeakReference(this)),
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
        mSearchBoxDataProvider.initialize(this);

        ViewGroup rootView = (ViewGroup) getWindow().getDecorView().getRootView();
        // Setting fitsSystemWindows to false ensures that the root view doesn't consume the
        // insets.
        rootView.setFitsSystemWindows(false);
        // Add an inset observer that stores the insets to access later.
        // WebContents needs the insets to determine the portion of the screen obscured by
        // non-content displaying things such as the OSK.
        mInsetObserverViewSupplier.attach(getWindowAndroid().getUnownedUserDataHost());
        mInsetObserverViewSupplier.set(new InsetObserver(rootView, true));

        var contentView = createContentView();
        setContentView(contentView);

        // Build the search box.
        mSearchBox =
                (SearchActivityLocationBarLayout)
                        contentView.findViewById(R.id.search_location_bar);
        mAnchorView = contentView.findViewById(R.id.toolbar);

        // Create status bar color controller and assign to search activity.
        if (OmniboxFeatures.shouldMatchToolbarAndStatusBarColor()) {
            // Update the status bar's color based on the toolbar color.
            Drawable anchorViewBackground = mAnchorView.getBackground();
            if (anchorViewBackground instanceof ColorDrawable) {
                int anchorViewColor = ((ColorDrawable) anchorViewBackground).getColor();
                StatusBarColorController.setStatusBarColor(this.getWindow(), anchorViewColor);
            }
        }

        BackPressManager backPressManager = new BackPressManager();
        getOnBackPressedDispatcher().addCallback(this, backPressManager.getCallback());

        mLocationBarCoordinator =
                new LocationBarCoordinator(
                        mSearchBox,
                        mAnchorView,
                        mProfileSupplier,
                        PrivacyPreferencesManagerImpl.getInstance(),
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
                                new SettingsLauncherImpl(),
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
                        ChromePureJavaExceptionReporter::reportJavaException,
                        backPressManager,
                        /* OmniboxSuggestionsDropdownScrollListener= */ null,
                        /* tabModelSelectorSupplier= */ null,
                        mLocationBarUiOverrides,
                        null);
        mLocationBarCoordinator.setUrlBarFocusable(true);
        mLocationBarCoordinator.setShouldShowMicButtonWhenUnfocused(true);
        mLocationBarCoordinator.getOmniboxStub().addUrlFocusChangeListener(this);

        // Kick off everything needed for the user to type into the box.
        handleNewIntent(getIntent());
        beginQuery();

        // Kick off loading of the native library.
        if (!getActivityDelegate().shouldDelayNativeInitialization()) {
            mHandler.post(this::startDelayedNativeInitialization);
        }

        onInitialLayoutInflationComplete();
    }

    @VisibleForTesting
    /* package */ void handleNewIntent(Intent intent) {
        mIntentOrigin = SearchActivityUtils.getIntentOrigin(intent);
        mSearchType = SearchActivityUtils.getIntentSearchType(intent);

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
    }

    /** Translate current intent origin and extras to a PageClassification. */
    @VisibleForTesting
    /* package */ void refinePageClassWithProfile(@NonNull Profile profile) {
        int pageClass = mSearchBoxDataProvider.getPageClassification(true, false);

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

        mSearchBoxDataProvider.onNativeLibraryReady(mTab);

        // Force the user to choose a search engine if they have to.
        final Callback<Boolean> onSearchEngineFinalizedCallback =
                (result) -> {
                    if (isActivityFinishingOrDestroyed()) return;

                    if (result == null || !result.booleanValue()) {
                        Log.e(TAG, "User failed to select a default search engine.");
                        finish();
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
        cancelSearch();
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
        VoiceRecognitionHandler voiceRecognitionHandler =
                mLocationBarCoordinator.getVoiceRecognitionHandler();
        mSearchBox.onDeferredStartup(mSearchType, voiceRecognitionHandler, getWindowAndroid());

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
        handleNewIntent(intent);
        beginQuery();
    }

    @Override
    public void onPause() {
        super.onPause();
        // Make sure that re-entering the SearchActivity from different widgets shows appropriate
        // suggestion types.
        mLocationBarCoordinator.clearOmniboxFocus();
    }

    @Override
    public void onResume() {
        super.onResume();
        mSearchBox.focusTextBox();
    }

    @Override
    public void onPauseWithNative() {
        umaSessionEnd();
        super.onPauseWithNative();
    }

    @Override
    public void onResumeWithNative() {
        // Start a new UMA session for the new activity.
        umaSessionResume();

        // Inform the actity lifecycle observers. Among other things, the observers record
        // metrics pertaining to the "resumed" activity. This needs to happens after
        // umaSessionResume has closed the old UMA record, pertaining to the previous
        // (backgrounded) activity, and opened a new one pertaining to the "resumed" activity.
        super.onResumeWithNative();
    }

    /** Mark that the UMA session has ended. */
    private void umaSessionResume() {
        mUmaActivityObserver.startUmaSession(ActivityType.TABBED, null, getWindowAndroid());
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
        mSearchBox.beginQuery(
                mIntentOrigin,
                mSearchType,
                SearchActivityUtils.getIntentQuery(getIntent()),
                mLocationBarCoordinator.getVoiceRecognitionHandler(),
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

    /* package */ boolean loadUrl(OmniboxLoadUrlParams params, boolean isIncognito) {
        var exitAnimationRes = 0;
        if (mIntentOrigin == IntentOrigin.CUSTOM_TAB) {
            SearchActivityUtils.resolveOmniboxRequestForResult(this, params);
            exitAnimationRes = android.R.anim.fade_out;
        } else {
            loadUrlInChromeBrowser(params);
        }

        finish();
        overridePendingTransition(0, exitAnimationRes);
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

    @VisibleForTesting
    /* package */ ViewGroup createContentView() {
        var contentView =
                (ViewGroup) getLayoutInflater().inflate(R.layout.search_activity, null, false);
        contentView.setOnClickListener(v -> cancelSearch());
        return contentView;
    }

    @VisibleForTesting
    /* package */ void cancelSearch() {
        var exitAnimationRes = 0;
        if (mIntentOrigin == IntentOrigin.CUSTOM_TAB) {
            SearchActivityUtils.resolveOmniboxRequestForResult(this, null);
            exitAnimationRes = android.R.anim.fade_out;
        } else {
            exitAnimationRes = R.anim.activity_close_exit;
        }
        finish();
        overridePendingTransition(0, exitAnimationRes);
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

    @Override
    @VisibleForTesting
    public final void startDelayedNativeInitialization() {
        super.startDelayedNativeInitialization();
    }

    private static SearchActivityDelegate getActivityDelegate() {
        synchronized (DELEGATE_LOCK) {
            if (sDelegate == null) sDelegate = new SearchActivityDelegate();
        }
        return sDelegate;
    }

    /** See {@link #sDelegate}. */
    static void setDelegateForTests(SearchActivityDelegate delegate) {
        var oldValue = sDelegate;
        sDelegate = delegate;
        ResettersForTesting.register(() -> sDelegate = oldValue);
    }

    public View getAnchorViewForTesting() {
        return mAnchorView;
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

    /* package */ ObservableSupplier getProfileSupplierForTesting() {
        return mProfileSupplier;
    }
}
