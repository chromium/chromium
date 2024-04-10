// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;
import android.graphics.Rect;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewStub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.reengagement.ReengagementActivity;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchObserver;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.features.branding.BrandingController;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizeDelegate;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.CustomTabHeightStrategy;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabDisplayManager;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTabObserver;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabHistoryIPHController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.desktop_site.DesktopSiteSettingsIPHController;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.gsa.GSAContextDisplaySelection;
import org.chromium.chrome.browser.history.HistoryManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthCoordinatorFactory;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.page_insights.PageInsightsConfigRequest;
import org.chromium.chrome.browser.page_insights.PageInsightsCoordinator;
import org.chromium.chrome.browser.page_insights.proto.Config.PageInsightsConfig;
import org.chromium.chrome.browser.page_insights.proto.IntentParams.PageInsightsIntentParams;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxDialogController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudIPHController;
import org.chromium.chrome.browser.reengagement.ReengagementNotificationController;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.RequestDesktopUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.BooleanSupplier;

/** A {@link RootUiCoordinator} variant that controls UI for {@link BaseCustomTabActivity}. */
public class BaseCustomTabRootUiCoordinator extends RootUiCoordinator {

    private final Supplier<CustomTabToolbarCoordinator> mToolbarCoordinator;
    private final Supplier<CustomTabActivityNavigationController> mNavigationController;
    private final Supplier<BrowserServicesIntentDataProvider> mIntentDataProvider;
    private final Supplier<CustomTabActivityTabController> mTabController;
    private final Supplier<CustomTabMinimizeDelegate> mMinimizeDelegateSupplier;
    private final Supplier<CustomTabFeatureOverridesManager> mFeatureOverridesManagerSupplier;

    private CustomTabHeightStrategy mCustomTabHeightStrategy;

    private @Nullable BrandingController mBrandingController;

    private @Nullable DesktopSiteSettingsIPHController mDesktopSiteSettingsIPHController;
    private @Nullable CustomTabHistoryIPHController mCustomTabHistoryIPHController;
    private @Nullable ReadAloudIPHController mReadAloudIPHController;

    private @Nullable PageInsightsCoordinator mPageInsightsCoordinator;
    private @Nullable ContextualSearchObserver mContextualSearchObserver;

    private @Nullable GoogleBottomBarCoordinator mGoogleBottomBarCoordinator;

