// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.ReturnToChromeUtil.FAIL_TO_SHOW_HOME_SURFACE_UI_UMA;
import static org.chromium.chrome.browser.tasks.ReturnToChromeUtil.HOME_SURFACE_SHOWN_AT_STARTUP_UMA;
import static org.chromium.chrome.browser.tasks.ReturnToChromeUtil.HOME_SURFACE_SHOWN_UMA;
import static org.chromium.chrome.browser.ui.fold_transitions.FoldTransitionController.RESUME_HOME_SURFACE_ON_MODE_CHANGE;
import static org.chromium.chrome.features.start_surface.StartSurfaceConfiguration.START_SURFACE_OPEN_START_AS_HOMEPAGE;
import static org.chromium.chrome.features.start_surface.StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS;
import static org.chromium.chrome.features.start_surface.StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS;
import static org.chromium.chrome.features.start_surface.StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_USE_MODEL;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.os.Bundle;
import android.text.format.DateUtils;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.BaseSwitches;
import org.chromium.base.IntentUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil.FailToShowHomeSurfaceReason;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtilUnitTest.ShadowHomepageManager;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtilUnitTest.ShadowHomepagePolicyManager;
import org.chromium.chrome.browser.ui.fold_transitions.FoldTransitionController;
import org.chromium.chrome.browser.ui.native_page.FrozenNativePage;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.segmentation_platform.ClassificationResult;
import org.chromium.components.segmentation_platform.prediction_status.PredictionStatus;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ReturnToChromeUtil} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowHomepageManager.class, ShadowHomepagePolicyManager.class})
@CommandLineFlags.Add({BaseSwitches.DISABLE_LOW_END_DEVICE_MODE})
@DisableFeatures({ChromeFeatureList.SHOW_NTP_AT_STARTUP_ANDROID})
public class ReturnToChromeUtilUnitTest {
    /** Shadow for {@link HomepageManager}. */
    @Implements(HomepageManager.class)
    static class ShadowHomepageManager {
        static GURL sHomepageGurl;
        static boolean sIsHomepageEnabled;

        @Implementation
        public static boolean isHomepageEnabled() {
            return sIsHomepageEnabled;
        }

        @Implementation
        public static GURL getHomepageGurl() {
            return sHomepageGurl;
        }
    }

    @Implements(HomepagePolicyManager.class)
    static class ShadowHomepagePolicyManager {
        static boolean sIsInitialized;

        @Implementation
        public static boolean isInitializedWithNative() {
            return sIsInitialized;
        }
    }

    private static final int ON_RETURN_THRESHOLD_SECOND = 1000;
    private static final int DELTA_MS = 100;

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private Context mContext;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ChromeInactivityTracker mInactivityTracker;
    @Mock private Resources mResources;
    @Mock private TabModel mCurrentTabModel;
    @Mock private TabCreator mTabCreater;
    @Mock private Tab mTab1;
    @Mock private Tab mNtpTab;
    @Mock private NewTabPage mNewTabPage;
    @Mock private HomeSurfaceTracker mHomeSurfaceTracker;
    @Mock private Bundle mSaveInstanceState;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(JUnitTestGURLs.NTP_NATIVE_URL).when(mNtpTab).getUrl();

        ChromeFeatureList.sStartSurfaceAndroid.setForTesting(true);

        // HomepageManager:
        ShadowHomepageManager.sHomepageGurl = UrlConstants.ntpGurl();
        ShadowHomepageManager.sIsHomepageEnabled = true;
        Assert.assertEquals(UrlConstants.ntpGurl(), HomepageManager.getHomepageGurl());
        Assert.assertTrue(HomepageManager.isHomepageEnabled());

        ShadowHomepagePolicyManager.sIsInitialized = true;
        Assert.assertTrue(HomepagePolicyManager.isInitializedWithNative());

        // Low end devices:
        Assert.assertFalse(SysUtils.isLowEndDevice());

