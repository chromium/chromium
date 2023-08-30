// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.chromium.chrome.browser.ui.fold_transitions.FoldTransitionController.RESUME_HOME_SURFACE_ON_MODE_CHANGE;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.text.format.DateUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.SegmentationPlatformServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.ActiveTabState;
import org.chromium.chrome.browser.ui.fold_transitions.FoldTransitionController;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.HostSurface;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.chrome.features.start_surface.StartSurfaceUserData;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.segmentation_platform.SegmentSelectionResult;
import org.chromium.components.segmentation_platform.SegmentationPlatformService;
import org.chromium.components.segmentation_platform.proto.SegmentationProto.SegmentId;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This is a utility class for managing features related to returning to Chrome after haven't used
 * Chrome for a while.
 */
public final class ReturnToChromeUtil {
    private static ChromeActivity sActivityPresentingOverivewWithOmniboxForTesting;

    /**
     * The reasons of failing to show the home surface UI on a NTP.
     *
     * These values are persisted to logs. Entries should not be renumbered and numeric values
     * should never be reused. See tools/metrics/histograms/enums.xml.
     */
    @IntDef({FailToShowHomeSurfaceReason.FAIL_TO_CREATE_NTP_TAB,
            FailToShowHomeSurfaceReason.FAIL_TO_FIND_NTP_TAB,
            FailToShowHomeSurfaceReason.NOT_A_NATIVE_PAGE,
            FailToShowHomeSurfaceReason.NOT_A_NTP_NATIVE_PAGE,
            FailToShowHomeSurfaceReason.NATIVE_PAGE_IS_FROZEN,
            FailToShowHomeSurfaceReason.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    @interface FailToShowHomeSurfaceReason {
        int FAIL_TO_CREATE_NTP_TAB = 0;
        int FAIL_TO_FIND_NTP_TAB = 1;
        int NOT_A_NATIVE_PAGE = 2;

        int NOT_A_NTP_NATIVE_PAGE = 3;
        int NATIVE_PAGE_IS_FROZEN = 4;
        int NUM_ENTRIES = 5;
    }

    @VisibleForTesting
    public static final String LAST_VISITED_TAB_IS_SRP_WHEN_OVERVIEW_IS_SHOWN_AT_LAUNCH_UMA =
            "Startup.Android.LastVisitedTabIsSRPWhenOverviewShownAtLaunch";
    public static final String LAST_ACTIVE_TAB_IS_NTP_WHEN_OVERVIEW_IS_SHOWN_AT_LAUNCH_UMA =
            "StartSurface.ColdStartup.IsLastActiveTabNtp";
    public static final String SHOWN_FROM_BACK_NAVIGATION_UMA =
            "StartSurface.ShownFromBackNavigation.";
    public static final String START_SHOW_STATE_UMA = "StartSurface.Show.State";

    public static final String HOME_SURFACE_SHOWN_AT_STARTUP_UMA =
            "NewTabPage.AsHomeSurface.ShownAtStartup";
    public static final String HOME_SURFACE_SHOWN_UMA = "NewTabPage.AsHomeSurface";
    public static final String FAIL_TO_SHOW_HOME_SURFACE_UI_UMA =
            "NewTabPage.FailToShowHomeSurfaceUI";

    private static final String START_V2_SEGMENTATION_PLATFORM_KEY = "chrome_start_android_v2";

    private static boolean sIsHomepagePolicyManagerInitializedRecorded;
    // Whether to skip the check of the initialization of HomepagePolicyManager.
    private static boolean sSkipInitializationCheckForTesting;

    public static void setActivityPresentingOverivewWithOmniboxForTesting(ChromeActivity value) {
        sActivityPresentingOverivewWithOmniboxForTesting = value;
        ResettersForTesting.register(() -> sActivityPresentingOverivewWithOmniboxForTesting = null);
    }

    private ReturnToChromeUtil() {}

    /**
     * A helper class to handle the back press related to ReturnToChrome feature. If a tab is opened
     * from start surface and this tab is unable to be navigated back further, then we trigger
     * the callback to show overview mode.
     */
    public static class ReturnToChromeBackPressHandler implements BackPressHandler, Destroyable {
        private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
                new ObservableSupplierImpl<>();
        private final Callback<Boolean> mOnBackPressedCallback;
        private final ActivityTabProvider.ActivityTabTabObserver mActivityTabObserver;
        private final ActivityTabProvider mActivityTabProvider;
        private final Supplier<Tab> mTabSupplier; // for debugging only
        private final Supplier<Long> mLastBackPressMsSupplier;
        private LayoutStateProvider mLayoutStateProvider;
        private LayoutStateObserver mLayoutStateObserver;
        private boolean mIsHandleTabSwitcherShownEnabled;

        public ReturnToChromeBackPressHandler(ActivityTabProvider activityTabProvider,
                Callback<Boolean> onBackPressedCallback, Supplier<Tab> tabSupplier,
                OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
                Supplier<Long> lastBackPressMsSupplier, boolean isHandleTabSwitcherShownEnabled) {
            mActivityTabProvider = activityTabProvider;
            mActivityTabObserver =
                    new ActivityTabProvider.ActivityTabTabObserver(activityTabProvider, true) {
                        @Override
                        protected void onObservingDifferentTab(Tab tab, boolean hint) {
                            onBackPressStateChanged();
                        }
                    };
            mOnBackPressedCallback = onBackPressedCallback;
            mTabSupplier = tabSupplier;
            mLastBackPressMsSupplier = lastBackPressMsSupplier;
            mIsHandleTabSwitcherShownEnabled = isHandleTabSwitcherShownEnabled;
            if (mIsHandleTabSwitcherShownEnabled) {
                layoutStateProviderSupplier.onAvailable(this::onLayoutStateProviderAvailable);
            }
            onBackPressStateChanged();
        }

        private void onLayoutStateProviderAvailable(LayoutStateProvider layoutStateProvider) {
            mLayoutStateProvider = layoutStateProvider;
            if (mLayoutStateObserver == null) {
                mLayoutStateObserver = new LayoutStateObserver() {
                    @Override
                    public void onFinishedShowing(int layoutType) {
                        onBackPressStateChanged();
                    }
                };
            }
            mLayoutStateProvider.addObserver(mLayoutStateObserver);
        }

        private void onBackPressStateChanged() {
            Tab tab = mActivityTabProvider.get();
            mBackPressChangedSupplier.set(tab != null && isTabFromStartSurface(tab)
                    || shouldHandleTabSwitcherShown(
                            mIsHandleTabSwitcherShownEnabled, mLayoutStateProvider));
        }

        @Override
        public @BackPressResult int handleBackPress() {
            Tab tab = mActivityTabProvider.get();
            boolean handleTabSwitcherShown = shouldHandleTabSwitcherShown(
                    mIsHandleTabSwitcherShownEnabled, mLayoutStateProvider);
            boolean res = tab != null && !tab.canGoBack() && isTabFromStartSurface(tab)
                    || handleTabSwitcherShown;
            if (!res) {
                var controlTab = mTabSupplier.get();
                int layoutType = mLayoutStateProvider != null
                        ? mLayoutStateProvider.getActiveLayoutType()
                        : LayoutType.NONE;
                long interval = -1;
                if (mLastBackPressMsSupplier.get() != -1) {
                    interval = TimeUtils.elapsedRealtimeMillis() - mLastBackPressMsSupplier.get();
                }
                String msg =
                        "tab %s; control tab %s; back press state %s; layout %s; isFromSS: %s; interval %s";
                boolean isFromSS = tab != null && isTabFromStartSurface(tab);
                assert false : String.format(msg, tab, controlTab, tab != null && tab.canGoBack(),
                                       layoutType, isFromSS, interval);
                if (BackPressManager.correctTabNavigationOnFallback()) {
                    return BackPressResult.FAILURE;
                }
            }
            mOnBackPressedCallback.onResult(handleTabSwitcherShown);
            return res ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
        }

        @Override
        public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
            return mBackPressChangedSupplier;
        }

        @Override
        public void destroy() {
            mActivityTabObserver.destroy();
            if (mLayoutStateProvider != null) {
                mLayoutStateProvider.removeObserver(mLayoutStateObserver);
                mLayoutStateProvider = null;
            }
        }
    }

    /**
     * Returns whether to handle the back operation if the Tab switcher is showing.
     * @param shouldHandleTabSwitcherShown Whether the back operation should be handled when the
     *     Tab switcher is showing. It is only true when both Start surface and Start surface
     * refactor feature flags are enabled.
     * @param layoutStateProvider The provider of the current layout state.
     */
    public static boolean shouldHandleTabSwitcherShown(
            boolean shouldHandleTabSwitcherShown, LayoutStateProvider layoutStateProvider) {
        return shouldHandleTabSwitcherShown && layoutStateProvider != null
                && layoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER);
    }