    /**
     * Construct a new BaseCustomTabRootUiCoordinator.
     *
     * @param activity The activity whose UI the coordinator is responsible for.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate}.
     * @param tabProvider The {@link ActivityTabProvider} to get current tab of the activity.
     * @param profileSupplier Supplier of the currently applicable profile.
     * @param bookmarkModelSupplier Supplier of the bookmark bridge for the current profile.
     * @param tabBookmarkerSupplier Supplier of {@link TabBookmarker} for bookmarking a given tab.
     * @param contextualSearchManagerSupplier Supplier of the {@link ContextualSearchManager}.
     * @param tabModelSelectorSupplier Supplies the {@link TabModelSelector}.
     * @param browserControlsManager Manages the browser controls.
     * @param windowAndroid The current {@link WindowAndroid}.
     * @param activityLifecycleDispatcher Allows observation of the activity lifecycle.
     * @param layoutManagerSupplier Supplies the {@link LayoutManager}.
     * @param menuOrKeyboardActionController Controls the menu or keyboard action controller.
     * @param activityThemeColorSupplier Supplies the activity color theme.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param appMenuBlocker Controls the app menu blocking.
     * @param supportsAppMenuSupplier Supplies the support state for the app menu.
     * @param supportsFindInPage Supplies the support state for find in page.
     * @param tabCreatorManagerSupplier Supplies the {@link TabCreatorManager}.
     * @param fullscreenManager Manages the fullscreen state.
     * @param compositorViewHolderSupplier Supplies the {@link CompositorViewHolder}.
     * @param tabContentManagerSupplier Supplies the {@link TabContentManager}.
     * @param snackbarManagerSupplier Supplies the {@link SnackbarManager}.
     * @param activityType The {@link ActivityType} for the activity.
     * @param isInOverviewModeSupplier Supplies whether the app is in overview mode.
     * @param isWarmOnResumeSupplier Supplies whether the app was warm on resume.
     * @param appMenuDelegate The app menu delegate.
     * @param statusBarColorProvider Provides the status bar color.
     * @param intentRequestTracker Tracks intent requests.
     * @param customTabToolbarCoordinator Coordinates the custom tab toolbar.
     * @param customTabNavigationController Controls the custom tab navigation.
     * @param intentDataProvider Contains intent information used to start the Activity.
     * @param tabController Activity tab controller.
     * @param minimizeDelegateSupplier Supplies the {@link CustomTabMinimizeDelegate} used to
     *     minimize the tab.
     * @param featureOverridesManagerSupplier Supplies the {@link CustomTabFeatureOverridesManager}.
     * @param baseChromeLayout The base view hosting Chrome that certain views (e.g. the omnibox
     *     suggestion list) will position themselves relative to. If null, the content view will be
     *     used.
     */
    public BaseCustomTabRootUiCoordinator(
            @NonNull AppCompatActivity activity,
            @NonNull ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            @NonNull ActivityTabProvider tabProvider,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            @NonNull ObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
            @NonNull Supplier<ContextualSearchManager> contextualSearchManagerSupplier,
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull ActivityWindowAndroid windowAndroid,
            @NonNull ActivityLifecycleDispatcher activityLifecycleDispatcher,
            @NonNull ObservableSupplier<LayoutManagerImpl> layoutManagerSupplier,
            @NonNull MenuOrKeyboardActionController menuOrKeyboardActionController,
            @NonNull Supplier<Integer> activityThemeColorSupplier,
            @NonNull ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull AppMenuBlocker appMenuBlocker,
            @NonNull BooleanSupplier supportsAppMenuSupplier,
            @NonNull BooleanSupplier supportsFindInPage,
            @NonNull Supplier<TabCreatorManager> tabCreatorManagerSupplier,
            @NonNull FullscreenManager fullscreenManager,
            @NonNull ObservableSupplier<CompositorViewHolder> compositorViewHolderSupplier,
            @NonNull Supplier<TabContentManager> tabContentManagerSupplier,
            @NonNull Supplier<SnackbarManager> snackbarManagerSupplier,
            @NonNull ObservableSupplierImpl<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            @ActivityType int activityType,
            @NonNull Supplier<Boolean> isInOverviewModeSupplier,
            @NonNull Supplier<Boolean> isWarmOnResumeSupplier,
            @NonNull AppMenuDelegate appMenuDelegate,
            @NonNull StatusBarColorProvider statusBarColorProvider,
            @NonNull IntentRequestTracker intentRequestTracker,
            @NonNull Supplier<CustomTabToolbarCoordinator> customTabToolbarCoordinator,
            @NonNull Supplier<CustomTabActivityNavigationController> customTabNavigationController,
            @NonNull Supplier<BrowserServicesIntentDataProvider> intentDataProvider,
            @NonNull Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            @NonNull BackPressManager backPressManager,
            @NonNull Supplier<CustomTabActivityTabController> tabController,
            @NonNull Supplier<CustomTabMinimizeDelegate> minimizeDelegateSupplier,
            @NonNull Supplier<CustomTabFeatureOverridesManager> featureOverridesManagerSupplier,
            @Nullable View baseChromeLayout) {
        super(
                activity,
                null,
                shareDelegateSupplier,
                tabProvider,
                profileSupplier,
                bookmarkModelSupplier,
                tabBookmarkerSupplier,
                contextualSearchManagerSupplier,
                tabModelSelectorSupplier,
                new OneshotSupplierImpl<>(),
                new OneshotSupplierImpl<>(),
                new OneshotSupplierImpl<>(),
                new OneshotSupplierImpl<>(),
                new OneshotSupplierImpl<>(),
                () -> null,
                browserControlsManager,
                windowAndroid,
                activityLifecycleDispatcher,
                layoutManagerSupplier,
                menuOrKeyboardActionController,
                activityThemeColorSupplier,
                modalDialogManagerSupplier,
                appMenuBlocker,
                supportsAppMenuSupplier,
                supportsFindInPage,
                tabCreatorManagerSupplier,
                fullscreenManager,
                compositorViewHolderSupplier,
                tabContentManagerSupplier,
                snackbarManagerSupplier,
                edgeToEdgeControllerSupplier,
                activityType,
                isInOverviewModeSupplier,
                isWarmOnResumeSupplier,
                appMenuDelegate,
                statusBarColorProvider,
                intentRequestTracker,
                new OneshotSupplierImpl<>(),
                ephemeralTabCoordinatorSupplier,
                false,
                backPressManager,
                null,
                /* overviewColorSupplier= */ null,
                baseChromeLayout);
        mToolbarCoordinator = customTabToolbarCoordinator;
        mNavigationController = customTabNavigationController;
        mIntentDataProvider = intentDataProvider;
        if (intentDataProvider.get().getActivityType() == ActivityType.CUSTOM_TAB
                && !intentDataProvider.get().isOpenedByChrome()
                && !intentDataProvider.get().isIncognito()) {
            String appId = mIntentDataProvider.get().getClientPackageName();
            if (TextUtils.isEmpty(appId)) {
                appId = CustomTabIntentDataProvider.getAppIdFromReferrer(activity);
            }
            String browserName = activity.getResources().getString(R.string.app_name);
            mBrandingController =
                    new BrandingController(
                            activity, appId, browserName, new ChromePureJavaExceptionReporter());
        }
        mTabController = tabController;
        mMinimizeDelegateSupplier = minimizeDelegateSupplier;
        mFeatureOverridesManagerSupplier = featureOverridesManagerSupplier;
        // TODO(https://crbug.com/1509163): move this RootUiCoordinator once this flag is removed.
        if (ChromeFeatureList.sCctTabModalDialog.isEnabled()) {
            getAppBrowserControlsVisibilityDelegate()
                    .addDelegate(browserControlsManager.getBrowserVisibilityDelegate());
        }
    }

