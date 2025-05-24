// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabProfileType.INCOGNITO;

import android.content.Intent;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.browser.customtabs.ExperimentalOpenInBrowser;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.IntentUtils;
import org.chromium.base.TimeUtils;
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
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.chrome.browser.customtabs.features.branding.BrandingController;
import org.chromium.chrome.browser.customtabs.features.branding.MismatchNotificationChecker;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizeDelegate;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.CustomTabHeightStrategy;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabDisplayManager;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabTabObserver;
import org.chromium.chrome.browser.customtabs.features.toolbar.BrowserServicesThemeColorProvider;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabHistoryIphController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarButtonsCoordinator;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.desktop_site.DesktopSiteSettingsIphController;
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
import org.chromium.chrome.browser.pdf.PdfPageIphController;
import org.chromium.chrome.browser.privacy_sandbox.ActivityTypeMapper;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxDialogController;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSurveyController;
import org.chromium.chrome.browser.privacy_sandbox.SurfaceType;
import org.chromium.chrome.browser.privacy_sandbox.TrackingProtectionSnackbarController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudIphController;
import org.chromium.chrome.browser.reengagement.ReengagementNotificationController;
import org.chromium.chrome.browser.searchwidget.SearchActivityClientImpl;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.RequestDesktopUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarBehavior;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuBlocker;
import org.chromium.chrome.browser.ui.appmenu.AppMenuDelegate;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityClient;
import org.chromium.chrome.browser.ui.searchactivityutils.SearchActivityExtras.IntentOrigin;
import org.chromium.chrome.browser.ui.system.StatusBarColorController.StatusBarColorProvider;
import org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderLayoutCoordinator;
import org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderUtils;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgeManager;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgeSupplier;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.BooleanSupplier;

/** A {@link RootUiCoordinator} variant that controls UI for {@link BaseCustomTabActivity}. */
public class BaseCustomTabRootUiCoordinator extends RootUiCoordinator {

    private final Supplier<CustomTabToolbarCoordinator> mToolbarCoordinator;
    private final Supplier<BrowserServicesIntentDataProvider> mIntentDataProvider;
    private final Supplier<CustomTabActivityTabController> mTabController;
    private final Supplier<CustomTabMinimizeDelegate> mMinimizeDelegateSupplier;
    private final Supplier<CustomTabFeatureOverridesManager> mFeatureOverridesManagerSupplier;
    private final SearchActivityClient mCustomTabSearchClient;

    private CustomTabHeightStrategy mCustomTabHeightStrategy;

    private @Nullable BrandingController mBrandingController;

    private @Nullable DesktopSiteSettingsIphController mDesktopSiteSettingsIphController;
    private @Nullable PdfPageIphController mPdfPageIphController;
    private @Nullable CustomTabHistoryIphController mCustomTabHistoryIphController;
    private @Nullable ReadAloudIphController mReadAloudIphController;
    private @Nullable GoogleBottomBarCoordinator mGoogleBottomBarCoordinator;
    private @Nullable TrackingProtectionSnackbarController mTrackingProtectionSnackbarController;

    private @Nullable EdgeToEdgeSupplier.ChangeObserver mEdgeToEdgeChangeObserver;
    private final @NonNull Runnable mOpenInBrowserRunnable;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private @Nullable WebAppHeaderLayoutCoordinator mWebAppHeaderLayoutCoordinator;
    private final Supplier<BrowserServicesThemeColorProvider> mWebAppThemeColorProvider;