    /**
     * Shows the Start surface if the given {@link handleTabSwitcherShown} is true.
     * @param handleTabSwitcherShown Whether to handle the back operation from the current showing
     *                               Tab switcher.
     * @param layoutManager The {@link LayoutManager} object.
     */
    public static boolean mayReturnToStartSurface(
            boolean handleTabSwitcherShown, LayoutManager layoutManager) {
        if (!handleTabSwitcherShown) return false;

        recordStartSurfaceState(StartSurfaceState.SHOWING_HOMEPAGE);
        recordBackNavigationToStart("FromTabSwitcher");
        layoutManager.showLayout(LayoutType.START_SURFACE, false);
        return true;
    }

    /**
     * Determine if we should show the tab switcher on returning to Chrome.
     *   Returns true if enough time has elapsed since the app was last backgrounded or foreground,
     *   depending on which time is the max.
     *   The threshold time in milliseconds is set by experiment "enable-start-surface-return-time"
     *   or from segmentation platform result if {@link ChromeFeatureList.START_SURFACE_RETURN_TIME}
     *   is enabled.
     *
     * @param lastTimeMillis The last time the application was backgrounded or foreground, depends
     *                       on which time is the max. Set in ChromeTabbedActivity::onStopWithNative
     * @param isTablet Whether the activity is running in tablet mode.
     * @return true if past threshold, false if not past threshold or experiment cannot be loaded.
     */
    public static boolean shouldShowTabSwitcher(final long lastTimeMillis, boolean isTablet) {
        long tabSwitcherAfterMillis = getReturnTime(isTablet
                        ? StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS
                        : StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS);

        if (lastTimeMillis == -1) {
            // No last background timestamp set, use control behavior unless "immediate" was set.
            return tabSwitcherAfterMillis == 0;
        }

        if (tabSwitcherAfterMillis < 0) {
            // If no value for experiment, use control behavior.
            return false;
        }

        return System.currentTimeMillis() - lastTimeMillis >= tabSwitcherAfterMillis;
    }