    @Override
    protected void initializeToolbar() {
        super.initializeToolbar();

        mToolbarCoordinator.get().onToolbarInitialized(mToolbarManager);
        mNavigationController.get().onToolbarInitialized(mToolbarManager);

        CustomTabToolbar toolbar = mActivity.findViewById(R.id.toolbar);
        if (ChromeFeatureList.sCctIntentFeatureOverrides.isEnabled()) {
            toolbar.setFeatureOverridesManager(mFeatureOverridesManagerSupplier.get());
        }
        View coordinator = mActivity.findViewById(R.id.coordinator);
        mCustomTabHeightStrategy.onToolbarInitialized(
                coordinator, toolbar, mIntentDataProvider.get().getPartialTabToolbarCornerRadius());
        if (mBrandingController != null) {
            mBrandingController.onToolbarInitialized(toolbar.getBrandingDelegate());
        }
        toolbar.setCloseButtonPosition(mIntentDataProvider.get().getCloseButtonPosition());
        if (mMinimizeDelegateSupplier.hasValue()) {
            toolbar.setMinimizeDelegate(mMinimizeDelegateSupplier.get());
        }
        if (MinimizedFeatureUtils.isWebApp(mIntentDataProvider.get())) {
            toolbar.setMinimizeButtonEnabled(false);
        }
        if (mIntentDataProvider.get().isPartialCustomTab()) {
            Callback<Runnable> softInputCallback =
                    ((PartialCustomTabDisplayManager) mCustomTabHeightStrategy)::onShowSoftInput;

            var tabController = mTabController.get();
            tabController.registerTabObserver(new PartialCustomTabTabObserver(softInputCallback));
            var csManager = mContextualSearchManagerSupplier.get();
            if (csManager != null) {
                tabController.registerTabObserver(
                        new EmptyTabObserver() {
                            @Override
                            public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                                csManager.setCanHideAndroidBrowserControls(false);
                            }
                        });
            }
        }