        // Sets accessibility:
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);

        // Sets for phones, i.e., !DeviceFormFactor.isNonMultiDisplayContextOnTablet():
        doReturn(mResources).when(mContext).getResources();
        doReturn(DeviceFormFactor.SCREEN_BUCKET_TABLET - 1)
                .when(mResources)
                .getInteger(org.chromium.ui.R.integer.min_screen_width_bucket);
        Assert.assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));
    }

    @Test
    @SmallTest
    public void testShouldShowTabSwitcher() {
        Assert.assertEquals(
                START_SURFACE_RETURN_TIME_SECONDS.getDefaultValue(),
                START_SURFACE_RETURN_TIME_SECONDS.getValue());

        long returnTimeMs =
                START_SURFACE_RETURN_TIME_SECONDS.getValue() * DateUtils.SECOND_IN_MILLIS;
        // When return time doesn't arrive, return false:
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowTabSwitcher(
                        System.currentTimeMillis() - returnTimeMs + DELTA_MS, false));

        // When return time arrives, return true:
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowTabSwitcher(
                        System.currentTimeMillis() - returnTimeMs - 1, false));
    }

    @Test
    @SmallTest
    public void testShouldShowTabSwitcherOnMixPhoneAndTabletMode() {
        Assert.assertEquals(
                START_SURFACE_RETURN_TIME_SECONDS.getDefaultValue(),
                START_SURFACE_RETURN_TIME_SECONDS.getValue());
        Assert.assertEquals(
                START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS.getDefaultValue(),
                START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS.getValue());

        int updatedReturnTimeMs = 1;
        // Sets the return time on phones arrived.
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(updatedReturnTimeMs);
        Assert.assertEquals(updatedReturnTimeMs, START_SURFACE_RETURN_TIME_SECONDS.getValue());
        long returnTimeMs = updatedReturnTimeMs * DateUtils.SECOND_IN_MILLIS;
        // When return time on phones arrives, return true on phones:
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowTabSwitcher(
                        System.currentTimeMillis() - returnTimeMs - 1, false));
        // Verifies that return time on phones doesn't impact the return time on tablets.
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowTabSwitcher(
                        System.currentTimeMillis() - returnTimeMs - 1, true));

        // Sets the return time on tablets arrived, while resets the one of phones.
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(
                START_SURFACE_RETURN_TIME_SECONDS.getDefaultValue());
        START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS.setForTesting(updatedReturnTimeMs);
        Assert.assertEquals(
                updatedReturnTimeMs, START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS.getValue());
        // When return time on tablets arrives, return true on tablets:
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowTabSwitcher(
                        System.currentTimeMillis() - returnTimeMs - 1, true));
        // Verifies that return time on tablets doesn't impact the return time on phones.
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowTabSwitcher(
                        System.currentTimeMillis() - returnTimeMs - 1, false));
    }

    @Test
    @SmallTest
    public void testShouldShowTabSwitcherWithStartReturnTimeWithoutUseModel() {
        Assert.assertFalse(START_SURFACE_RETURN_TIME_USE_MODEL.getValue());

        // Set to not shown.
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(-1);
        Assert.assertEquals(-1, START_SURFACE_RETURN_TIME_SECONDS.getValue());
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowTabSwitcher(
                        System.currentTimeMillis() - DELTA_MS, false));

        // Sets to immediate return.
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        Assert.assertEquals(0, START_SURFACE_RETURN_TIME_SECONDS.getValue());
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowTabSwitcher(System.currentTimeMillis(), false));

        // Sets to an random time.
        int expectedReturnTimeSeconds = 60; // one minute
        int expectedReturnTimeMs = (int) (expectedReturnTimeSeconds * DateUtils.SECOND_IN_MILLIS);
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(expectedReturnTimeSeconds);
        Assert.assertEquals(
                expectedReturnTimeSeconds, START_SURFACE_RETURN_TIME_SECONDS.getValue());
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowTabSwitcher(
                        System.currentTimeMillis() - expectedReturnTimeMs, false));
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowTabSwitcher(
                        System.currentTimeMillis() - expectedReturnTimeSeconds, false));
    }

    @Test
    @SmallTest
    public void testShouldShowTabSwitcherWithSegmentationReturnTime() {
        // Verifies that when the preference key isn't stored, return
        // START_SURFACE_RETURN_TIME_SECONDS.getDefaultValue() as default value, i.e., 8 hours.
        Assert.assertEquals(
                START_SURFACE_RETURN_TIME_SECONDS.getDefaultValue(),
                ReturnToChromeUtil.getReturnTimeFromSegmentation(
                        START_SURFACE_RETURN_TIME_SECONDS));

        // Verifies returning false if both flags haven't been set any value or any meaningful yet.
        START_SURFACE_RETURN_TIME_USE_MODEL.setForTesting(true);
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));

        // Return time from segmentation model is enabled for 1 min:
        long returnTimeSeconds = 60; // One minute
        long returnTimeMs = returnTimeSeconds * DateUtils.SECOND_IN_MILLIS; // One minute
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        ClassificationResult result =
                new ClassificationResult(
                        PredictionStatus.SUCCEEDED,
                        new String[] {String.valueOf(returnTimeSeconds)});
        ReturnToChromeUtil.cacheReturnTimeFromSegmentationImpl(result);
        Assert.assertEquals(
                returnTimeMs,
                ReturnToChromeUtil.getReturnTimeFromSegmentation(
                        START_SURFACE_RETURN_TIME_SECONDS));

        // Returns false if it isn't immediate return but without last backgrounded time available:
        result = new ClassificationResult(PredictionStatus.SUCCEEDED, new String[] {"1"});
        ReturnToChromeUtil.cacheReturnTimeFromSegmentationImpl(result);
        Assert.assertEquals(
                1 * DateUtils.SECOND_IN_MILLIS,
                ReturnToChromeUtil.getReturnTimeFromSegmentation(
                        START_SURFACE_RETURN_TIME_SECONDS));
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));

        // Verifies returning false if segmentation result is negative (not show).
        result = new ClassificationResult(PredictionStatus.NOT_READY, null);
        ReturnToChromeUtil.cacheReturnTimeFromSegmentationImpl(result);
        Assert.assertEquals(
                -1,
                ReturnToChromeUtil.getReturnTimeFromSegmentation(
                        START_SURFACE_RETURN_TIME_SECONDS));
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(1, false));

        // Tests regular cases with last backgrounded time set:
        result =
                new ClassificationResult(
                        PredictionStatus.SUCCEEDED,
                        new String[] {String.valueOf(returnTimeSeconds)});
        ReturnToChromeUtil.cacheReturnTimeFromSegmentationImpl(result);
        Assert.assertEquals(
                returnTimeMs,
                ReturnToChromeUtil.getReturnTimeFromSegmentation(
                        START_SURFACE_RETURN_TIME_SECONDS));

        int doubleReturnTimeMs = (int) (2 * returnTimeMs); // Two minutes
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(doubleReturnTimeMs);
        Assert.assertEquals(doubleReturnTimeMs, START_SURFACE_RETURN_TIME_SECONDS.getValue());

        // When segmentation platform's return time arrives, return true:
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowTabSwitcher(
                        System.currentTimeMillis() - returnTimeMs - 1, false));

        // When segmentation platform's return times hasn't arrived, return false:
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowTabSwitcher(System.currentTimeMillis(), false));

        // Clean up.
        sharedPreferencesManager.removeKey(
                ChromePreferenceKeys.START_RETURN_TIME_SEGMENTATION_RESULT_MS);
    }

    @Test
    @SmallTest
    public void testShouldShowStartSurfaceAsTheHomePageUseVisibleTime() {
        START_SURFACE_OPEN_START_AS_HOMEPAGE.setForTesting(true);
        Assert.assertTrue(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));

        // Sets main intent from launcher:
        Intent intent = createMainIntentFromLauncher();
        Assert.assertTrue(IntentUtils.isMainIntentFromLauncher(intent));
        // Tests the case when the total tab count > 0:
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(ON_RETURN_THRESHOLD_SECOND);

        long currentTime = System.currentTimeMillis();
        long returnTimeMS = ON_RETURN_THRESHOLD_SECOND * DateUtils.SECOND_IN_MILLIS;
        long expectedVisibleTime = currentTime - returnTimeMS - 1; // has reached
        long expectedLastBackgroundTime = -1;
        doReturn(expectedVisibleTime).when(mInactivityTracker).getLastVisibleTimeMs();
        doReturn(expectedLastBackgroundTime).when(mInactivityTracker).getLastBackgroundedTimeMs();

        // Verifies that Start will show if the threshold of return time has reached using last
        // visible time, while last background time is lost or not set.
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(expectedVisibleTime, false));
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowTabSwitcher(expectedLastBackgroundTime, false));
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                        mContext,
                        intent,
                        mTabModelSelector,
                        mInactivityTracker,
                        /* isTablet= */ false));

        // Verifies that Start will NOT show if the threshold of return time hasn't reached using
        // last visible time, while last background time is lost or not set.
        currentTime = System.currentTimeMillis();
        expectedVisibleTime = currentTime - returnTimeMS + DELTA_MS; // doesn't reach
        doReturn(expectedVisibleTime).when(mInactivityTracker).getLastVisibleTimeMs();
        doReturn(expectedLastBackgroundTime).when(mInactivityTracker).getLastBackgroundedTimeMs();
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(expectedVisibleTime, false));
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowTabSwitcher(expectedLastBackgroundTime, false));
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                        mContext,
                        intent,
                        mTabModelSelector,
                        mInactivityTracker,
                        /* isTablet= */ false));

        // Verifies that Start will NOT show if the threshold of return time has reached using
        // last visible time, while hasn't using the last background time which is the max time.
        currentTime = System.currentTimeMillis();
        expectedVisibleTime = currentTime - returnTimeMS - 1; // has reached
        expectedLastBackgroundTime = currentTime - returnTimeMS + DELTA_MS; // doesn't reach
        doReturn(expectedVisibleTime).when(mInactivityTracker).getLastVisibleTimeMs();
        doReturn(expectedLastBackgroundTime).when(mInactivityTracker).getLastBackgroundedTimeMs();
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(expectedVisibleTime, false));
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowTabSwitcher(expectedLastBackgroundTime, false));
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                        mContext,
                        intent,
                        mTabModelSelector,
                        mInactivityTracker,
                        /* isTablet= */ false));

        // Verifies that Start will show if the threshold of both return time has reached using
        // either last visible time or the last background time.
        currentTime = System.currentTimeMillis();
        expectedVisibleTime = currentTime - returnTimeMS - 2; // has reached
        expectedLastBackgroundTime = currentTime - returnTimeMS - 1; // has reached
        doReturn(expectedVisibleTime).when(mInactivityTracker).getLastVisibleTimeMs();
        doReturn(expectedLastBackgroundTime).when(mInactivityTracker).getLastBackgroundedTimeMs();
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(expectedVisibleTime, false));
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowTabSwitcher(expectedLastBackgroundTime, false));
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                        mContext,
                        intent,
                        mTabModelSelector,
                        mInactivityTracker,
                        /* isTablet= */ false));
    }

    @Test
    @SmallTest
    public void testShouldShowStartSurfaceAtStartupWithDefaultChromeHomepage() {
        START_SURFACE_OPEN_START_AS_HOMEPAGE.setForTesting(true);
        Assert.assertTrue(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));

        // Sets main intent from launcher:
        Intent intent = createMainIntentFromLauncher();

        // Sets background time to not show Start:
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(-1);
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(0, false));

        // Tests the case when there isn't any Tab:
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(0).when(mTabModelSelector).getTotalTabCount();
        Assert.assertTrue(HomepagePolicyManager.isInitializedWithNative());
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                        mContext,
                        intent,
                        mTabModelSelector,
                        mInactivityTracker,
                        /* isTablet= */ false));

        // Tests the case when the total tab count > 0:
        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                        mContext,
                        intent,
                        mTabModelSelector,
                        mInactivityTracker,
                        /* isTablet= */ false));

        // Sets background time to make the return time arrive:
        ChromeSharedPreferences.getInstance()
                .addToStringSet(
                        ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF, "0");
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(0, false));

        // Verifies that Start will show since the return time has arrived.
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                        mContext,
                        intent,
                        mTabModelSelector,
                        mInactivityTracker,
                        /* isTablet= */ false));

        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF);
    }

    @Test
    @SmallTest
    public void testShouldShowStartSurfaceWithCustomizedHomePage() {
        START_SURFACE_OPEN_START_AS_HOMEPAGE.setForTesting(true);
        Assert.assertTrue(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));

        // Sets a customized homepage:
        ShadowHomepageManager.sHomepageGurl = new GURL("http://foo.com");
        Assert.assertFalse(ReturnToChromeUtil.useChromeHomepage());

        // Sets main intent from launcher:
        Intent intent = createMainIntentFromLauncher();

        // Sets background time to make the return time arrive:
        ChromeSharedPreferences.getInstance()
                .addToStringSet(
                        ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF, "0");
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(0, false));

        // Tests the case when there isn't any Tab but with customized homepage:
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(0).when(mTabModelSelector).getTotalTabCount();
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                        mContext,
                        intent,
                        mTabModelSelector,
                        mInactivityTracker,
                        /* isTablet= */ false));

        // Tests the case when the total tab count > 0 and return time arrives, Start will show.
        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                        mContext,
                        intent,
                        mTabModelSelector,
                        mInactivityTracker,
                        /* isTablet= */ false));

        ShadowHomepageManager.sHomepageGurl = UrlConstants.ntpGurl();
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF);
    }

    @Test
    @SmallTest
    public void testShouldShowStartSurfaceAtStartupWithHomepageDisabled() {
        START_SURFACE_OPEN_START_AS_HOMEPAGE.setForTesting(true);
        Assert.assertTrue(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));

        // Sets main intent from launcher:
        Intent intent = createMainIntentFromLauncher();

        // Sets background time to make the return time arrive:
        ChromeSharedPreferences.getInstance()
                .addToStringSet(
                        ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF, "0");
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(0, false));

        // When homepage is disabled, verifies that Start isn't shown when there isn't any Tab, even
        // if the return time has arrived.
        ShadowHomepageManager.sIsHomepageEnabled = false;
        Assert.assertFalse(HomepageManager.isHomepageEnabled());
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(0).when(mTabModelSelector).getTotalTabCount();
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                        mContext,
                        intent,
                        mTabModelSelector,
                        mInactivityTracker,
                        /* isTablet= */ false));
    }

    @Test
    @SmallTest
    public void testStartSurfaceIsDisabledOnTablet() {
        // Sets for !DeviceFormFactor.isNonMultiDisplayContextOnTablet()
        setupAndVerifyTablets();

        Assert.assertFalse(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.NEW_TAB_SEARCH_ENGINE_URL_ANDROID})
    public void testStartSurfaceIsEnabledWithNewTabSearchEngineUrlDisabled() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.IS_DSE_GOOGLE, false);

        Assert.assertTrue(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.IS_DSE_GOOGLE);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.SHOW_NTP_AT_STARTUP_ANDROID})
    public void testStartSurfaceIsDisabledWithShowNtpAtStartup() {
        Assert.assertTrue(ChromeFeatureList.sShowNtpAtStartupAndroid.isEnabled());
        Assert.assertFalse(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.NEW_TAB_SEARCH_ENGINE_URL_ANDROID})
    public void testStartSurfaceMayBeDisabledWithNewTabSearchEngineUrlEnabled() {
        Assert.assertTrue(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));

        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.IS_DSE_GOOGLE, false);
        Assert.assertFalse(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));

        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.IS_DSE_GOOGLE);
    }

    @Test
    @SmallTest
    public void testShouldNotShowStartSurfaceOnStartWhenHomepagePolicyManagerIsNotInitialized() {
        START_SURFACE_OPEN_START_AS_HOMEPAGE.setForTesting(true);
        Assert.assertTrue(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));
        ShadowHomepagePolicyManager.sIsInitialized = false;
        Assert.assertFalse(HomepagePolicyManager.isInitializedWithNative());

        // Sets main intent from launcher:
        Intent intent = createMainIntentFromLauncher();

        // Sets background time to make the return time arrive:
        ChromeSharedPreferences.getInstance()
                .addToStringSet(
                        ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF, "0");
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(0, false));

        // Tests the case when there isn't any Tab. Verifies that Start isn't shown if
        // HomepagePolicyManager isn't initialized.
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(0).when(mTabModelSelector).getTotalTabCount();
        Assert.assertTrue(HomepageManager.isHomepageEnabled());
        Assert.assertFalse(ReturnToChromeUtil.useChromeHomepage());
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                        mContext,
                        intent,
                        mTabModelSelector,
                        mInactivityTracker,
                        /* isTablet= */ false));
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Startup.Android.IsHomepagePolicyManagerInitialized"));

        // Tests the case when the total tab count > 0. Verifies that Start is shown when the return
        // time arrives.
        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                        mContext,
                        intent,
                        mTabModelSelector,
                        mInactivityTracker,
                        /* isTablet= */ false));
        // Verifies that we don't record the histogram again.
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Startup.Android.IsHomepagePolicyManagerInitialized"));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.START_SURFACE_ON_TABLET)
    public void testShouldShowNtpAsHomeSurfaceAtStartupOnTablet() {
        Assert.assertTrue(StartSurfaceConfiguration.isNtpAsHomeSurfaceEnabled(true));

        // Sets main intent from launcher:
        Intent intent = createMainIntentFromLauncher();

        // Sets background time to make the return time arrive:
        ChromeSharedPreferences.getInstance()
                .addToStringSet(
                        ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF, "0");
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(0, false));

        // Tests the case when there isn't any Tab. Verifies that Start is only shown on tablets.
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(0).when(mTabModelSelector).getTotalTabCount();
        Assert.assertTrue(HomepagePolicyManager.isInitializedWithNative());
        Assert.assertTrue(HomepageManager.isHomepageEnabled());
        Assert.assertTrue(IntentUtils.isMainIntentFromLauncher(intent));
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowNtpAsHomeSurfaceAtStartup(
                        false, intent, null, mTabModelSelector, mInactivityTracker));
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowNtpAsHomeSurfaceAtStartup(
                        true, intent, null, mTabModelSelector, mInactivityTracker));

        // Tests the case when the total tab count > 0. Verifies that Start is only shown on
        // tablets.
        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowNtpAsHomeSurfaceAtStartup(
                        false, intent, null, mTabModelSelector, mInactivityTracker));
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowNtpAsHomeSurfaceAtStartup(
                        true, intent, null, mTabModelSelector, mInactivityTracker));

        // Sets the return time not arrive.
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(-1);
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));
        doReturn(true)
                .when(mSaveInstanceState)
                .getBoolean(FoldTransitionController.DID_CHANGE_TABLET_MODE, false);
        doReturn(true)
                .when(mSaveInstanceState)
                .getBoolean(RESUME_HOME_SURFACE_ON_MODE_CHANGE, false);
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowNtpAsHomeSurfaceAtStartup(
                        false, intent, null, mTabModelSelector, mInactivityTracker));
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowNtpAsHomeSurfaceAtStartup(
                        false, intent, mSaveInstanceState, mTabModelSelector, mInactivityTracker));
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowNtpAsHomeSurfaceAtStartup(
                        false, intent, null, mTabModelSelector, mInactivityTracker));
        Assert.assertTrue(
                ReturnToChromeUtil.shouldShowNtpAsHomeSurfaceAtStartup(
                        true, intent, mSaveInstanceState, mTabModelSelector, mInactivityTracker));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.START_SURFACE_ON_TABLET)
    public void testShowNtpAsHomeSurfaceAtResumeOnTabletWithExistingNtp() {
        Assert.assertTrue(StartSurfaceConfiguration.isNtpAsHomeSurfaceEnabled(true));

        doReturn(2).when(mCurrentTabModel).getCount();
        doReturn(JUnitTestGURLs.URL_1).when(mTab1).getUrl();
        doReturn(mTab1).when(mCurrentTabModel).getTabAt(0);

        doReturn(true).when(mNtpTab).isNativePage();
        doReturn(mNewTabPage).when(mNtpTab).getNativePage();
        doReturn(mNtpTab).when(mCurrentTabModel).getTabAt(1);

        // Sets the NTP is the last active Tab.
        doReturn(1).when(mCurrentTabModel).index();

        // Tests case of the last active NTP has home surface UI.
        doReturn(true).when(mHomeSurfaceTracker).canShowHomeSurface(mNtpTab);
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectIntRecords(HOME_SURFACE_SHOWN_UMA, 1)
                        .build();

        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        verify(mTabCreater, never()).createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null));
        verify(mCurrentTabModel, never())
                .setIndex(anyInt(), eq(TabSelectionType.FROM_USER), eq(false));
        verify(mNewTabPage, never()).showHomeSurfaceUi(any());
        verify(mHomeSurfaceTracker).updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), eq(null));
        histogram.assertExpected();

        // Tests the case of the last active NTP doesn't has home surface UI.
        doReturn(false).when(mHomeSurfaceTracker).canShowHomeSurface(mNtpTab);
        histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectIntRecords(HOME_SURFACE_SHOWN_UMA, 1)
                        .build();
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        verify(mHomeSurfaceTracker, times(2))
                .updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), eq(null));
        histogram.assertExpected();

        // Sets the last active Tab isn't a NTP.
        doReturn(0).when(mCurrentTabModel).index();

        // Verifies that if the NTP isn't the last active Tab, we reuse it, set index and call
        // showHomeSurfaceUi() to show the single tab card module.
        histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectIntRecords(HOME_SURFACE_SHOWN_UMA, 1)
                        .build();
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        verify(mTabCreater, never()).createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null));
        verify(mCurrentTabModel).setIndex(eq(1), eq(TabSelectionType.FROM_USER), eq(false));
        verify(mNewTabPage).showHomeSurfaceUi(eq(mTab1));
        verify(mHomeSurfaceTracker).updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), eq(mTab1));
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.START_SURFACE_ON_TABLET)
    public void testShowNtpAsHomeSurfaceAtResumeOnTabletWithoutAnyExistingNtp() {
        Assert.assertTrue(StartSurfaceConfiguration.isNtpAsHomeSurfaceEnabled(true));

        doReturn(1).when(mCurrentTabModel).getCount();
        doReturn(JUnitTestGURLs.URL_1).when(mTab1).getUrl();
        doReturn(mTab1).when(mCurrentTabModel).getTabAt(0);

        // Verifies that if the return time doesn't arrive, there isn't a new NTP is created.
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ false,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        verify(mTabCreater, never()).createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null));
        verify(mHomeSurfaceTracker, never()).updateHomeSurfaceAndTrackingTabs(any(), any());

        // Verifies that a new NTP is created when there isn't any existing one to reuse.
        doReturn(2).when(mNtpTab).getId();
        doReturn(true).when(mNtpTab).isNativePage();
        doReturn(mNewTabPage).when(mNtpTab).getNativePage();
        doReturn(mNtpTab)
                .when(mTabCreater)
                .createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null));
        doReturn(0).when(mCurrentTabModel).index();

        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_UMA, true)
                        .build();
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        verify(mTabCreater, times(1)).createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null));
        verify(mNewTabPage).showHomeSurfaceUi(eq(mTab1));
        verify(mHomeSurfaceTracker).updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), eq(mTab1));
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.START_SURFACE_ON_TABLET)
    public void testShowNtpAsHomeSurfaceAtResumeOnTabletWithMixedNtps() {
        Assert.assertTrue(StartSurfaceConfiguration.isNtpAsHomeSurfaceEnabled(true));

        doReturn(3).when(mCurrentTabModel).getCount();
        doReturn(JUnitTestGURLs.URL_1).when(mTab1).getUrl();
        doReturn(mTab1).when(mCurrentTabModel).getTabAt(0);

        doReturn(JUnitTestGURLs.NTP_NATIVE_URL).when(mNtpTab).getUrl();
        doReturn(true).when(mNtpTab).isNativePage();
        doReturn(mNewTabPage).when(mNtpTab).getNativePage();
        doReturn(mNtpTab).when(mCurrentTabModel).getTabAt(1);

        Tab activeNtpTab = Mockito.mock(Tab.class);
        NewTabPage activeNtp = Mockito.mock(NewTabPage.class);
        doReturn(JUnitTestGURLs.NTP_NATIVE_URL).when(activeNtpTab).getUrl();
        doReturn(true).when(activeNtpTab).isNativePage();
        doReturn(activeNtp).when(activeNtpTab).getNativePage();
        doReturn(activeNtpTab).when(mCurrentTabModel).getTabAt(2);

        // Set the active NTP tab as the last Tab, and has a tracking Tab.
        doReturn(2).when(mCurrentTabModel).index();
        doReturn(true).when(mHomeSurfaceTracker).canShowHomeSurface(activeNtpTab);

        // Verifies that the first found NTP isn't the active NTP Tab.
        Assert.assertEquals(
                1, TabModelUtils.getTabIndexByUrl(mCurrentTabModel, UrlConstants.NTP_URL));
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_UMA, true)
                        .build();
        // Verifies the active NTP will be shown with its home surface UI, not the first found NTP.
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        histogram.assertExpected();
        verify(mHomeSurfaceTracker, never()).updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), any());
        verify(mNewTabPage, never()).showHomeSurfaceUi(any());

        // Set the last active NTP doesn't have a tracking Tab.
        doReturn(false).when(mHomeSurfaceTracker).canShowHomeSurface(activeNtpTab);
        histogram =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_AT_STARTUP_UMA, true)
                        .expectBooleanRecord(HOME_SURFACE_SHOWN_UMA, true)
                        .build();
        // Verifies the active NTP will be shown as it is now, i.e., an empty NTP, not the first
        // found NTP.
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        histogram.assertExpected();
        verify(mHomeSurfaceTracker, never()).updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), any());
        verify(mNewTabPage, never()).showHomeSurfaceUi(any());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.START_SURFACE_ON_TABLET)
    public void testNoAnyTabCase() {
        Assert.assertTrue(StartSurfaceConfiguration.isNtpAsHomeSurfaceEnabled(true));

        doReturn(0).when(mCurrentTabModel).getCount();

        // Verifies that if there isn't any existing Tab, we don't create a home surface NTP.
        ReturnToChromeUtil.setInitialOverviewStateOnResumeWithNtp(
                false,
                /* shouldShowNtpHomeSurfaceOnStartup= */ true,
                mCurrentTabModel,
                mTabCreater,
                mHomeSurfaceTracker);
        verify(mTabCreater, never()).createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null));
        verify(mHomeSurfaceTracker, never()).updateHomeSurfaceAndTrackingTabs(any(), any());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.START_SURFACE_ON_TABLET)
    public void testColdStartupWithOnlyLastActiveTabUrl() {
        Assert.assertTrue(StartSurfaceConfiguration.isNtpAsHomeSurfaceEnabled(true));

        doReturn(JUnitTestGURLs.URL_1).when(mTab1).getUrl();
        doReturn(true).when(mNtpTab).isNativePage();
        doReturn(mNewTabPage).when(mNtpTab).getNativePage();
        doReturn(mNtpTab)
                .when(mTabCreater)
                .createNewTab(any(), eq(TabLaunchType.FROM_STARTUP), eq(null));
        doReturn(mCurrentTabModel).when(mTabModelSelector).getModel(false);

        // Tests the case that a new NTP is created and waits for its tracking last active Tab being
        // restored.
        ReturnToChromeUtil.createNewTabAndShowHomeSurfaceUi(
                mTabCreater,
                mHomeSurfaceTracker,
                mTabModelSelector,
                JUnitTestGURLs.URL_1.getSpec(),
                null);
        verify(mCurrentTabModel).addObserver(mTabModelObserverCaptor.capture());

        // Verifies if the added Tab matches the tracking URL, call showHomeSurfaceUi().
        mTabModelObserverCaptor.getValue().willAddTab(mTab1, TabLaunchType.FROM_RESTORE);
        verify(mNewTabPage).showHomeSurfaceUi(eq(mTab1));
        verify(mHomeSurfaceTracker).updateHomeSurfaceAndTrackingTabs(eq(mNtpTab), eq(mTab1));
    }

    @Test
    @SmallTest
    public void testShouldResumeHomeSurfaceOnFoldConfigurationChange() {
        Assert.assertFalse(
                ReturnToChromeUtil.shouldResumeHomeSurfaceOnFoldConfigurationChange(null));

        doReturn(true)
                .when(mSaveInstanceState)
                .getBoolean(FoldTransitionController.DID_CHANGE_TABLET_MODE, false);
        doReturn(false)
                .when(mSaveInstanceState)
                .getBoolean(RESUME_HOME_SURFACE_ON_MODE_CHANGE, false);
        Assert.assertFalse(
                ReturnToChromeUtil.shouldResumeHomeSurfaceOnFoldConfigurationChange(
                        mSaveInstanceState));

        doReturn(false)
                .when(mSaveInstanceState)
                .getBoolean(FoldTransitionController.DID_CHANGE_TABLET_MODE, false);
        doReturn(true)
                .when(mSaveInstanceState)
                .getBoolean(RESUME_HOME_SURFACE_ON_MODE_CHANGE, false);
        Assert.assertFalse(
                ReturnToChromeUtil.shouldResumeHomeSurfaceOnFoldConfigurationChange(
                        mSaveInstanceState));

        doReturn(true)
                .when(mSaveInstanceState)
                .getBoolean(FoldTransitionController.DID_CHANGE_TABLET_MODE, false);
        doReturn(true)
                .when(mSaveInstanceState)
                .getBoolean(RESUME_HOME_SURFACE_ON_MODE_CHANGE, false);
        Assert.assertTrue(
                ReturnToChromeUtil.shouldResumeHomeSurfaceOnFoldConfigurationChange(
                        mSaveInstanceState));
    }

    @Test
    @SmallTest
    public void testLogFailToShowHomeSurfaceUI() {
        HistogramWatcher histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                FAIL_TO_SHOW_HOME_SURFACE_UI_UMA,
                                FailToShowHomeSurfaceReason.NOT_A_NATIVE_PAGE)
                        .build();
        doReturn(null).when(mNtpTab).getNativePage();
        ReturnToChromeUtil.showHomeSurfaceUiOnNtp(mNtpTab, mTab1, mHomeSurfaceTracker);
        histogram.assertExpected();

        FrozenNativePage frozenNativePage = Mockito.mock(FrozenNativePage.class);
        doReturn(true).when(frozenNativePage).isFrozen();
        doReturn(frozenNativePage).when(mNtpTab).getNativePage();
        histogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                FAIL_TO_SHOW_HOME_SURFACE_UI_UMA,
                                FailToShowHomeSurfaceReason.NOT_A_NTP_NATIVE_PAGE)
                        .expectIntRecords(
                                FAIL_TO_SHOW_HOME_SURFACE_UI_UMA,
                                FailToShowHomeSurfaceReason.NATIVE_PAGE_IS_FROZEN)
                        .build();
        ReturnToChromeUtil.showHomeSurfaceUiOnNtp(mNtpTab, mTab1, mHomeSurfaceTracker);
        histogram.assertExpected();
    }

    @Test
    @SmallTest
    public void testShouldHandleTabSwitcherShown() {
        LayoutStateProvider layoutStateProvider = Mockito.mock(LayoutStateProvider.class);

        // Verifies ReturnToChromeUtil.shouldHandleTabSwitcherShown() returns false in all invalid
        // cases.
        Assert.assertFalse(
                ReturnToChromeUtil.shouldHandleTabSwitcherShown(false, layoutStateProvider));
        Assert.assertFalse(ReturnToChromeUtil.shouldHandleTabSwitcherShown(true, null));
        doReturn(false).when(layoutStateProvider).isLayoutVisible(eq(LayoutType.TAB_SWITCHER));
        Assert.assertFalse(
                ReturnToChromeUtil.shouldHandleTabSwitcherShown(true, layoutStateProvider));

        // Verifies ReturnToChromeUtil.shouldHandleTabSwitcherShown() returns true.
        doReturn(true).when(layoutStateProvider).isLayoutVisible(eq(LayoutType.TAB_SWITCHER));
        Assert.assertTrue(
                ReturnToChromeUtil.shouldHandleTabSwitcherShown(true, layoutStateProvider));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SURFACE_POLISH})
    @DisableFeatures({ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID})
    public void testIsScrollableMvtEnabledWhenSurfacePolishEnabled_tablets() {
        Assert.assertTrue(ChromeFeatureList.sSurfacePolish.isEnabled());
        Assert.assertFalse(
                ChromeFeatureList.isEnabled(ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID));
        Assert.assertFalse(StartSurfaceConfiguration.SURFACE_POLISH_SCROLLABLE_MVT.getValue());
        // Tests on tablets.
        setupAndVerifyTablets();

        // Verifies if feature ChromeFeatureList.SURFACE_POLISH is enabled on tablets, always show
        // the scrollable MV tiles.
        Assert.assertTrue(ReturnToChromeUtil.isScrollableMvtEnabled(mContext));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.SURFACE_POLISH})
    @EnableFeatures({
        ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID,
        ChromeFeatureList.START_SURFACE_ON_TABLET
    })
    public void testIsScrollableMvtEnabled_SurfacePolishDisabled_ScrollableMvtEnabled_tablets() {
        Assert.assertFalse(ChromeFeatureList.sSurfacePolish.isEnabled());
        Assert.assertTrue(
                ChromeFeatureList.isEnabled(ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID));
        Assert.assertFalse(StartSurfaceConfiguration.SURFACE_POLISH_SCROLLABLE_MVT.getValue());

        // Tests on tablets.
        setupAndVerifyTablets();

        // Verifies if feature ChromeFeatureList.SURFACE_POLISH is disabled on tablets, the
        // scrollable MV tiles is only shown when both features
        // SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID and START_SURFACE_ON_TABLET are enabled.
        Assert.assertTrue(ReturnToChromeUtil.isScrollableMvtEnabled(mContext));
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.SURFACE_POLISH,
        ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID
    })
    @EnableFeatures({ChromeFeatureList.START_SURFACE_ON_TABLET})
    public void testIsScrollableMvtEnabled_SurfacePolishDisabled_ScrollableMvtDisabled_tablets() {
        Assert.assertFalse(ChromeFeatureList.sSurfacePolish.isEnabled());
        Assert.assertFalse(
                ChromeFeatureList.isEnabled(ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID));
        Assert.assertFalse(StartSurfaceConfiguration.SURFACE_POLISH_SCROLLABLE_MVT.getValue());

        // Tests on tablets.
        setupAndVerifyTablets();

        // Verifies if feature ChromeFeatureList.SURFACE_POLISH is disabled on tablets, the
        // scrollable MV tiles is disabled if SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID is disabled.
        Assert.assertFalse(ReturnToChromeUtil.isScrollableMvtEnabled(mContext));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.SURFACE_POLISH})
    @DisableFeatures({ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_PHONE_ANDROID})
    public void testIsScrollableMvtEnabledWhenSurfacePolishEnabled_phones() {
        Assert.assertTrue(ChromeFeatureList.sSurfacePolish.isEnabled());
        Assert.assertFalse(
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_PHONE_ANDROID));
        Assert.assertFalse(StartSurfaceConfiguration.SURFACE_POLISH_SCROLLABLE_MVT.getValue());

        // Tests on phones.
        // Verifies if feature ChromeFeatureList.SURFACE_POLISH is enabled, feature
        // ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_PHONE_ANDROID is ignored. Whether to show
        // the scrollable MV tiles is determined by the value of
        // StartSurfaceConfiguration.SURFACE_POLISH_SCROLLABLE_MVT.
        Assert.assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));
        Assert.assertFalse(ReturnToChromeUtil.isScrollableMvtEnabled(mContext));

        StartSurfaceConfiguration.SURFACE_POLISH_SCROLLABLE_MVT.setForTesting(true);
        Assert.assertTrue(ReturnToChromeUtil.isScrollableMvtEnabled(mContext));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.SURFACE_POLISH})
    @EnableFeatures({ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_PHONE_ANDROID})
    public void testIsScrollableMvtEnabled_SurfacePolishDisabled_ScrollableMvtEnabled_phones() {
        Assert.assertFalse(ChromeFeatureList.sSurfacePolish.isEnabled());
        Assert.assertTrue(
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_PHONE_ANDROID));
        Assert.assertFalse(StartSurfaceConfiguration.SURFACE_POLISH_SCROLLABLE_MVT.getValue());

        // Tests on phones.
        // Verifies if feature ChromeFeatureList.SURFACE_POLISH is disabled, whether to show the
        // scrollable MV tiles depends on feature flag
        // ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_PHONE_ANDROID.
        Assert.assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));
        Assert.assertTrue(ReturnToChromeUtil.isScrollableMvtEnabled(mContext));
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.SURFACE_POLISH,
        ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_PHONE_ANDROID
    })
    public void testIsScrollableMvtEnabled_SurfacePolishDisabled_ScrollableMvtDisabled_phones() {
        Assert.assertFalse(ChromeFeatureList.sSurfacePolish.isEnabled());
        Assert.assertFalse(
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_PHONE_ANDROID));
        Assert.assertFalse(StartSurfaceConfiguration.SURFACE_POLISH_SCROLLABLE_MVT.getValue());

        // Tests on phones.
        // Verifies if feature ChromeFeatureList.SURFACE_POLISH is disabled, whether to show the
        // scrollable MV tiles is determined by the feature flag
        // ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_PHONE_ANDROID.
        Assert.assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));
        Assert.assertFalse(ReturnToChromeUtil.isScrollableMvtEnabled(mContext));
    }

    private void setupAndVerifyTablets() {
        doReturn(mResources).when(mContext).getResources();
        doReturn(DeviceFormFactor.SCREEN_BUCKET_TABLET)
                .when(mResources)
                .getInteger(org.chromium.ui.R.integer.min_screen_width_bucket);
        Assert.assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));
    }

    private Intent createMainIntentFromLauncher() {
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        Assert.assertTrue(IntentUtils.isMainIntentFromLauncher(intent));
        return intent;
    }
}