    /**
     * Gets the return time interval. The return time is in the unit of milliseconds.
     * @param returnTime The return time parameter based on form factor, either phones or tablets.
     */
    private static long getReturnTime(IntCachedFieldTrialParameter returnTime) {
        if (ChromeFeatureList.sStartSurfaceReturnTime.isEnabled() && returnTime.getValue() != 0
                && StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_USE_MODEL.getValue()) {
            return getReturnTimeFromSegmentation(returnTime);
        }

        return returnTime.getValue() * DateUtils.SECOND_IN_MILLIS;
    }

    /**
     * Gets the cached return time obtained from the segmentation platform service.
     * Note: this function should NOT been called on tablets! The default value for tablets is -1
     * which means not showing.
     * @return How long to show the Start surface again on startup. A negative value means not show,
     *         0 means showing immediately. The return time is in the unit of milliseconds.
     */
    @VisibleForTesting
    public static long getReturnTimeFromSegmentation(IntCachedFieldTrialParameter returnTime) {
        // Sets the default value as 8 hours; 0 means showing immediately.
        return SharedPreferencesManager.getInstance().readLong(
                ChromePreferenceKeys.START_RETURN_TIME_SEGMENTATION_RESULT_MS,
                returnTime.getDefaultValue());
    }

    /**
     * Check if we should handle the navigation. If so, create a new tab and load the URL.
     *
     * @param params The LoadUrlParams to load.
     * @param incognito Whether to load URL in an incognito Tab.
     * @param parentTab  The parent tab used to create a new tab if needed.
     * @return Current tab created if we have handled the navigation, null otherwise.
     */
    public static Tab handleLoadUrlFromStartSurface(
            LoadUrlParams params, @Nullable Boolean incognito, @Nullable Tab parentTab) {
        return handleLoadUrlFromStartSurface(params, false, incognito, parentTab);
    }

    /**
     * Check if we should handle the navigation. If so, create a new tab and load the URL.
     *
     * @param params The LoadUrlParams to load.
     * @param isBackground Whether to load the URL in a new tab in the background.
     * @param incognito Whether to load URL in an incognito Tab.
     * @param parentTab  The parent tab used to create a new tab if needed.
     * @return Current tab created if we have handled the navigation, null otherwise.
     */
    public static Tab handleLoadUrlFromStartSurface(LoadUrlParams params, boolean isBackground,
            @Nullable Boolean incognito, @Nullable Tab parentTab) {
        try (TraceEvent e = TraceEvent.scoped("StartSurface.LoadUrl")) {
            return handleLoadUrlWithPostDataFromStartSurface(
                    params, null, null, isBackground, incognito, parentTab);
        }
    }

    /**
     * Check if we should handle the navigation. If so, create a new tab and load the URL with POST
     * data.
     *
     * @param params The LoadUrlParams to load.
     * @param postDataType postData type.
     * @param postData POST data to include in the tab URL's request body, ex. bitmap when image
     *                 search.
     * @param incognito Whether to load URL in an incognito Tab. If null, the current tab model will
     *                  be used.
     * @param parentTab The parent tab used to create a new tab if needed.
     * @return true if we have handled the navigation, false otherwise.
     */
    public static boolean handleLoadUrlWithPostDataFromStartSurface(LoadUrlParams params,
            @Nullable String postDataType, @Nullable byte[] postData, @Nullable Boolean incognito,
            @Nullable Tab parentTab) {
        return handleLoadUrlWithPostDataFromStartSurface(
                       params, postDataType, postData, false, incognito, parentTab)
                != null;
    }

