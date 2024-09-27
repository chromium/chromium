// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabProfileType.INCOGNITO;

import android.content.Intent;
import android.graphics.Rect;
import android.text.TextUtils;
import android.view.View;

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
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.reengagement.ReengagementActivity;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
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
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthCoordinatorFactory;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.page_info.ChromePageInfo;
import org.chromium.chrome.browser.page_info.ChromePageInfoHighlight;
import org.chromium.chrome.browser.privacy_sandbox.ActivityTypeMapper;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxDialogController;
import org.chromium.chrome.browser.privacy_sandbox.SurfaceType;
import org.chromium.chrome.browser.privacy_sandbox.TrackingProtectionSnackbarController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudIPHController;
import org.chromium.chrome.browser.reengagement.ReengagementNotificationController;
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
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeSupplier;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.content_public.browser.LoadUrlParams;
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
    private @Nullable GoogleBottomBarCoordinator mGoogleBottomBarCoordinator;
    private @Nullable TrackingProtectionSnackbarController mTrackingProtectionSnackbarController;

    private @Nullable EdgeToEdgeSupplier.ChangeObserver mEdgeToEdgeChangeObserver;

    /**
     * Construct a new BaseCustomTabRootUiCoordinator.
     *
     * @param activity The activity whose UI the coordinator is responsible for.
     * @param shareDelegateSupplier Supplies the {@link ShareDelegate}.
     * @param tabProvider The {@link ActivityTabProvider} to get current tab of the activity.
     * @param profileSupplier Supplier of the currently applicable profile.
     * @param bookmarkModelSupplier Supplier of the bookmark bridge for the current profile.
     * @param tabBookmarkerSupplier Supplier of {@link TabBookmarker} for bookmarking a given tab.
     * @param tabModelSelectorSupplier Supplies the {@link TabModelSelector}.
     * @param lastUserInteractionTimeSupplier Supplier of the last user interaction time.
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
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            @NonNull Supplier<Long> lastUserInteractionTimeSupplier,
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
            @NonNull AppMenuDelegate appMenuDelegate,
            @NonNull StatusBarColorProvider statusBarColorProvider,
            @NonNull IntentRequestTracker intentRequestTracker,
            @NonNull Supplier<CustomTabToolbarCoordinator> customTabToolbarCoordinator,
            @NonNull Supplier<CustomTabActivityNavigationController> customTabNavigationController,
            @NonNull Supplier<BrowserServicesIntentDataProvider> intentDataProvider,
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
                tabModelSelectorSupplier,
                new OneshotSupplierImpl<>(),
                new OneshotSupplierImpl<>(),
                new OneshotSupplierImpl<>(),
                new OneshotSupplierImpl<>(),
                lastUserInteractionTimeSupplier,
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
                appMenuDelegate,
                statusBarColorProvider,
                intentRequestTracker,
                new OneshotSupplierImpl<>(),
                false,
                backPressManager,
                null,
                /* overviewColorSupplier= */ null,
                baseChromeLayout);
        mToolbarCoordinator = customTabToolbarCoordinator;
        mNavigationController = customTabNavigationController;
        mIntentDataProvider = intentDataProvider;
        boolean isAuthTab = intentDataProvider.get().isAuthTab();
        if ((activityType == ActivityType.CUSTOM_TAB || isAuthTab)
                && !intentDataProvider.get().isOpenedByChrome()
                && intentDataProvider.get().getCustomTabMode() != INCOGNITO) {
            String appId = null;
            // AuthTab sets the ID to null by design to show the default branding (toast)
            // every time one is launched.
            if (!isAuthTab) {
                appId = mIntentDataProvider.get().getClientPackageName();
                if (TextUtils.isEmpty(appId)) {
                    appId = CustomTabIntentDataProvider.getAppIdFromReferrer(activity);
                }
            }
            String browserName = activity.getResources().getString(R.string.app_name);
            int toastTemplateId =
                    isAuthTab
                            ? R.string.auth_tab_secured_by_chrome_template
                            : R.string.twa_running_in_chrome_template;
            mBrandingController =
                    new BrandingController(
                            activity,
                            appId,
                            browserName,
                            toastTemplateId,
                            new ChromePureJavaExceptionReporter());
        }
        // TODO(353517557): Do initialization necessary for ActivityType.AUTH_TAB

        mTabController = tabController;
        mMinimizeDelegateSupplier = minimizeDelegateSupplier;
        mFeatureOverridesManagerSupplier = featureOverridesManagerSupplier;
        // TODO(crbug.com/41481778): move this RootUiCoordinator once this flag is removed.
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
        if (!MinimizedFeatureUtils.shouldEnableMinimizedCustomTabs(mIntentDataProvider.get())) {
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

        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        boolean shouldEnableOmnibox =
                connection.shouldEnableOmniboxForIntent(mIntentDataProvider.get());
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.Omnibox.EnabledState", shouldEnableOmnibox);
        if (shouldEnableOmnibox) {
            toolbar.setOmniboxEnabled(
                    mIntentDataProvider.get().getClientPackageName(),
                    connection.getAlternateOmniboxTapHandler(mIntentDataProvider.get()));
        }
    }

    @Override
    protected boolean canPreviewPromoteToTab() {
        return mActivityType == ActivityType.CUSTOM_TAB;
    }

    @Override
    public void onFinishNativeInitialization() {
        super.onFinishNativeInitialization();

        mGoogleBottomBarCoordinator = getGoogleBottomBarCoordinator();
        if (mGoogleBottomBarCoordinator != null) {
            mGoogleBottomBarCoordinator.onFinishNativeInitialization();
        }
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

        SupplierUtils.waitForAll(
                () -> initializeTrackingProtectionSnackbarController(),
                mActivityTabProvider,
                mProfileSupplier);
    }

    @Override
    protected void initProfileDependentFeatures(Profile currentlySelectedProfile) {
        super.initProfileDependentFeatures(currentlySelectedProfile);

        GoogleBottomBarCoordinator googleBottomBarCoordinator = getGoogleBottomBarCoordinator();

        if (googleBottomBarCoordinator != null) {
            googleBottomBarCoordinator.initDefaultSearchEngine(
                    currentlySelectedProfile.getOriginalProfile());
        }
    }

    private void initializeTrackingProtectionSnackbarController() {
        if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA_TRIGGER)
                && mActivityType == ActivityType.WEB_APK) {

            mTrackingProtectionSnackbarController =
                    new TrackingProtectionSnackbarController(
                            getPageInfoSnackbarOnAction(),
                            mSnackbarManagerSupplier,
                            mActivityTabProvider.get().getWebContents(),
                            mProfileSupplier.get(),
                            mActivityType);
        }
    }

    public CustomTabHistoryIPHController getHistoryIPHController() {
        return mCustomTabHistoryIPHController;
    }

    @Override
    public int getControlContainerHeightResource() {
        return R.dimen.custom_tabs_control_container_height;
    }

    @Override
    protected boolean isContextualSearchEnabled() {
        if (mIntentDataProvider.get().isAuthTab()) return false;
        return super.isContextualSearchEnabled();
    }

    @Override
    public void createContextualSearchTab(String searchUrl) {
        if (mActivityTabProvider.get() == null) return;
        mActivityTabProvider.get().loadUrl(new LoadUrlParams(searchUrl));
    }

    // Google Bottom bar
    private @Nullable GoogleBottomBarCoordinator maybeCreateGoogleBottomBarComponents() {
        if (!isGoogleBottomBarEnabled()) {
            return null;
        }

        return new GoogleBottomBarCoordinator(
                mActivity,
                mActivityTabProvider,
                mShareDelegateSupplier,
                CustomTabsConnection.getInstance()
                        .getGoogleBottomBarIntentParams(mIntentDataProvider.get()),
                mIntentDataProvider.get().getCustomButtonsOnGoogleBottomBar());
    }

    public @Nullable GoogleBottomBarCoordinator getGoogleBottomBarCoordinator() {
        if (mGoogleBottomBarCoordinator == null) {
            mGoogleBottomBarCoordinator = maybeCreateGoogleBottomBarComponents();
        }
        return mGoogleBottomBarCoordinator;
    }

    private boolean isGoogleBottomBarEnabled() {
        return isGoogleBottomBarEnabled(mIntentDataProvider.get());
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static boolean isGoogleBottomBarEnabled(BrowserServicesIntentDataProvider intentDataProvider) {
        return GoogleBottomBarCoordinator.isFeatureEnabled()
                && CustomTabsConnection.getInstance()
                        .shouldEnableGoogleBottomBarForIntent(intentDataProvider);
    }

    @Override
    protected IncognitoReauthCoordinatorFactory getIncognitoReauthCoordinatorFactory(
            Profile profile) {
        // TODO(crbug.com/335609494): Disable this for ephemeral CCTs.
        Intent showRegularOverviewIntent = new Intent(Intent.ACTION_MAIN);
        showRegularOverviewIntent.setClass(mActivity, ChromeLauncherActivity.class);
        showRegularOverviewIntent.putExtra(IntentHandler.EXTRA_OPEN_REGULAR_OVERVIEW_MODE, true);
        IntentUtils.addTrustedIntentExtras(showRegularOverviewIntent);

        return new IncognitoReauthCoordinatorFactory(
                mActivity,
                mTabModelSelectorSupplier.get(),
                mModalDialogManagerSupplier.get(),
                new IncognitoReauthManager(mActivity, profile),
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
        mCustomTabHistoryIPHController =
                CustomTabAppMenuHelper.maybeCreateHistoryIPHController(
                        mAppMenuCoordinator,
                        mActivity,
                        mActivityTabProvider,
                        mProfileSupplier,
                        mIntentDataProvider.get());
    }

    @Override
    protected boolean showWebSearchInActionMode() {
        return !mIntentDataProvider.get().isAuthTab();
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
        // TODO(crbug.com/40877078): Add a render test to prevent regressions.
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
    protected boolean supportsEdgeToEdge() {
        // Currently edge to edge only supports CCT media viewer.
        return EdgeToEdgeControllerFactory.isSupportedConfiguration(mActivity)
                && EdgeToEdgeUtils.isDrawKeyNativePageToEdgeEnabled()
                && !EdgeToEdgeUtils.DISABLE_CCT_MEDIA_VIEWER_E2E.getValue()
                && mIntentDataProvider.get() != null
                && mIntentDataProvider.get().shouldEnableEmbeddedMediaExperience();
    }

    @Override
    protected void initializeEdgeToEdgeController() {
        super.initializeEdgeToEdgeController();

        if (mEdgeToEdgeControllerSupplier.get() != null) {
            mEdgeToEdgeChangeObserver =
                    (int bottomInset, boolean isDrawingToEdge, boolean isPageOptInToEdge) -> {
                        CustomTabNavigationBarController.update(
                                mWindowAndroid.getWindow(),
                                mIntentDataProvider.get(),
                                mActivity,
                                isDrawingToEdge && isPageOptInToEdge);
                    };
            mEdgeToEdgeControllerSupplier.get().registerObserver(mEdgeToEdgeChangeObserver);
        }
    }

    @Override
    public void onDestroy() {
        if (mEdgeToEdgeControllerSupplier.get() != null) {
            mEdgeToEdgeControllerSupplier.get().unregisterObserver(mEdgeToEdgeChangeObserver);
            mEdgeToEdgeChangeObserver = null;
        }

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

    private boolean isAdsNoticeInCCTFeatureEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT)
                && (ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT,
                                "include-mode-b",
                                false)
                        || !ChromeFeatureList.isEnabled(
                                ChromeFeatureList.COOKIE_DEPRECATION_FACILITATED_TESTING));
    }

    /** Runs a set of deferred startup tasks. */
    void onDeferredStartup() {
        new OneShotCallback<>(
                mProfileSupplier,
                mCallbackController.makeCancelable(
                        (profile) -> {
                            Profile regularProfile = profile.getOriginalProfile();
                            Profile currentModelProfile =
                                    mTabModelSelectorSupplier.get().getCurrentModel().getProfile();

                            boolean didShowPrompt = false;
                            boolean shouldShowPrivacySandboxDialog =
                                    PrivacySandboxDialogController.shouldShowPrivacySandboxDialog(
                                            currentModelProfile, SurfaceType.AGACCT);
                            int activityType = mIntentDataProvider.get().getActivityType();
                            boolean isCustomTab =
                                    activityType == ActivityType.CUSTOM_TAB
                                            && !mIntentDataProvider.get().isPartialCustomTab();
                            if (isCustomTab) {
                                RecordHistogram.recordBooleanHistogram(
                                        "Startup.Android.PrivacySandbox.ShouldShowAdsNoticeCCT",
                                        shouldShowPrivacySandboxDialog);
                            }

                            if (isAdsNoticeInCCTFeatureEnabled()
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
                                                            currentModelProfile,
                                                            SurfaceType.AGACCT,
                                                            mWindowAndroid);
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

                            if (!didShowPrompt
                                    && ChromeFeatureList.isEnabled(
                                            ChromeFeatureList
                                                    .TRACKING_PROTECTION_USER_BYPASS_PWA_TRIGGER)
                                    && mActivityType == ActivityType.WEB_APK
                                    && mTrackingProtectionSnackbarController != null) {
                                mTrackingProtectionSnackbarController.maybeTriggerSnackbar();
                            }
                        }));
        SupplierUtils.waitForAll(
                () -> maybeRecordPrivacySandboxActivityType(),
                mIntentDataProvider,
                mProfileSupplier);
    }

    private void maybeRecordPrivacySandboxActivityType() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_ACTIVITY_TYPE_STORAGE)) {
            return;
        }

        int privacySandboxStorageActivityType =
                ActivityTypeMapper.toPrivacySandboxStorageActivityType(
                        mActivityType, mIntentDataProvider.get());

        PrivacySandboxBridge privacySandboxBridge =
                new PrivacySandboxBridge(mProfileSupplier.get());
        privacySandboxBridge.recordActivityType(privacySandboxStorageActivityType);
    }

    private Runnable getPageInfoSnackbarOnAction() {
        return () ->
                new ChromePageInfo(
                                mModalDialogManagerSupplier,
                                null,
                                OpenedFromSource.WEBAPK_SNACKBAR,
                                getMerchantTrustSignalsCoordinatorSupplier()::get,
                                getEphemeralTabCoordinatorSupplier(),
                                mTabCreatorManagerSupplier
                                        .get()
                                        .getTabCreator(mProfileSupplier.get().isOffTheRecord()))
                        .show(mActivityTabProvider.get(), ChromePageInfoHighlight.noHighlight());
    }

    CustomTabHeightStrategy getCustomTabSizeStrategyForTesting() {
        return mCustomTabHeightStrategy;
    }
}