    // TODO(crbug.com/402213312): This can be NonNull once the flag is enabled by default.
    private @Nullable CustomTabToolbarButtonsCoordinator mToolbarButtonsCoordinator;

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
     * @param intentDataProvider Contains intent information used to start the Activity.
     * @param tabController Activity tab controller.
     * @param minimizeDelegateSupplier Supplies the {@link CustomTabMinimizeDelegate} used to
     *     minimize the tab.
     * @param featureOverridesManagerSupplier Supplies the {@link CustomTabFeatureOverridesManager}.
     * @param openInBrowserRunnable Runnable opening the current tab in BrApp.
     * @param edgeToEdgeManager Manages core edge-to-edge state and logic.
     * @param desktopWindowStateManager Provides information about desktop windowing state.
     * @param webAppThemeColorProvider Provides current theme of a web app.
     */
    public BaseCustomTabRootUiCoordinator(
            @NonNull AppCompatActivity activity,
            @NonNull ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            @NonNull ActivityTabProvider tabProvider,
            @NonNull ObservableSupplier<Profile> profileSupplier,
            @NonNull ObservableSupplier<BookmarkModel> bookmarkModelSupplier,
            @NonNull ObservableSupplier<TabBookmarker> tabBookmarkerSupplier,
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
            @NonNull AppMenuDelegate appMenuDelegate,
            @NonNull StatusBarColorProvider statusBarColorProvider,
            @NonNull IntentRequestTracker intentRequestTracker,
            @NonNull Supplier<CustomTabToolbarCoordinator> customTabToolbarCoordinator,
            @NonNull Supplier<BrowserServicesIntentDataProvider> intentDataProvider,
            @NonNull BackPressManager backPressManager,
            @NonNull Supplier<CustomTabActivityTabController> tabController,
            @NonNull Supplier<CustomTabMinimizeDelegate> minimizeDelegateSupplier,
            @NonNull Supplier<CustomTabFeatureOverridesManager> featureOverridesManagerSupplier,
            @NonNull Runnable openInBrowserRunnable,
            @NonNull EdgeToEdgeManager edgeToEdgeManager,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            @Nullable Supplier<BrowserServicesThemeColorProvider> webAppThemeColorProvider) {
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
                new ObservableSupplierImpl<>(Color.TRANSPARENT),
                edgeToEdgeManager);
        mToolbarCoordinator = customTabToolbarCoordinator;
        mIntentDataProvider = intentDataProvider;
        mCustomTabSearchClient = new SearchActivityClientImpl(activity, IntentOrigin.CUSTOM_TAB);
        mDesktopWindowStateManager = desktopWindowStateManager;
        mWebAppThemeColorProvider = webAppThemeColorProvider;

        boolean isAuthTab = intentDataProvider.get().isAuthTab();
        if ((activityType == ActivityType.CUSTOM_TAB || isAuthTab)
                && !intentDataProvider.get().isOpenedByChrome()
                && intentDataProvider.get().getCustomTabMode() != INCOGNITO) {
            String packageName = null;
            // AuthTab sets the package name to null by design to show the default branding (toast)
            // every time one is launched.
            if (!isAuthTab) {
                packageName = mIntentDataProvider.get().getClientPackageName();
                if (TextUtils.isEmpty(packageName)) {
                    packageName = CustomTabIntentDataProvider.getAppIdFromReferrer(activity);
                }
            }
            String browserName = activity.getResources().getString(R.string.app_name);
            int toastTemplateId =
                    isAuthTab
                            ? R.string.auth_tab_secured_by_chrome_template
                            : R.string.twa_running_in_chrome_template;
            String appId = packageName; // effective final for lambda func
            mBrandingController =
                    new BrandingController(
                            activity,
                            appId,
                            browserName,
                            toastTemplateId,
                            () -> createMismatchNotificationChecker(appId),
                            new ChromePureJavaExceptionReporter());
        }
        // TODO(353517557): Do initialization necessary for ActivityType.AUTH_TAB