    /**
     * Check if we should handle the navigation. If so, create a new tab and load the URL with POST
     * data.
     *
     * @param params The LoadUrlParams to load.
     * @param postDataType   postData type.
     * @param postData       POST data to include in the tab URL's request body, ex. bitmap when
     *         image search.
     * @param isBackground Whether to load the URL in a new tab in the background.
     * @param incognito Whether to load URL in an incognito Tab. If null, the current tab model will
     *         be used.
     * @param parentTab  The parent tab used to create a new tab if needed.
     * @return Current tab created if we have handled the navigation, null otherwise.
     */
    private static Tab handleLoadUrlWithPostDataFromStartSurface(LoadUrlParams params,
            @Nullable String postDataType, @Nullable byte[] postData, boolean isBackground,
            @Nullable Boolean incognito, @Nullable Tab parentTab) {
        String url = params.getUrl();
        ChromeActivity chromeActivity = getActivityPresentingOverviewWithOmnibox(url);
        if (chromeActivity == null) return null;

        // Create a new unparented tab.
        boolean incognitoParam;
        if (incognito == null) {
            incognitoParam = chromeActivity.getCurrentTabModel().isIncognito();
        } else {
            incognitoParam = incognito;
        }

        if (!TextUtils.isEmpty(postDataType) && postData != null && postData.length != 0) {
            params.setVerbatimHeaders("Content-Type: " + postDataType);
            params.setPostData(ResourceRequestBody.createFromBytes(postData));
        }

        Tab newTab = chromeActivity.getTabCreator(incognitoParam)
                             .createNewTab(params,
                                     isBackground ? TabLaunchType.FROM_LONGPRESS_BACKGROUND
                                                  : TabLaunchType.FROM_START_SURFACE,
                                     parentTab);
        if (isBackground) {
            StartSurfaceUserData.setOpenedFromStart(newTab);
        }

        int transitionAfterMask = params.getTransitionType() & PageTransition.CORE_MASK;
        if (transitionAfterMask == PageTransition.TYPED
                || transitionAfterMask == PageTransition.GENERATED) {
            RecordUserAction.record("MobileOmniboxUse.StartSurface");
            BrowserUiUtils.recordModuleClickHistogram(BrowserUiUtils.HostSurface.START_SURFACE,
                    BrowserUiUtils.ModuleTypeOnStartAndNTP.OMNIBOX);

            // These are not duplicated here with the recording in LocationBarLayout#loadUrl.
            RecordUserAction.record("MobileOmniboxUse");
            LocaleManager.getInstance().recordLocaleBasedSearchMetrics(
                    false, url, params.getTransitionType());
        }

        return newTab;
    }

    /**
     * @param url The URL to load.
     * @return The ChromeActivity if it is presenting the omnibox on the tab switcher, else null.
     */
    private static ChromeActivity getActivityPresentingOverviewWithOmnibox(String url) {
        if (sActivityPresentingOverivewWithOmniboxForTesting != null) {
            return sActivityPresentingOverivewWithOmniboxForTesting;
        }
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null || !isStartSurfaceEnabled(activity)
                || !(activity instanceof ChromeActivity)) {
            return null;
        }

        ChromeActivity chromeActivity = (ChromeActivity) activity;

        assert LibraryLoader.getInstance().isInitialized();
        if (!chromeActivity.isInOverviewMode() && !UrlUtilities.isNTPUrl(url)) return null;

