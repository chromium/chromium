// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.searchwidget;

import android.app.Activity;
import android.app.SearchManager;
import android.content.ComponentName;
import android.content.Intent;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.app.ActivityOptionsCompat;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulatorFactory;
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
import org.chromium.chrome.browser.omnibox.OmniboxFeatures;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionsDropdownScrollListener;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.chrome.browser.password_manager.ManagePasswordsReferrer;
import org.chromium.chrome.browser.password_manager.PasswordManagerLauncher;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBuilder;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.toolbar.VoiceToolbarButtonController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient.IntentOrigin;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient.SearchType;
import org.chromium.chrome.browser.ui.system.StatusBarColorController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.widget.InsetObserver;
import org.chromium.components.browser_ui.widget.InsetObserverSupplier;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.WindowDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;

/** Queries the user's default search engine and shows autocomplete suggestions. */
public class SearchActivity extends AsyncInitializationActivity
        implements SnackbarManageable,
                BackKeyBehaviorDelegate,
                UrlFocusChangeListener,
                OmniboxSuggestionsDropdownScrollListener {
    // Shared with other org.chromium.chrome.browser.searchwidget classes.
    protected static final String TAG = "searchwidget";

    public static final String EXTRA_FROM_SEARCH_ACTIVITY =
            "org.chromium.chrome.browser.searchwidget.FROM_SEARCH_ACTIVITY";

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

        /** Returning true causes the Activity to finish itself immediately when starting up. */
        boolean isActivityDisabledForTests() {
            return false;
        }
    }

    private static final Object DELEGATE_LOCK = new Object();

    /** Notified about events happening for the SearchActivity. */
    private static SearchActivityDelegate sDelegate;

    /** Main content view. */
    private ViewGroup mContentView;

    private View mAnchorView;

    // Incoming intent request type. See {@link SearchActivityUtils#IntentOrigin}.
    @IntentOrigin Integer mIntentOrigin;
    // Incoming intent search type. See {@link SearchActivityUtils#SearchType}.
    @SearchType Integer mSearchType;

    /** Whether the user is now allowed to perform searches. */
    private boolean mIsActivityUsable;

    /** Input submitted before before the native library was loaded. */
    private OmniboxLoadUrlParams mQueuedParams;

    /** The View that represents the search box. */
    private SearchActivityLocationBarLayout mSearchBox;

    LocationBarCoordinator mLocationBarCoordinator;

    private SnackbarManager mSnackbarManager;
    private Tab mTab;
    private ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    protected final UnownedUserDataSupplier<InsetObserver> mInsetObserverViewSupplier =
            new InsetObserverSupplier();

    // SearchBoxDataProvider is passed to several child components upon construction. Ensure we
    // don't accidentally introduce disconnection by keeping one live instance here.
    private final SearchBoxDataProvider mSearchBoxDataProvider = new SearchBoxDataProvider();
    private final UmaActivityObserver mUmaActivityObserver;

    public SearchActivity() {
        mUmaActivityObserver = new UmaActivityObserver(this);
    }

    @Override
    protected boolean isStartedUpCorrectly(Intent intent) {
        if (getActivityDelegate().isActivityDisabledForTests()) return false;
        return super.isStartedUpCorrectly(intent);
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

        mContentView = createContentView();
        setContentView(mContentView);

        // Build the search box.
        mSearchBox =
                (SearchActivityLocationBarLayout)
                        mContentView.findViewById(R.id.search_location_bar);
        mAnchorView = mContentView.findViewById(R.id.toolbar);
        updateAnchorViewLayout();

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
                        /* OmniboxSuggestionsDropdownScrollListener= */ this,
                        /* tabModelSelectorSupplier= */ null,
                        /* forcePhoneStyleOmnibox= */ true,
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

        switch (mIntentOrigin) {
            case IntentOrigin.CUSTOM_TAB:
                // TODO(crbug/327023983): Recognize SRP.
                mSearchBoxDataProvider.setPageClassification(PageClassification.OTHER_VALUE);
                break;

            case IntentOrigin.QUICK_ACTION_SEARCH_WIDGET:
                recordQuickActionSearchType(mSearchType);
                mSearchBoxDataProvider.setPageClassification(
                        PageClassification.ANDROID_SHORTCUTS_WIDGET_VALUE);
                break;

            case IntentOrigin.SEARCH_WIDGET:
            default:
                mSearchBoxDataProvider.setPageClassification(
                        PageClassification.ANDROID_SEARCH_WIDGET_VALUE);
                break;
        }

        mSearchBoxDataProvider.setCurrentUrl(SearchActivityUtils.getIntentUrl(intent));
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
        TabDelegateFactory factory =
                new TabDelegateFactory() {
                    @Override
                    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
                        return new TabWebContentsDelegateAndroid() {
                            @Override
                            public int getDisplayMode() {
                                return DisplayMode.BROWSER;
                            }

                            @Override
                            protected boolean shouldResumeRequestsForCreatedWindow() {
                                return false;
                            }

                            @Override
                            protected boolean addNewContents(
                                    WebContents sourceWebContents,
                                    WebContents webContents,
                                    int disposition,
                                    Rect initialPosition,
                                    boolean userGesture) {
                                return false;
                            }

                            @Override
                            protected void setOverlayMode(boolean useOverlayMode) {}

                            @Override
                            public boolean canShowAppBanners() {
                                return false;
                            }
                        };
                    }

                    @Override
                    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
                        return null;
                    }

                    @Override
                    public ContextMenuPopulatorFactory createContextMenuPopulatorFactory(Tab tab) {
                        return null;
                    }

                    @Override
                    public BrowserControlsVisibilityDelegate
                            createBrowserControlsVisibilityDelegate(Tab tab) {
                        return null;
                    }

                    @Override
                    public NativePage createNativePage(
                            String url, NativePage candidatePage, Tab tab, boolean isPdf) {
                        // SearchActivity does not create native pages.
                        return null;
                    }
                };

        WebContents webContents = WebContentsFactory.createWebContents(profile, false, false);
        mTab =
                new TabBuilder(profile)
                        .setWindow(getWindowAndroid())
                        .setLaunchType(TabLaunchType.FROM_EXTERNAL_APP)
                        .setWebContents(webContents)
                        .setDelegateFactory(factory)
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

    private void finishDeferredInitialization() {
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
        RecordUserAction.record("SearchWidget.WidgetSelected");

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

    private String getOptionalIntentQuery() {
        return IntentUtils.safeGetStringExtra(getIntent(), SearchManager.QUERY);
    }

    private void beginQuery() {
        mSearchBox.beginQuery(
                mSearchType,
                getOptionalIntentQuery(),
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
        finish();
        if (mIntentOrigin == IntentOrigin.CUSTOM_TAB) {
            SearchActivityUtils.resolveOmniboxRequestForResult(this, params);
            overridePendingTransition(0, android.R.anim.fade_out);
        } else {
            loadUrlInChromeBrowser(params);
        }
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

    private ViewGroup createContentView() {
        assert mContentView == null;

        ViewGroup contentView =
                (ViewGroup)
                        LayoutInflater.from(this).inflate(R.layout.search_activity, null, false);
        contentView.setOnClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        cancelSearch();
                    }
                });

        if (OmniboxFeatures.shouldShowModernizeVisualUpdate(this)) {
            View toolbarView = contentView.findViewById(R.id.toolbar);
            final int edgePadding = OmniboxResourceProvider.getToolbarSidePadding(this);
            toolbarView.setPaddingRelative(
                    edgePadding,
                    toolbarView.getPaddingTop(),
                    edgePadding,
                    toolbarView.getPaddingBottom());
            toolbarView.setBackground(
                    new ColorDrawable(
                            ChromeColors.getSurfaceColor(
                                    this, R.dimen.omnibox_suggestion_dropdown_bg_elevation)));
        }
        return contentView;
    }

    @VisibleForTesting
    /* package */ void cancelSearch() {
        finish();
        if (mIntentOrigin == IntentOrigin.CUSTOM_TAB) {
            SearchActivityUtils.resolveOmniboxRequestForResult(this, null);
            overridePendingTransition(0, android.R.anim.fade_out);
        } else {
            overridePendingTransition(0, R.anim.activity_close_exit);
        }
    }

    private static void recordQuickActionSearchType(@SearchType int searchType) {
        if (searchType == SearchType.VOICE) {
            RecordUserAction.record("QuickActionSearchWidget.VoiceQuery");
        } else if (searchType == SearchType.LENS) {
            RecordUserAction.record("QuickActionSearchWidget.LensQuery");
        } else if (searchType == SearchType.TEXT) {
            RecordUserAction.record("QuickActionSearchWidget.TextQuery");
        }
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

    LocationBarCoordinator getLocationBarCoordinatorForTesting() {
        return mLocationBarCoordinator;
    }

    /**
     * Increase the toolbar vertical height and bottom padding if the omnibox phase 2 feature is
     * enabled.
     */
    private void updateAnchorViewLayout() {
        if (!OmniboxFeatures.shouldShowModernizeVisualUpdate(mAnchorView.getContext())) {
            return;
        }

        var layoutParams = mAnchorView.getLayoutParams();
        int heightIncrease =
                getResources()
                        .getDimensionPixelSize(
                                OmniboxFeatures.shouldShowActiveColorOnOmnibox()
                                        ? R.dimen.toolbar_url_focus_height_increase_active_color
                                        : R.dimen
                                                .toolbar_url_focus_height_increase_no_active_color);
        layoutParams.height =
                getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                        + heightIncrease;
        mAnchorView.setLayoutParams(layoutParams);

        // Apply extra bottom padding for no active-color treatments.
        if (!OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
            int bottomPadding =
                    getResources().getDimensionPixelSize(R.dimen.toolbar_url_focus_bottom_padding);
            mAnchorView.setPaddingRelative(
                    mAnchorView.getPaddingStart(),
                    mAnchorView.getPaddingTop(),
                    mAnchorView.getPaddingEnd(),
                    bottomPadding);
        }
    }

    /** Apply the color to locationbar's and toolbar's background. */
    private void applyColor(@ColorInt int color) {
        if (!OmniboxFeatures.shouldShowModernizeVisualUpdate(SearchActivity.this)
                || OmniboxFeatures.shouldShowActiveColorOnOmnibox()) {
            return;
        }

        Drawable locationbarBackground =
                mContentView.findViewById(R.id.search_location_bar).getBackground();
        Drawable toolbarBackground = mContentView.findViewById(R.id.toolbar).getBackground();
        locationbarBackground.setTint(color);
        toolbarBackground.setTint(color);

        if (OmniboxFeatures.shouldMatchToolbarAndStatusBarColor()) {
            StatusBarColorController.setStatusBarColor(this.getWindow(), color);
        }
    }

    @Override
    public void onSuggestionDropdownScroll() {
        applyColor(
                ChromeColors.getSurfaceColor(
                        SearchActivity.this, R.dimen.toolbar_text_box_elevation));
    }

    @Override
    public void onSuggestionDropdownOverscrolledToTop() {
        applyColor(
                ChromeColors.getSurfaceColor(
                        SearchActivity.this, R.dimen.omnibox_suggestion_dropdown_bg_elevation));
    }

    /* package */ void setActivityUsableForTesting(boolean isUsable) {
        mIsActivityUsable = isUsable;
    }

    /* package */ SearchBoxDataProvider getSearchBoxDataProvider() {
        return mSearchBoxDataProvider;
    }
}
