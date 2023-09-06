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
import org.chromium.base.supplier.ObservableSupplier;
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
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchObserver;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.features.branding.BrandingController;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.CustomTabHeightStrategy;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBottomSheetStrategy;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabDisplayManager;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTabObserver;
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
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthCoordinatorFactory;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.page_insights.PageInsightsActivator;
import org.chromium.chrome.browser.page_insights.PageInsightsCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.reengagement.ReengagementNotificationController;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.RequestDesktopUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.util.TokenHolder;
import org.chromium.url.GURL;

import java.util.function.BooleanSupplier;

/**
 * A {@link RootUiCoordinator} variant that controls UI for {@link BaseCustomTabActivity}.
 */
public class BaseCustomTabRootUiCoordinator extends RootUiCoordinator {
    private final Supplier<CustomTabToolbarCoordinator> mToolbarCoordinator;
    private final Supplier<CustomTabActivityNavigationController> mNavigationController;
    private final Supplier<BrowserServicesIntentDataProvider> mIntentDataProvider;
    private final Supplier<CustomTabActivityTabController> mTabController;

    private CustomTabHeightStrategy mCustomTabHeightStrategy;

    // Created only when ChromeFeatureList.CctBrandTransparency is enabled.
    // TODO(https://crbug.com/1343056): Make it part of the ctor.
    private @Nullable BrandingController mBrandingController;

    private @Nullable DesktopSiteSettingsIPHController mDesktopSiteSettingsIPHController;

    private @Nullable PageInsightsCoordinator mPageInsightsCoordinator;
    private @Nullable ContextualSearchObserver mContextualSearchObserver;
    private int mPageInsightsToken;