        mTabController = tabController;
        mMinimizeDelegateSupplier = minimizeDelegateSupplier;
        mFeatureOverridesManagerSupplier = featureOverridesManagerSupplier;
        mOpenInBrowserRunnable = openInBrowserRunnable;
        // TODO(crbug.com/41481778): move this RootUiCoordinator once this flag is removed.
        if (ChromeFeatureList.sCctTabModalDialog.isEnabled()) {
            getAppBrowserControlsVisibilityDelegate()
                    .addDelegate(browserControlsManager.getBrowserVisibilityDelegate());
        }
    }

    MismatchNotificationChecker createMismatchNotificationChecker(String appId) {
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        Intent intent = mIntentDataProvider.get().getIntent();
        if (!connection.isAppForAccountMismatchNotification(intent)) return null;

        // MismatchNotificationChecker requires Profile which becomes available after
        // native init. This method is expected to be called lazily, when it is actually
        // needed.
        boolean signInPromptEnabled =
                appId != null
                        && FeatureList.isInitialized()
                        && SigninFeatureMap.isEnabled(SigninFeatures.CCT_SIGN_IN_PROMPT);
        if (!signInPromptEnabled) return null;

        if (isMismatchNotificationSuppressed()) {
            MismatchNotificationController.recordMismatchNoticeSuppressedHistogram(
                    MismatchNotificationController.SuppressedReason.FRE_COMPLETED_RECENTLY);
            return null;
        }

        if (!mProfileSupplier.hasValue()) {
            return null;
        }

        Profile profile = mProfileSupplier.get();
        // Exclude incognito and ephemeral sessions.
        if (profile.isOffTheRecord()) {
            MismatchNotificationController.recordMismatchNoticeSuppressedHistogram(
                    MismatchNotificationController.SuppressedReason.CCT_IS_OFF_THE_RECORD);
            return null;
        }
        return new MismatchNotificationChecker(
                profile,
                IdentityServicesProvider.get().getIdentityManager(profile),
                (accountId, lastShownTime, mimData, onClose) -> {
                    boolean show =
                            connection.shouldShowAccountMismatchNotification(
                                    intent, profile, accountId, lastShownTime, mimData);
                    if (show) {
                        MismatchNotificationController.get(
                                        mWindowAndroid,
                                        profile,
                                        connection.getAppAccountName(intent))
                                .showSignedOutMessage(mActivity, onClose);
                    }
                    return show;
                });
    }

    private static boolean isMismatchNotificationSuppressed() {
        // Skip checking if the cadence is set to zero for easy local testing.
        // TODO(crbug.com/372609889): Use a dedicated flag param.
        SigninFeatureMap featureMap = SigninFeatureMap.getInstance();
        int cadence =
                featureMap.getFieldTrialParamByFeatureAsInt(
                        SigninFeatures.CCT_SIGN_IN_PROMPT, "cadence_day", 14);
        if (cadence == 0) return false;

        final long suppressionPeriodStart =
                SigninPreferencesManager.getInstance().getCctMismatchNoticeSuppressionPeriodStart();
        if (suppressionPeriodStart == 0) return false;
        final long currentTime = TimeUtils.currentTimeMillis();
        // We suppress the notification for two weeks after the FRE was completed.
        if (currentTime - suppressionPeriodStart < 2 * DateUtils.WEEK_IN_MILLIS) {
            return true;
        }
        SigninPreferencesManager.getInstance().clearCctMismatchNoticeSuppressionPeriodStart();
        return false;
    }

    @Override
    protected void initializeToolbar() {
        CustomTabsConnection connection = CustomTabsConnection.getInstance();
        boolean shouldEnableOmnibox =
                connection.shouldEnableOmniboxForIntent(mIntentDataProvider.get());
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.Omnibox.EnabledState", shouldEnableOmnibox);
        var omniboxParams =
                shouldEnableOmnibox
                        ? new CustomTabToolbar.OmniboxParams(
                                mCustomTabSearchClient,
                                mIntentDataProvider.get().getClientPackageName(),
                                connection.getAlternateOmniboxTapHandler(mIntentDataProvider.get()))
                        : null;
        if (ChromeFeatureList.sCctToolbarRefactor.isEnabled()) {
            CustomTabToolbar toolbar = mActivity.findViewById(R.id.toolbar);
            mToolbarButtonsCoordinator =
                    new CustomTabToolbarButtonsCoordinator(
                            mActivity,
                            toolbar,
                            mIntentDataProvider.get(),
                            params -> mToolbarCoordinator.get().onCustomButtonClick(params),
                            mMinimizeDelegateSupplier.get(),
                            mFeatureOverridesManagerSupplier.get(),
                            omniboxParams,
                            mActivityLifecycleDispatcher);
            super.initializeToolbar();

            mToolbarCoordinator.get().onToolbarInitialized(mToolbarManager);
            View coordinator = mActivity.findViewById(R.id.coordinator);
            mCustomTabHeightStrategy.onToolbarInitialized(
                    coordinator,
                    toolbar,
                    mIntentDataProvider.get().getPartialTabToolbarCornerRadius(),
                    mToolbarButtonsCoordinator);

            return;
        }

        super.initializeToolbar();

        // TODO(crbug.com/402213312): Move as much of this as possible into
        // CustomTabToolbar#initializeToolbar rather than calling a bunch of setters.

        mToolbarCoordinator.get().onToolbarInitialized(mToolbarManager);

        CustomTabToolbar toolbar = mActivity.findViewById(R.id.toolbar);
        if (ChromeFeatureList.sCctIntentFeatureOverrides.isEnabled()) {
            toolbar.setFeatureOverridesManager(mFeatureOverridesManagerSupplier.get());
        }
        View coordinator = mActivity.findViewById(R.id.coordinator);
        mCustomTabHeightStrategy.onToolbarInitialized(
                coordinator,
                toolbar,
                mIntentDataProvider.get().getPartialTabToolbarCornerRadius(),
                null);
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

        if (shouldEnableOmnibox) {
            toolbar.setOmniboxParams(omniboxParams);
        }
    }

    @ExperimentalOpenInBrowser
    @Override
    protected AdaptiveToolbarBehavior createAdaptiveToolbarBehavior(
            Supplier<Tracker> trackerSupplier) {
        return new CustomTabAdaptiveToolbarBehavior(
                mActivity,
                mActivityTabProvider,
                mIntentDataProvider.get(),
                AppCompatResources.getDrawable(mActivity, R.drawable.ic_open_in_new_white_24dp),
                mOpenInBrowserRunnable,
                () -> addVoiceSearchAdaptiveButton(trackerSupplier));
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
                                mReadAloudIphController =
                                        new ReadAloudIphController(
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
                            mActivityType,
                            mProfileSupplier.get().isIncognitoBranded());
        }
    }

    public CustomTabHistoryIphController getHistoryIphController() {
        return mCustomTabHistoryIphController;
    }

    public ContextualPageActionController getContextualPageActionController() {
        return mAdaptiveToolbarUiCoordinator.getContextualPageActionController();
    }

    public void runPriceInsightsAction() {
        mAdaptiveToolbarUiCoordinator.runPriceInsightsAction();
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
    protected boolean shouldAllowThemingOnTablets() {
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
                        mActivityLifecycleDispatcher,
                        mFullscreenManager,
                        () -> mMinimizeDelegateSupplier.get().isMinimized(),
                        DeviceFormFactor.isWindowOnTablet(mWindowAndroid));
    }

    @Override
    public void onPostInflationStartup() {
        super.onPostInflationStartup();
        mCustomTabHeightStrategy.onPostInflationStartup();
        mCustomTabHistoryIphController =
                CustomTabAppMenuHelper.maybeCreateHistoryIphController(
                        mAppMenuCoordinator,
                        mActivity,
                        mActivityTabProvider,
                        mProfileSupplier,
                        mIntentDataProvider.get());

        final var intentDataProvider = mIntentDataProvider.get();
        if (WebAppHeaderUtils.isMinimalUiEnabled(intentDataProvider)) {
            final var desktopWindowStateManager = getDesktopWindowStateManager();
            assert desktopWindowStateManager != null;

            mWebAppHeaderLayoutCoordinator =
                    new WebAppHeaderLayoutCoordinator(
                            mActivity.findViewById(
                                    org.chromium.chrome.browser.web_app_header.R.id
                                            .web_app_header_layout),
                            desktopWindowStateManager,
                            mActivityTabProvider,
                            mWebAppThemeColorProvider.get(),
                            intentDataProvider,
                            getScrimManager(),
                            (tab) -> {
                                Intent fullHistoryIntent = new Intent(Intent.ACTION_MAIN);
                                fullHistoryIntent.setClass(mActivity, ChromeLauncherActivity.class);
                                fullHistoryIntent.putExtra(IntentHandler.EXTRA_OPEN_HISTORY, true);
                                IntentUtils.addTrustedIntentExtras(fullHistoryIntent);
                                mActivity.startActivity(fullHistoryIntent);
                            });
        }
    }

    @Nullable
    @Override
    public DesktopWindowStateManager getDesktopWindowStateManager() {
        return mDesktopWindowStateManager;
    }

    @Override
    protected boolean showWebSearchInActionMode() {
        return !mIntentDataProvider.get().isAuthTab();
    }

    @Override
    protected void onScrimColorChanged(@ColorInt int scrimColor) {
        super.onScrimColorChanged(scrimColor);
        // TODO(jinsukkim): Separate CCT scrim update action from status bar scrim stuff.
        mCustomTabHeightStrategy.setScrimColor(scrimColor);
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
                && !ChromeFeatureList.sDrawKeyNativeEdgeToEdgeDisableCctMediaViewerE2e.getValue()
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

        if (mDesktopSiteSettingsIphController != null) {
            mDesktopSiteSettingsIphController.destroy();
            mDesktopSiteSettingsIphController = null;
        }

        if (mPdfPageIphController != null) {
            mPdfPageIphController.destroy();
            mPdfPageIphController = null;
        }

        if (mReadAloudIphController != null) {
            mReadAloudIphController.destroy();
            mReadAloudIphController = null;
        }

        mCustomTabHeightStrategy.destroy();

        if (mCustomTabHistoryIphController != null) {
            mCustomTabHistoryIphController.destroy();
            mCustomTabHistoryIphController = null;
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                && mWebAppHeaderLayoutCoordinator != null) {
            mWebAppHeaderLayoutCoordinator.destroy();
            mWebAppHeaderLayoutCoordinator = null;
        }

        if (mToolbarButtonsCoordinator != null) {
            mToolbarButtonsCoordinator.destroy();
            mToolbarButtonsCoordinator = null;
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
                            PrivacySandboxSurveyController surveyController =
                                    PrivacySandboxSurveyController.initialize(
                                            mTabModelSelectorSupplier.get(),
                                            mActivityLifecycleDispatcher,
                                            mActivity,
                                            mMessageDispatcher,
                                            mActivityTabProvider,
                                            profile);
                            String appId = mIntentDataProvider.get().getClientPackageName();
                            // TODO(crbug.com/390429345): Refactor Ads CCT Notice logic into the PS
                            // dialog controller
                            if (ChromeFeatureList.isEnabled(
                                            ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT)
                                    && shouldShowPrivacySandboxDialog
                                    && isCustomTab) {
                                boolean shouldShowPrivacySandboxDialogAppIdCheck = true;
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
                                    if (surveyController != null) {
                                        PrivacySandboxDialogController.setOnDialogDismissRunnable(
                                                () ->
                                                        surveyController
                                                                .maybeScheduleAdsCctTreatmentSurveyLaunch(
                                                                        appId));
                                    }
                                    didShowPrompt =
                                            PrivacySandboxDialogController
                                                    .maybeLaunchPrivacySandboxDialog(
                                                            mActivity,
                                                            currentModelProfile,
                                                            SurfaceType.AGACCT,
                                                            mWindowAndroid);
                                }
                            } else if (surveyController != null
                                    && !ChromeFeatureList.isEnabled(
                                            ChromeFeatureList.PRIVACY_SANDBOX_ADS_NOTICE_CCT)
                                    && shouldShowPrivacySandboxDialog
                                    && isCustomTab) {
                                surveyController.maybeScheduleAdsCctControlSurveyLaunch(
                                        appId,
                                        new PrivacySandboxBridge(currentModelProfile)
                                                .getRequiredPromptType(SurfaceType.AGACCT));
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
                                mDesktopSiteSettingsIphController =
                                        DesktopSiteSettingsIphController.create(
                                                mActivity,
                                                mWindowAndroid,
                                                mActivityTabProvider,
                                                regularProfile,
                                                getToolbarManager().getMenuButtonView(),
                                                mAppMenuCoordinator.getAppMenuHandler());
                                mPdfPageIphController =
                                        PdfPageIphController.create(
                                                mActivity,
                                                mWindowAndroid,
                                                mActivityTabProvider,
                                                profile,
                                                getToolbarManager().getMenuButtonView(),
                                                mAppMenuCoordinator.getAppMenuHandler(),
                                                /* isBrowserApp= */ false);
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

    /** Returns SearchActivityClient instance used by Search in CCT. */
    /* package */ SearchActivityClient getCustomTabSearchClient() {
        return mCustomTabSearchClient;
    }

    @VisibleForTesting
    public WebAppHeaderLayoutCoordinator getWebAppHeaderLayoutCoordinator() {
        return mWebAppHeaderLayoutCoordinator;
    }
}