        return chromeActivity;
    }

    /**
     * Check whether we should show Start Surface as the home page. This is used for all cases
     * except initial tab creation, which uses {@link
     * ReturnToChromeUtil#isStartSurfaceEnabled(Context)}.
     *
     * @return Whether Start Surface should be shown as the home page.
     * @param context The activity context
     */
    public static boolean shouldShowStartSurfaceAsTheHomePage(Context context) {
        return isStartSurfaceEnabled(context)
                && StartSurfaceConfiguration.START_SURFACE_OPEN_START_AS_HOMEPAGE.getValue()
                && useChromeHomepage();
    }

    /**
     * Returns whether to use Chrome's homepage. This function doesn't distinguish whether to show
     * NTP or Start though. If checking whether to show Start as homepage, use
     * {@link ReturnToChromeUtil#shouldShowStartSurfaceAsTheHomePage(Context)} instead.
     */
    @VisibleForTesting
    public static boolean useChromeHomepage() {
        String homePageUrl = HomepageManager.getHomepageUri();
        return HomepageManager.isHomepageEnabled()
                && ((HomepagePolicyManager.isInitializedWithNative()
                            || sSkipInitializationCheckForTesting)
                        && (TextUtils.isEmpty(homePageUrl)
                                || UrlUtilities.isCanonicalizedNTPUrl(homePageUrl)));
    }

    /**
     * @return Whether we should show Start Surface as the home page on phone. Start surface
     *         hasn't been enabled on tablet yet.
     */
    public static boolean shouldShowStartSurfaceAsTheHomePageOnPhone(
            Context context, boolean isTablet) {
        return !isTablet && shouldShowStartSurfaceAsTheHomePage(context);
    }

    /**
     * @return Whether Start Surface should be shown as a new Tab.
     */
    public static boolean shouldShowStartSurfaceHomeAsNewTab(
            Context context, boolean incognito, boolean isTablet) {
        return !incognito && !isTablet && isStartSurfaceEnabled(context)
                && !StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START.getValue();
    }

    /**
     * @return Whether opening a NTP instead of Start surface for new Tab is enabled.
     */
    public static boolean shouldOpenNTPInsteadOfStart() {
        return StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START.getValue();
    }

    /**
     * Returns whether Start Surface is enabled in the given context.
     * This includes checks of:
     * 1) whether home page is enabled and whether it is Chrome' home page url;
     * 2) whether Start surface is enabled with current accessibility settings;
     * 3) whether it is on phone.
     * @param context The activity context.
     */
    public static boolean isStartSurfaceEnabled(Context context) {
        // When creating initial tab, i.e. cold start without restored tabs, we should only show
        // StartSurface as the HomePage if Single Pane is enabled, HomePage is not customized, not
        // on tablet, accessibility is not enabled or the tab group continuation feature is enabled.
        return StartSurfaceConfiguration.isStartSurfaceFlagEnabled()
                && !shouldHideStartSurfaceWithAccessibilityOn(context)
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(context);
    }

    /**
     * @return Whether start surface should be hidden when accessibility is enabled. If it's true,
     *         NTP is shown as homepage. Also, when time threshold is reached, grid tab switcher or
     *         overview list layout is shown instead of start surface.
     */
    public static boolean shouldHideStartSurfaceWithAccessibilityOn(Context context) {
        // TODO(crbug.com/1127732): Move this method back to StartSurfaceConfiguration.
        return ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                && !(ChromeFeatureList.sStartSurfaceWithAccessibility.isEnabled());
    }

    /**
     * @param tabModelSelector The tab model selector.
     * @return the total tab count, and works before native initialization.
     */
    public static int getTotalTabCount(TabModelSelector tabModelSelector) {
        if (!tabModelSelector.isTabStateInitialized()) {
            return SharedPreferencesManager.getInstance().readInt(
                           ChromePreferenceKeys.REGULAR_TAB_COUNT)
                    + SharedPreferencesManager.getInstance().readInt(
                            ChromePreferenceKeys.INCOGNITO_TAB_COUNT);
        }

        return tabModelSelector.getTotalTabCount();
    }

    /**
     * Returns whether grid Tab switcher or the Start surface should be shown at startup.
     */
    public static boolean shouldShowOverviewPageOnStart(Context context, Intent intent,
            TabModelSelector tabModelSelector, ChromeInactivityTracker inactivityTracker,
            boolean isTablet) {
        // Neither Start surface or GTS should be shown on Tablet at startup.
        if (isTablet) return false;

        String intentUrl = IntentHandler.getUrlFromIntent(intent);

        // If user launches Chrome by tapping the app icon, the intentUrl is NULL;
        // If user taps the "New Tab" item from the app icon, the intentUrl will be chrome://newtab,
        // and UrlUtilities.isCanonicalizedNTPUrl(intentUrl) returns true.
        // If user taps the "New Incognito Tab" item from the app icon, skip here and continue the
        // following checks.
        if (UrlUtilities.isCanonicalizedNTPUrl(intentUrl)
                && ReturnToChromeUtil.shouldShowStartSurfaceHomeAsNewTab(
                        context, tabModelSelector.isIncognitoSelected(), isTablet)
                && !intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false)) {
            return true;
        }

        // If Start surface isn't enabled, return false.
        if (!ReturnToChromeUtil.isStartSurfaceEnabled(context)) return false;

        return shouldShowHomeSurfaceAtStartupImpl(
                false /* isTablet */, intent, tabModelSelector, inactivityTracker);
    }

    private static boolean shouldShowHomeSurfaceAtStartupImpl(boolean isTablet, Intent intent,
            TabModelSelector tabModelSelector, ChromeInactivityTracker inactivityTracker) {
        // All of the following checks are based on Start surface is enabled.
        // If there's no tab existing, handle the initial tab creation.
        // Note: if user has a customized homepage, we don't show Start even there isn't any tab.
        // However, if NTP is used as homepage, we show Start when there isn't any tab. See
        // https://crbug.com/1368224.
        if (IntentUtils.isMainIntentFromLauncher(intent)
                && ReturnToChromeUtil.getTotalTabCount(tabModelSelector) <= 0) {
            boolean initialized = HomepagePolicyManager.isInitializedWithNative();
            if (!sIsHomepagePolicyManagerInitializedRecorded) {
                sIsHomepagePolicyManagerInitializedRecorded = true;
                RecordHistogram.recordBooleanHistogram(
                        "Startup.Android.IsHomepagePolicyManagerInitialized", initialized);
            }
            if (!initialized && !sSkipInitializationCheckForTesting) {
                return false;
            } else {
                return useChromeHomepage();
            }
        }

        // Checks whether to show the Start surface due to feature flag TAB_SWITCHER_ON_RETURN_MS.
        long lastVisibleTimeMs = inactivityTracker.getLastVisibleTimeMs();
        long lastBackgroundTimeMs = inactivityTracker.getLastBackgroundedTimeMs();
        return IntentUtils.isMainIntentFromLauncher(intent)
                && ReturnToChromeUtil.shouldShowTabSwitcher(
                        Math.max(lastBackgroundTimeMs, lastVisibleTimeMs), isTablet);
    }

    /**
     * Returns whether should show a NTP as the home surface at startup. This feature is only
     * enabled on Tablet.
     */
    public static boolean shouldShowNtpAsHomeSurfaceAtStartup(boolean isTablet, Intent intent,
            Bundle bundle, TabModelSelector tabModelSelector,
            ChromeInactivityTracker inactivityTracker) {
        // If "Start surface on tablet" isn't enabled, return false.
        if (!StartSurfaceConfiguration.isNtpAsHomeSurfaceEnabled(isTablet)) return false;

        // If the current session is recreated due to a transition from the phone mode to the tablet
        // mode on foldable, checks if the Start surface was shown on the phone mode before the
        // transition.
        if (shouldResumeHomeSurfaceOnFoldConfigurationChange(bundle)) return true;

        return shouldShowHomeSurfaceAtStartupImpl(
                true /* isTablet */, intent, tabModelSelector, inactivityTracker);
    }

    /**
     * Returns whether to show a Home surface on foldable when transiting from the phone mode to the
     * tablet mode. Returns true if Start surface was showing on phone mode before the transition.
     */
    @VisibleForTesting
    public static boolean shouldResumeHomeSurfaceOnFoldConfigurationChange(Bundle bundle) {
        if (bundle == null) return false;

        return bundle.getBoolean(FoldTransitionController.DID_CHANGE_TABLET_MODE, false)
                && bundle.getBoolean(RESUME_HOME_SURFACE_ON_MODE_CHANGE, false);
    }

    /**
     * @param currentTab  The current {@link Tab}.
     * @return Whether the Tab is launched with launchType TabLaunchType.FROM_START_SURFACE or it
     *         has "OpenedFromStart" property.
     */
    public static boolean isTabFromStartSurface(Tab currentTab) {
        final @TabLaunchType int type = currentTab.getLaunchType();
        return type == TabLaunchType.FROM_START_SURFACE
                || StartSurfaceUserData.isOpenedFromStart(currentTab);
    }

    /**
     * Creates a new Tab and show Home surface UI. This is called when the last active Tab isn't a
     * NTP, and we need to create one and show Home surface UI (a module showing the last active
     * Tab).
     * @param tabCreator The {@link TabCreator} object.
     * @param tabModelSelector The {@link TabModelSelector} object.
     * @param homeSurfaceTracker The {@link HomeSurfaceTracker} object.
     * @param lastActiveTabUrl The URL of the last active Tab. It is non-null in cold startup before
     *                         the Tab is restored.
     * @param lastActiveTab The object of the last active Tab. It is non-null after TabModel is
     *                      initialized, e.g., in warm startup.
     */
    public static Tab createNewTabAndShowHomeSurfaceUi(@NonNull TabCreator tabCreator,
            @NonNull HomeSurfaceTracker homeSurfaceTracker,
            @Nullable TabModelSelector tabModelSelector, @Nullable String lastActiveTabUrl,
            @Nullable Tab lastActiveTab) {
        assert lastActiveTab != null || lastActiveTabUrl != null;

        // Creates a new Tab if doesn't find an existing to reuse.
        Tab ntpTab = tabCreator.createNewTab(
                new LoadUrlParams(UrlConstants.NTP_URL), TabLaunchType.FROM_STARTUP, null);
        boolean isNtpUrl = UrlUtilities.isNTPUrl(ntpTab.getUrl());
        assert isNtpUrl : "The URL of the newly created NTP doesn't match NTP URL!";
        if (!isNtpUrl) {
            recordFailToShowHomeSurfaceReasonUma(
                    FailToShowHomeSurfaceReason.FAIL_TO_CREATE_NTP_TAB);
            return null;
        }

        // In cold startup, we only have the URL of the last active Tab.
        if (lastActiveTab == null) {
            // If the last active Tab isn't ready yet, we will listen to the willAddTab() event and
            // find the Tab instance with the given last active Tab's URL. The last active Tab is
            // always the first one to be restored.
            assert lastActiveTabUrl != null;
            TabModelObserver observer = new TabModelObserver() {
                @Override
                public void willAddTab(Tab tab, int type) {
                    boolean isTabExpected =
                            TextUtils.equals(lastActiveTabUrl, tab.getUrl().getSpec());
                    assert isTabExpected
                        : "The URL of first Tab restored doesn't match the URL of the last active "
                          + "Tab read from the Tab state metadata file! Existing Tab count = %d"
                          + tabModelSelector.getModel(false).getCount()
                          + ".";
                    if (!isTabExpected) {
                        return;
                    }
                    showHomeSurfaceUiOnNtp(ntpTab, tab, homeSurfaceTracker);
                    tabModelSelector.getModel(false).removeObserver(this);
                }

                @Override
                public void restoreCompleted() {
                    // This would be no-op if the observer has been removed in willAddTab().
                    tabModelSelector.getModel(false).removeObserver(this);
                }
            };
            tabModelSelector.getModel(false).addObserver(observer);
        } else {
            // In warm startup, the last active Tab is ready.
            showHomeSurfaceUiOnNtp(ntpTab, lastActiveTab, homeSurfaceTracker);
        }

        return ntpTab;
    }

    /**
     * Shows a NTP on warm startup on tablets if return time arrives. Only create a new NTP if there
     * isn't any existing NTP to reuse.
     * @param isIncognito Whether the incognito mode is selected.
     * @param shouldShowNtpHomeSurfaceOnStartup Whether to show a NTP as home surface on startup.
     * @param currentTabModel The object of the current {@link  TabModel}.
     * @param tabCreator The {@link TabCreator} object.
     * @param homeSurfaceTracker The {@link HomeSurfaceTracker} object.
     */
    public static void setInitialOverviewStateOnResumeOnTablet(boolean isIncognito,
            boolean shouldShowNtpHomeSurfaceOnStartup, TabModel currentTabModel,
            TabCreator tabCreator, HomeSurfaceTracker homeSurfaceTracker) {
        if (isIncognito || !shouldShowNtpHomeSurfaceOnStartup) {
            return;
        }

        int index = currentTabModel.index();
        Tab lastActiveTab = TabModelUtils.getCurrentTab(currentTabModel);
        // Early exits if there isn't any Tab, i.e., don't create a home surface.
        if (lastActiveTab == null) return;

        // If the last active Tab is a NTP, we continue to show this NTP as it is now.
        if (UrlUtilities.isNTPUrl(lastActiveTab.getUrl())) {
            if (!homeSurfaceTracker.isHomeSurfaceTab(lastActiveTab)) {
                homeSurfaceTracker.updateHomeSurfaceAndTrackingTabs(lastActiveTab, null);
            }
        } else {
            int indexOfFirstNtp =
                    TabModelUtils.getTabIndexByUrl(currentTabModel, UrlConstants.NTP_URL);
            if (indexOfFirstNtp != TabModel.INVALID_TAB_INDEX) {
                Tab ntpTab = currentTabModel.getTabAt(indexOfFirstNtp);
                assert indexOfFirstNtp != index;
                boolean isNtpUrl = UrlUtilities.isNTPUrl(ntpTab.getUrl());
                assert isNtpUrl
                    : "The URL of the first NTP found onResume doesn't match a NTP URL!";
                if (!isNtpUrl) {
                    recordFailToShowHomeSurfaceReasonUma(
                            FailToShowHomeSurfaceReason.FAIL_TO_FIND_NTP_TAB);
                    return;
                }

                // Sets the found NTP as home surface.
                TabModelUtils.setIndex(currentTabModel, indexOfFirstNtp, false);
                showHomeSurfaceUiOnNtp(ntpTab, lastActiveTab, homeSurfaceTracker);
            } else {
                // There isn't any existing NTP, create one.
                createNewTabAndShowHomeSurfaceUi(
                        tabCreator, homeSurfaceTracker, null, null, lastActiveTab);
            }
        }

        recordHomeSurfaceShownAtStartup();
        recordHomeSurfaceShown();
    }

    /*
     * Computes a return time from the result of the segmentation platform and stores to prefs.
     */
    public static void cacheReturnTimeFromSegmentation() {
        SegmentationPlatformService segmentationPlatformService =
                SegmentationPlatformServiceFactory.getForProfile(
                        Profile.getLastUsedRegularProfile());

        segmentationPlatformService.getSelectedSegment(START_V2_SEGMENTATION_PLATFORM_KEY,
                result -> { cacheReturnTimeFromSegmentationImpl(result); });
    }

    @VisibleForTesting
    public static void cacheReturnTimeFromSegmentationImpl(SegmentSelectionResult result) {
        long returnTimeMs =
                StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS.getDefaultValue()
                * DateUtils.SECOND_IN_MILLIS;
        if (result.isReady) {
            if (result.selectedSegment
                    != SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID_V2) {
                // If selected segment is not Start, then don't show.
                returnTimeMs = -1;
            } else {
                // The value of result.rank is in the unit of seconds.
                assert result.rank >= 0;
                // Converts to milliseconds.
                returnTimeMs = result.rank.longValue() * DateUtils.SECOND_IN_MILLIS;
            }
        }
        SharedPreferencesManager.getInstance().writeLong(
                ChromePreferenceKeys.START_RETURN_TIME_SEGMENTATION_RESULT_MS, returnTimeMs);
    }

    /**
     * Called when Start surface is shown at startup.
     */
    public static void recordHistogramsWhenOverviewIsShownAtLaunch() {
        // Records whether the last visited tab shown in the single tab switcher or carousel tab
        // switcher is a search result page or not.
        RecordHistogram.recordBooleanHistogram(
                LAST_VISITED_TAB_IS_SRP_WHEN_OVERVIEW_IS_SHOWN_AT_LAUNCH_UMA,
                SharedPreferencesManager.getInstance().readBoolean(
                        ChromePreferenceKeys.IS_LAST_VISITED_TAB_SRP, false));

        // Records whether the last active tab from tab restore is a NTP.
        RecordHistogram.recordBooleanHistogram(
                LAST_ACTIVE_TAB_IS_NTP_WHEN_OVERVIEW_IS_SHOWN_AT_LAUNCH_UMA,
                SharedPreferencesManager.getInstance().readInt(
                        ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE)
                        == ActiveTabState.NTP);
    }

    /**
     * Add an observer to keep {@link ChromePreferenceKeys#FEED_ARTICLES_LIST_VISIBLE} consistent
     * with {@link Pref#ARTICLES_LIST_VISIBLE}.
     */
    public static void addFeedVisibilityObserver() {
        updateFeedVisibility();
        PrefChangeRegistrar prefChangeRegistrar = new PrefChangeRegistrar();
        prefChangeRegistrar.addObserver(
                Pref.ARTICLES_LIST_VISIBLE, ReturnToChromeUtil::updateFeedVisibility);
    }

    private static void updateFeedVisibility() {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE,
                FeedFeatures.isFeedEnabled()
                        && UserPrefs.get(Profile.getLastUsedRegularProfile())
                                   .getBoolean(Pref.ARTICLES_LIST_VISIBLE));
    }

    /**
     * @return Whether the Feed articles are visible.
     */
    public static boolean getFeedArticlesVisibility() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.FEED_ARTICLES_LIST_VISIBLE, true);
    }

    /**
     * Returns whether to improve Start surface when Feed is not visible.
     */
    public static boolean shouldImproveStartWhenFeedIsDisabled(Context context) {
        return ChromeFeatureList.sStartSurfaceDisabledFeedImprovement.isEnabled()
                && !getFeedArticlesVisibility() && isStartSurfaceEnabled(context);
    }

    /**
     * Returns whether to move logo out of toolbar from Start surface.
     */
    public static boolean moveDownLogo() {
        return ChromeFeatureList.sSurfacePolish.isEnabled()
                && StartSurfaceConfiguration.SURFACE_POLISH_MOVE_DOWN_LOGO.getValue();
    }

    /**
     * Returns true if START_SURFACE_REFACTOR is enabled.
     */
    public static boolean isStartSurfaceRefactorEnabled(Context context) {
        return ChromeFeatureList.sStartSurfaceRefactor.isEnabled();
    }

    /**
     * Records a user action that Start surface is showing due to tapping the back button.
     * @param from: Where the back navigation is initiated, either "FromTab" or "FromTabSwitcher".
     */
    public static void recordBackNavigationToStart(String from) {
        RecordUserAction.record(SHOWN_FROM_BACK_NAVIGATION_UMA + from);
    }

    /**
     * Records the StartSurfaceState when overview page is shown.
     * @param state: the current StartSurfaceState.
     */
    public static void recordStartSurfaceState(@StartSurfaceState int state) {
        RecordHistogram.recordEnumeratedHistogram(
                START_SHOW_STATE_UMA, state, StartSurfaceState.NUM_ENTRIES);
    }

    public static void setSkipInitializationCheckForTesting(boolean skipInitializationCheck) {
        sSkipInitializationCheckForTesting = skipInitializationCheck;
        ResettersForTesting.register(() -> sSkipInitializationCheckForTesting = false);
    }

    /**
     * Records user clicks on the tab switcher button in New tab page or Start surface.
     * @param isInOverview Whether the current tab is in overview mode.
     * @param currentTab Current tab or null if none exists.
     */
    public static void recordClickTabSwitcher(boolean isInOverview, @Nullable Tab currentTab) {
        if (isInOverview) {
            BrowserUiUtils.recordModuleClickHistogram(HostSurface.START_SURFACE,
                    BrowserUiUtils.ModuleTypeOnStartAndNTP.TAB_SWITCHER_BUTTON);
        } else if (currentTab != null && !currentTab.isIncognito()
                && UrlUtilities.isNTPUrl(currentTab.getUrl())) {
            BrowserUiUtils.recordModuleClickHistogram(HostSurface.NEW_TAB_PAGE,
                    BrowserUiUtils.ModuleTypeOnStartAndNTP.TAB_SWITCHER_BUTTON);
        }
    }

    /**
     * Recorded when the home surface NTP is shown at startup.
     */
    public static void recordHomeSurfaceShownAtStartup() {
        RecordHistogram.recordBooleanHistogram(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true);
    }

    /**
     * Records the home surface shown impressions.
     */
    public static void recordHomeSurfaceShown() {
        RecordHistogram.recordBooleanHistogram(HOME_SURFACE_SHOWN_UMA, true);
    }

    /**
     * Shows the home surface UI on the given Ntp on tablets.
     */
    static void showHomeSurfaceUiOnNtp(
            Tab ntpTab, Tab lastActiveTab, HomeSurfaceTracker homeSurfaceTracker) {
        NativePage nativePage = ntpTab.getNativePage();
        if (nativePage == null) {
            recordFailToShowHomeSurfaceReasonUma(FailToShowHomeSurfaceReason.NOT_A_NATIVE_PAGE);
            return;
        }

        // It is possible to get null after casting ntpTab.getNativePage() to NewTabPage, early
        // exit here. See https://crbug.com/1449900.
        if (!(nativePage instanceof NewTabPage)) {
            recordFailToShowHomeSurfaceReasonUma(FailToShowHomeSurfaceReason.NOT_A_NTP_NATIVE_PAGE);
            if (nativePage.isFrozen()) {
                recordFailToShowHomeSurfaceReasonUma(
                        FailToShowHomeSurfaceReason.NATIVE_PAGE_IS_FROZEN);
            }
            return;
        }

        // This cast is now guaranteed to succeed to a non-null value.
        NewTabPage newTabPage = (NewTabPage) nativePage;
        homeSurfaceTracker.updateHomeSurfaceAndTrackingTabs(ntpTab, lastActiveTab);
        newTabPage.showHomeSurfaceUi(lastActiveTab);
    }

    // TODO(https://crbug.com/1450578): Removes this histogram once we understand the root cause of
    // the crash.
    private static void recordFailToShowHomeSurfaceReasonUma(
            @FailToShowHomeSurfaceReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                FAIL_TO_SHOW_HOME_SURFACE_UI_UMA, reason, FailToShowHomeSurfaceReason.NUM_ENTRIES);
    }
}