    /**
     * Construct a new BaseCustomTabRootUiCoordinator.
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
     */
    public BaseCustomTabRootUiCoordinator(@NonNull AppCompatActivity activity,
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
            @ActivityType int activityType, @NonNull Supplier<Boolean> isInOverviewModeSupplier,
            @NonNull Supplier<Boolean> isWarmOnResumeSupplier,
            @NonNull AppMenuDelegate appMenuDelegate,
            @NonNull StatusBarColorProvider statusBarColorProvider,
            @NonNull IntentRequestTracker intentRequestTracker,
            @NonNull Supplier<CustomTabToolbarCoordinator> customTabToolbarCoordinator,
            @NonNull Supplier<CustomTabActivityNavigationController> customTabNavigationController,
            @NonNull Supplier<BrowserServicesIntentDataProvider> intentDataProvider,
            @NonNull Supplier<EphemeralTabCoordinator> ephemeralTabCoordinatorSupplier,
            @NonNull BackPressManager backPressManager,
            @NonNull Supplier<CustomTabActivityTabController> tabController) {
        // clang-format off
        super(activity, null, shareDelegateSupplier, tabProvider,
                profileSupplier, bookmarkModelSupplier, tabBookmarkerSupplier,
                contextualSearchManagerSupplier, tabModelSelectorSupplier,
                new OneshotSupplierImpl<>(), new OneshotSupplierImpl<>(),
                new OneshotSupplierImpl<>(), new OneshotSupplierImpl<>(), () -> null,
                browserControlsManager, windowAndroid,
                activityLifecycleDispatcher, layoutManagerSupplier, menuOrKeyboardActionController,
                activityThemeColorSupplier, modalDialogManagerSupplier, appMenuBlocker,
                supportsAppMenuSupplier, supportsFindInPage, tabCreatorManagerSupplier,
                fullscreenManager, compositorViewHolderSupplier, tabContentManagerSupplier,
                snackbarManagerSupplier, activityType,
                isInOverviewModeSupplier, isWarmOnResumeSupplier, appMenuDelegate,
                statusBarColorProvider, intentRequestTracker, new OneshotSupplierImpl<>(),
                ephemeralTabCoordinatorSupplier, false, backPressManager, null);
        // clang-format on
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
            mBrandingController = new BrandingController(
                    activity, appId, browserName, new ChromePureJavaExceptionReporter());
        }
        mTabController = tabController;
        mPageInsightsToken = TokenHolder.INVALID_TOKEN;
    }

    @Override
    protected void initializeToolbar() {
        super.initializeToolbar();

        mToolbarCoordinator.get().onToolbarInitialized(mToolbarManager);
        mNavigationController.get().onToolbarInitialized(mToolbarManager);

        CustomTabToolbar toolbar = mActivity.findViewById(R.id.toolbar);
        View coordinator = mActivity.findViewById(R.id.coordinator);
        mCustomTabHeightStrategy.onToolbarInitialized(
                coordinator, toolbar, mIntentDataProvider.get().getPartialTabToolbarCornerRadius());
        if (mBrandingController != null) {
            mBrandingController.onToolbarInitialized(toolbar.getBrandingDelegate());
        }
        toolbar.setCloseButtonPosition(mIntentDataProvider.get().getCloseButtonPosition());
        if (mIntentDataProvider.get().isPartialCustomTab()) {
            Callback<Runnable> softInputCallback;
            if (ChromeFeatureList.sCctResizableSideSheet.isEnabled()) {
                softInputCallback = ((
                        PartialCustomTabDisplayManager) mCustomTabHeightStrategy)::onShowSoftInput;
            } else {
                softInputCallback = ((PartialCustomTabBottomSheetStrategy)
                                mCustomTabHeightStrategy)::onShowSoftInput;
            }

            mTabController.get().registerTabObserver(
                    new PartialCustomTabTabObserver(softInputCallback));
            mTabController.get().registerTabObserver(new EmptyTabObserver() {
                @Override
                public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                    BaseCustomTabActivity baseActivity = (BaseCustomTabActivity) mActivity;
                    assert baseActivity != null;
                    baseActivity.getContextualSearchManagerSupplier()
                            .get()
                            .setCanHideAndroidBrowserControls(false);
                }
            });
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        super.onFinishNativeInitialization();

        maybeCreatePageInsightsComponent();
        if (isPageInsightsCapable(mIntentDataProvider.get())) {
            // Start sWAA checking handler while page insight-capable CCT is in use. We give some
            // delay in starting it to avoid the first call failing, which is observed sometimes.
            Tab tab = mTabModelSelectorSupplier.get().getCurrentTab();
            tab.addObserver(new EmptyTabObserver() {
                @Override
                public void onPageLoadFinished(Tab tab, GURL url) {
                    tab.removeObserver(this);
                    var activator = PageInsightsActivator.getForProfile(mProfileSupplier.get());
                    mPageInsightsToken = activator.start(() -> maybeCreatePageInsightsComponent());
                }
            });
        }

        if (!ReengagementNotificationController.isEnabled()) return;
        new OneShotCallback<>(mProfileSupplier, mCallbackController.makeCancelable(profile -> {
            assert profile != null : "Unexpectedly null profile from TabModel.";
            if (profile == null) return;
            Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
            ReengagementNotificationController controller = new ReengagementNotificationController(
                    mActivity, tracker, ReengagementActivity.class);
            controller.tryToReengageTheUser();
        }));
    }

    private void maybeCreatePageInsightsComponent() {
        if (!isPageInsightsHubEnabled() || mPageInsightsCoordinator != null) return;

        ViewStub containerStub = mActivity.findViewById(R.id.page_insights_hub_container_stub);
        if (containerStub != null) containerStub.inflate();
        var controller = BottomSheetControllerFactory.createFullWidthBottomSheetController(
                this::getScrimCoordinator,
                (v)
                        -> mPageInsightsCoordinator.initView(v),
                mActivity.getWindow(), mWindowAndroid.getKeyboardDelegate(),
                () -> mActivity.findViewById(R.id.page_insights_hub_container));

        mPageInsightsCoordinator = new PageInsightsCoordinator(mActivity, mActivityTabProvider,
                mShareDelegateSupplier, controller, getBottomSheetController(),
                mExpandedBottomSheetHelper, mBrowserControlsManager, mBrowserControlsManager,
                this::isPageInsightsHubEnabled);

        mContextualSearchObserver = new ContextualSearchObserver() {
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

    /**
     * Whether the CCT is meant to open page insights sheet. Dynamic conditions are checked
     * separately.
     */
    private static boolean isPageInsightsCapable(BrowserServicesIntentDataProvider intentData) {
        return PageInsightsCoordinator.isFeatureEnabled()
                && CustomTabsConnection.getInstance().shouldEnablePageInsightsForIntent(intentData);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static boolean isPageInsightsHubEnabledSync(
            BrowserServicesIntentDataProvider intentData, Supplier<Profile> profile) {
        return isPageInsightsCapable(intentData)
                && isChromeHistorySyncWithoutCustomPassphraseEnabled(profile);
    }

    private static boolean isChromeHistorySyncWithoutCustomPassphraseEnabled(
            Supplier<Profile> profile) {
        SyncService syncService = SyncServiceFactory.getForProfile(profile.get());
        return syncService != null && syncService.isSyncingUnencryptedUrls();
    }

    boolean isPageInsightsHubEnabled() {
        // TODO(b/286327847): Add UMA logging for failure cases.
        return isPageInsightsHubEnabledSync(mIntentDataProvider.get(), mProfileSupplier)
                && PageInsightsActivator.isSwaaEnabled();
    }

    public @Nullable PageInsightsCoordinator getPageInsightsCoordinator() {
        if (!isPageInsightsHubEnabled()) return null;
        if (mPageInsightsCoordinator == null) maybeCreatePageInsightsComponent();
        return mPageInsightsCoordinator;
    }

    @Override
    protected IncognitoReauthCoordinatorFactory getIncognitoReauthCoordinatorFactory() {
        Intent showRegularOverviewIntent = new Intent(Intent.ACTION_MAIN);
        showRegularOverviewIntent.setClass(mActivity, ChromeLauncherActivity.class);
        showRegularOverviewIntent.putExtra(IntentHandler.EXTRA_OPEN_REGULAR_OVERVIEW_MODE, true);
        IntentUtils.addTrustedIntentExtras(showRegularOverviewIntent);

        return new IncognitoReauthCoordinatorFactory(mActivity, mTabModelSelectorSupplier.get(),
                mModalDialogManagerSupplier.get(), new IncognitoReauthManager(),
                new SettingsLauncherImpl(),
                /*incognitoReauthTopToolbarDelegate= */ null,
                /*layoutManager=*/null,
                /*showRegularOverviewIntent= */ showRegularOverviewIntent,
                /*isTabbedActivity= */ false);
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
        assert intentDataProvider
                != null : "IntentDataProvider needs to be non-null after preInflationStartup";

        mCustomTabHeightStrategy = CustomTabHeightStrategy.createStrategy(mActivity,
                intentDataProvider.getInitialActivityHeight(),
                intentDataProvider.getInitialActivityWidth(),
                intentDataProvider.getActivityBreakPoint(),
                intentDataProvider.isPartialCustomTabFixedHeight(),
                CustomTabsConnection.getInstance(), intentDataProvider.getSession(),
                mActivityLifecycleDispatcher, mFullscreenManager,
                DeviceFormFactor.isWindowOnTablet(mWindowAndroid),
                intentDataProvider.canInteractWithBackground(),
                intentDataProvider.showSideSheetMaximizeButton(),
                intentDataProvider.getActivitySideSheetDecorationType(),
                intentDataProvider.getSideSheetPosition(),
                intentDataProvider.getSideSheetSlideInBehavior(),
                intentDataProvider.getActivitySideSheetRoundedCornersPosition());
    }

    @Override
    public void onPostInflationStartup() {
        super.onPostInflationStartup();
        mCustomTabHeightStrategy.onPostInflationStartup();
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
            return new Rect(location[0], location[1], location[0] + coord.getWidth(),
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

        mCustomTabHeightStrategy.destroy();

        if (mContextualSearchObserver != null && mContextualSearchManagerSupplier.get() != null) {
            mContextualSearchManagerSupplier.get().removeObserver(mContextualSearchObserver);
            mContextualSearchObserver = null;
        }

        if (isPageInsightsCapable(mIntentDataProvider.get())
                && mPageInsightsToken != TokenHolder.INVALID_TOKEN) {
            PageInsightsActivator.getForProfile(mProfileSupplier.get()).stop(mPageInsightsToken);
        }

        if (mPageInsightsCoordinator != null) {
            mPageInsightsCoordinator.destroy();
            mPageInsightsCoordinator = null;
        }
    }

    /**
     * Delegates changing the background color to the {@link CustomTabHeightStrategy}.
     * Returns {@code true} if any action were taken, {@code false} if not.
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

    /**
     * Runs a set of deferred startup tasks.
     */
    void onDeferredStartup() {
        new OneShotCallback<>(mProfileSupplier, mCallbackController.makeCancelable((profile) -> {
            Profile regularProfile = profile.getOriginalProfile();

            boolean didShowPrompt = RequestDesktopUtils.maybeShowDefaultEnableGlobalSettingMessage(
                    regularProfile, mMessageDispatcher, mActivity);
            if (!didShowPrompt && mAppMenuCoordinator != null) {
                mDesktopSiteSettingsIPHController = DesktopSiteSettingsIPHController.create(
                        mActivity, mWindowAndroid, mActivityTabProvider, regularProfile,
                        getToolbarManager().getMenuButtonView(),
                        mAppMenuCoordinator.getAppMenuHandler());
            }
        }));
    }

    CustomTabHeightStrategy getCustomTabSizeStrategyForTesting() {
        return mCustomTabHeightStrategy;
    }
}