        if (CustomTabsConnection.getInstance()
                .shouldEnableOmniboxForIntent(mIntentDataProvider.get())) {
            toolbar.setOmniboxEnabled();
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        super.onFinishNativeInitialization();

        maybeCreatePageInsightsComponent();

        new OneShotCallback<>(
                mProfileSupplier,
                mCallbackController.makeCancelable(
                        profile -> {
                            assert profile != null : "Unexpectedly null profile from TabModel.";
                            if (profile == null) return;
                            if (ReengagementNotificationController.isEnabled()) {
                                Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
                                ReengagementNotificationController controller =
                                        new ReengagementNotificationController(
                                                mActivity, tracker, ReengagementActivity.class);
                                controller.tryToReengageTheUser();
                            }
                            if (mAppMenuCoordinator != null) {
                                mReadAloudIPHController =
                                        new ReadAloudIPHController(
                                                mActivity,
                                                profile,
                                                getToolbarManager().getMenuButtonView(),
                                                mAppMenuCoordinator.getAppMenuHandler(),
                                                mActivityTabProvider,
                                                mReadAloudControllerSupplier,
                                                /* showAppMenuTextBubble= */ false);
                            }
                        }));
    }

    @Override
    public @Nullable ManagedBottomSheetController getPageInsightsBottomSheetController() {
        PageInsightsCoordinator coordinator = getPageInsightsCoordinator();
        if (coordinator == null) {
            return null;
        }
        return coordinator.getBottomSheetController();
    }

    public CustomTabHistoryIPHController getHistoryIPHController() {
        return mCustomTabHistoryIPHController;
    }

    private void maybeCreatePageInsightsComponent() {
        if (!isPageInsightsHubEnabled() || mPageInsightsCoordinator != null) return;

        ViewStub containerStub = mActivity.findViewById(R.id.page_insights_hub_container_stub);
        if (containerStub != null) containerStub.inflate();
        var controller =
                BottomSheetControllerFactory.createFullWidthBottomSheetController(
                        this::getScrimCoordinator,
                        (v) -> mPageInsightsCoordinator.initView(v),
                        mActivity.getWindow(),
                        mWindowAndroid.getKeyboardDelegate(),
                        () -> mActivity.findViewById(R.id.page_insights_hub_container));

        mPageInsightsCoordinator =
                new PageInsightsCoordinator(
                        mActivity,
                        mActivity.getWindow().getDecorView(),
                        mActivityTabProvider,
                        mShareDelegateSupplier,
                        mProfileSupplier,
                        controller,
                        getBottomSheetController(),
                        mExpandedBottomSheetHelper,
                        mBrowserControlsManager,
                        mBrowserControlsManager,
                        mBackPressManager,
                        mCompositorViewHolderSupplier.get() == null
                                ? null
                                : mCompositorViewHolderSupplier.get().getInMotionSupplier(),
                        mWindowAndroid.getApplicationBottomInsetSupplier(),
                        getPageInsightsIntentParams(),
                        this::isPageInsightsHubEnabled,
                        this::getPageInsightsConfig);

        if (mContextualSearchManagerSupplier.get() != null) {
            mContextualSearchObserver =
                    new ContextualSearchObserver() {
                        @Override
                        public void onShowContextualSearch(
                                @Nullable GSAContextDisplaySelection selectionContext) {
                            mPageInsightsCoordinator.onBottomUiStateChanged(true);
                        }

                        @Override
                        public void onHideContextualSearch() {
                            mPageInsightsCoordinator.onBottomUiStateChanged(false);
                        }
                    };
            mContextualSearchManagerSupplier.get().addObserver(mContextualSearchObserver);
        }
    }

    boolean isPageInsightsHubEnabled() {
        return isPageInsightsHubEnabled(mIntentDataProvider.get());
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static boolean isPageInsightsHubEnabled(BrowserServicesIntentDataProvider intentDataProvider) {
        // TODO(b/286327847): Add UMA logging for failure cases.
        return PageInsightsCoordinator.isFeatureEnabled()
                && CustomTabsConnection.getInstance()
                        .shouldEnablePageInsightsForIntent(intentDataProvider);
    }

    private PageInsightsIntentParams getPageInsightsIntentParams() {
        return CustomTabsConnection.getInstance()
                .getPageInsightsIntentParams(mIntentDataProvider.get());
    }

    private PageInsightsConfig getPageInsightsConfig(PageInsightsConfigRequest request) {
        return CustomTabsConnection.getInstance()
                .getPageInsightsConfig(request, mIntentDataProvider.get(), mProfileSupplier);
    }

    public @Nullable PageInsightsCoordinator getPageInsightsCoordinator() {
        maybeCreatePageInsightsComponent();
        return mPageInsightsCoordinator;
    }

    // Google Bottom bar
    private @Nullable GoogleBottomBarCoordinator maybeCreateGoogleBottomBarComponents(
            BrowserServicesIntentDataProvider intentDataProvider) {
        if (!isGoogleBottomBarEnabled(intentDataProvider)) {
            return null;
        }

        return new GoogleBottomBarCoordinator(
                mActivity,
                CustomTabsConnection.getInstance()
                        .getGoogleBottomBarIntentParams(intentDataProvider),
                intentDataProvider.getCustomButtonsOnGoogleBottomBar());
    }

    public @Nullable GoogleBottomBarCoordinator getGoogleBottomBarCoordinator() {
        if (mGoogleBottomBarCoordinator == null) {
            mGoogleBottomBarCoordinator =
                    maybeCreateGoogleBottomBarComponents(mIntentDataProvider.get());
        }
        return mGoogleBottomBarCoordinator;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static boolean isGoogleBottomBarEnabled(BrowserServicesIntentDataProvider intentDataProvider) {
        return GoogleBottomBarCoordinator.isFeatureEnabled()
                && CustomTabsConnection.getInstance()
                        .shouldEnableGoogleBottomBarForIntent(intentDataProvider);
    }

    @Override
    protected IncognitoReauthCoordinatorFactory getIncognitoReauthCoordinatorFactory() {
        Intent showRegularOverviewIntent = new Intent(Intent.ACTION_MAIN);
        showRegularOverviewIntent.setClass(mActivity, ChromeLauncherActivity.class);
        showRegularOverviewIntent.putExtra(IntentHandler.EXTRA_OPEN_REGULAR_OVERVIEW_MODE, true);
        IntentUtils.addTrustedIntentExtras(showRegularOverviewIntent);

        return new IncognitoReauthCoordinatorFactory(
                mActivity,
                mTabModelSelectorSupplier.get(),
                mModalDialogManagerSupplier.get(),
                new IncognitoReauthManager(),
                new SettingsLauncherImpl(),
                /* incognitoReauthTopToolbarDelegate= */ null,
                /* layoutManager= */ null,
                /* hubManagerSupplier= */ null,
                /* showRegularOverviewIntent= */ showRegularOverviewIntent,
                /* isTabbedActivity= */ false);
    }

    @Override
    protected boolean shouldAllowThemingInNightMode() {
        return mActivityType == ActivityType.TRUSTED_WEB_ACTIVITY
                || mActivityType == ActivityType.WEB_APK;
    }

    @Override
    protected boolean shouldAllowBrightThemeColors() {
        return mActivityType == ActivityType.TRUSTED_WEB_ACTIVITY
                || mActivityType == ActivityType.WEB_APK;
    }

    @Override
    public void onPreInflationStartup() {
        super.onPreInflationStartup();

        BrowserServicesIntentDataProvider intentDataProvider = mIntentDataProvider.get();
        assert intentDataProvider != null
                : "IntentDataProvider needs to be non-null after preInflationStartup";

        mCustomTabHeightStrategy =
                CustomTabHeightStrategy.createStrategy(
                        mActivity,
                        intentDataProvider,
                        () -> mCompositorViewHolderSupplier.get(),
                        () -> mTabModelSelectorSupplier.get().getCurrentTab(),
                        CustomTabsConnection.getInstance(),
                        mActivityLifecycleDispatcher,
                        mFullscreenManager,
                        DeviceFormFactor.isWindowOnTablet(mWindowAndroid));
    }

    @Override
    public void onPostInflationStartup() {
        super.onPostInflationStartup();
        mCustomTabHeightStrategy.onPostInflationStartup();
        if (HistoryManager.isAppSpecificHistoryEnabled() && mAppMenuCoordinator != null) {
            mCustomTabHistoryIPHController =
                    new CustomTabHistoryIPHController(
                            mActivity,
                            mActivityTabProvider,
                            mProfileSupplier,
                            mAppMenuCoordinator.getAppMenuHandler());
        }
    }

    @Override
    protected void setStatusBarScrimFraction(float scrimFraction) {
        super.setStatusBarScrimFraction(scrimFraction);
        // TODO(jinsukkim): Separate CCT scrim update action from status bar scrim stuff.
        mCustomTabHeightStrategy.setScrimFraction(scrimFraction);
    }

    @Override
    protected Rect getAppRectOnScreen() {
        // This is necessary if app handler cannot rely on the popup window that ensures the menu
        // will not be clipped off the screen, which can happen in partial CCT.
        // TODO(crbug.com/1382010): Add a render test to prevent regressions.
        if (mIntentDataProvider.get().isPartialCustomTab()) {
            View coord = mActivity.findViewById(R.id.coordinator);
            int[] location = new int[2];
            coord.getLocationOnScreen(location);
            return new Rect(
                    location[0],
                    location[1],
                    location[0] + coord.getWidth(),
                    location[1] + coord.getHeight());
        }
        return super.getAppRectOnScreen();
    }

    @Override
    protected void onFindToolbarShown() {
        super.onFindToolbarShown();
        CustomTabToolbar toolbar = mActivity.findViewById(R.id.toolbar);
        toolbar.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
        mCustomTabHeightStrategy.onFindToolbarShown();
    }

    @Override
    protected void onFindToolbarHidden() {
        super.onFindToolbarHidden();
        CustomTabToolbar toolbar = mActivity.findViewById(R.id.toolbar);
        toolbar.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_YES);
        mCustomTabHeightStrategy.onFindToolbarHidden();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (mBrandingController != null) {
            mBrandingController.destroy();
            mBrandingController = null;
        }

        if (mDesktopSiteSettingsIPHController != null) {
            mDesktopSiteSettingsIPHController.destroy();
            mDesktopSiteSettingsIPHController = null;
        }

        if (mReadAloudIPHController != null) {
            mReadAloudIPHController.destroy();
            mReadAloudIPHController = null;
        }

        mCustomTabHeightStrategy.destroy();

        if (mContextualSearchObserver != null && mContextualSearchManagerSupplier.get() != null) {
            mContextualSearchManagerSupplier.get().removeObserver(mContextualSearchObserver);
            mContextualSearchObserver = null;
        }

        if (mPageInsightsCoordinator != null) {
            mPageInsightsCoordinator.destroy();
            mPageInsightsCoordinator = null;
        }

        if (mCustomTabHistoryIPHController != null) {
            mCustomTabHistoryIPHController.destroy();
            mCustomTabHistoryIPHController = null;
        }
    }

    /**
     * Delegates changing the background color to the {@link CustomTabHeightStrategy}. Returns
     * {@code true} if any action were taken, {@code false} if not.
     */
    public boolean changeBackgroundColorForResizing() {
        if (mCustomTabHeightStrategy == null) return false;

        return mCustomTabHeightStrategy.changeBackgroundColorForResizing();
    }

    /**
     * Perform slide-down animation on closing.
     * @param finishRunnable Runnable finishing the activity after the animation.
     */
    void handleCloseAnimation(Runnable finishRunnable) {
        mCustomTabHeightStrategy.handleCloseAnimation(finishRunnable);
    }

    /** Runs a set of deferred startup tasks. */
    void onDeferredStartup() {
        new OneShotCallback<>(
                mProfileSupplier,
                mCallbackController.makeCancelable(
                        (profile) -> {
                            Profile regularProfile = profile.getOriginalProfile();
                            boolean didShowPrompt = false;
                            boolean shouldShowPrivacySandboxDialog =
                                    PrivacySandboxDialogController.shouldShowPrivacySandboxDialog(
                                            mTabModelSelectorSupplier.get().isIncognitoSelected());
                            boolean isCustomTab =
                                    mIntentDataProvider.get().getActivityType()
                                                    == ActivityType.CUSTOM_TAB
                                            && !(mIntentDataProvider.get().isPartialCustomTab());
                            if (isCustomTab) {
                                RecordHistogram.recordBooleanHistogram(
                                        "Startup.Android.PrivacySandbox.ShouldShowAdsNoticeCCT",
                                        shouldShowPrivacySandboxDialog);
                            }
                            if (ChromeFeatureList.isEnabled(
                                            ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT)
                                    && shouldShowPrivacySandboxDialog
                                    && isCustomTab) {
                                boolean shouldShowPrivacySandboxDialogAppIdCheck = true;
                                String appId = mIntentDataProvider.get().getClientPackageName();
                                String paramAdsNoticeAppId =
                                        ChromeFeatureList.getFieldTrialParamByFeature(
                                                ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
                                                "app-id");
                                if (!paramAdsNoticeAppId.isEmpty()
                                        && !paramAdsNoticeAppId.equals(appId)) {
                                    shouldShowPrivacySandboxDialogAppIdCheck = false;
                                }
                                RecordHistogram.recordBooleanHistogram(
                                        "Startup.Android.PrivacySandbox.AdsNoticeCCTAppIDCheck",
                                        shouldShowPrivacySandboxDialogAppIdCheck);
                                if (shouldShowPrivacySandboxDialogAppIdCheck) {
                                    didShowPrompt =
                                            PrivacySandboxDialogController
                                                    .maybeLaunchPrivacySandboxDialog(
                                                            mActivity,
                                                            new SettingsLauncherImpl(),
                                                            mTabModelSelectorSupplier
                                                                    .get()
                                                                    .isIncognitoSelected());
                                }
                            }
                            if (!didShowPrompt) {
                                didShowPrompt =
                                        RequestDesktopUtils
                                                .maybeShowDefaultEnableGlobalSettingMessage(
                                                        regularProfile,
                                                        mMessageDispatcher,
                                                        mActivity);
                            }
                            if (!didShowPrompt && mAppMenuCoordinator != null) {
                                mDesktopSiteSettingsIPHController =
                                        DesktopSiteSettingsIPHController.create(
                                                mActivity,
                                                mWindowAndroid,
                                                mActivityTabProvider,
                                                regularProfile,
                                                getToolbarManager().getMenuButtonView(),
                                                mAppMenuCoordinator.getAppMenuHandler());
                            }
                        }));
    }

    CustomTabHeightStrategy getCustomTabSizeStrategyForTesting() {
        return mCustomTabHeightStrategy;
    }
}
