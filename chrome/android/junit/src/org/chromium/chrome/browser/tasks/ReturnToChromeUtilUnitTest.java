// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.features.start_surface.StartSurfaceConfiguration.START_SURFACE_OPEN_START_AS_HOMEPAGE;
import static org.chromium.chrome.features.start_surface.StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS;
import static org.chromium.chrome.features.start_surface.StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_USE_MODEL;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.text.format.DateUtils;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.IntentUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtilUnitTest.ShadowHomepageManager;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtilUnitTest.ShadowHomepagePolicyManager;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtilUnitTest.ShadowSysUtils;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.segmentation_platform.SegmentSelectionResult;
import org.chromium.components.segmentation_platform.proto.SegmentationProto.SegmentId;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.base.test.util.DisabledTest;

/** Unit tests for {@link ReturnToChromeUtil} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowHomepageManager.class, ShadowHomepagePolicyManager.class,
                ShadowSysUtils.class})
@Features.EnableFeatures({ChromeFeatureList.START_SURFACE_WITH_ACCESSIBILITY})
public class ReturnToChromeUtilUnitTest {
    /** Shadow for {@link HomepageManager}. */
    @Implements(HomepageManager.class)
    static class ShadowHomepageManager {
        static String sHomepageUrl;
        static boolean sIsHomepageEnabled;

        @Implementation
        public static boolean isHomepageEnabled() {
            return sIsHomepageEnabled;
        }

        @Implementation
        public static String getHomepageUri() {
            return sHomepageUrl;
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

    @Implements(SysUtils.class)
    static class ShadowSysUtils {
        @Implementation
        public static boolean isLowEndDevice() {
            return false;
        }
    }

    private static final int ON_RETURN_THRESHOLD_SECOND = 1000;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock
    private Context mContext;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private ChromeInactivityTracker mInactivityTracker;
    @Mock
    private Resources mResources;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        doReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL))
                .when(mUrlFormatterJniMock)
                .fixupUrl(UrlConstants.NTP_NON_NATIVE_URL);

        ChromeFeatureList.sStartSurfaceAndroid.setForTesting(true);

        // HomepageManager:
        ShadowHomepageManager.sHomepageUrl = UrlConstants.NTP_NON_NATIVE_URL;
        ShadowHomepageManager.sIsHomepageEnabled = true;
        Assert.assertEquals(UrlConstants.NTP_NON_NATIVE_URL, HomepageManager.getHomepageUri());
        Assert.assertTrue(HomepageManager.isHomepageEnabled());

        ShadowHomepagePolicyManager.sIsInitialized = true;
        Assert.assertTrue(HomepagePolicyManager.isInitializedWithNative());

        // Low end devices:
        Assert.assertFalse(SysUtils.isLowEndDevice());

        // Sets accessibility:
        ChromeAccessibilityUtil.get().setAccessibilityEnabledForTesting(false);

        // Sets for !DeviceFormFactor.isNonMultiDisplayContextOnTablet():
        doReturn(mResources).when(mContext).getResources();
        doReturn(DeviceFormFactor.SCREEN_BUCKET_TABLET - 1)
                .when(mResources)
                .getInteger(org.chromium.ui.R.integer.min_screen_width_bucket);
        Assert.assertFalse(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));
    }

    @After
    public void tearDown() {
        SysUtils.resetForTesting();
    }

    @Test
    @SmallTest
    @DisabledTest(message = "Flaky, see crbug.com/1425954")
    public void testShouldShowTabSwitcher() {
        Assert.assertEquals(START_SURFACE_RETURN_TIME_SECONDS.getDefaultValue(),
                START_SURFACE_RETURN_TIME_SECONDS.getValue());
        Assert.assertFalse(ChromeFeatureList.sStartSurfaceReturnTime.isEnabled());

        long returnTimeMs =
                START_SURFACE_RETURN_TIME_SECONDS.getValue() * DateUtils.SECOND_IN_MILLIS;
        // When return time doesn't arrive, return false:
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(
                System.currentTimeMillis() - returnTimeMs + 1));

        // When return time arrives, return true:
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(
                System.currentTimeMillis() - returnTimeMs - 1));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME})
    public void testShouldShowTabSwitcherWithStartReturnTimeWithoutUseModel() {
        Assert.assertTrue(ChromeFeatureList.sStartSurfaceReturnTime.isEnabled());
        Assert.assertTrue(ChromeFeatureList.sStartSurfaceAndroid.isEnabled());
        START_SURFACE_RETURN_TIME_USE_MODEL.setForTesting(false);
        Assert.assertFalse(START_SURFACE_RETURN_TIME_USE_MODEL.getValue());

        // Set to not shown.
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(-1);
        Assert.assertEquals(-1, START_SURFACE_RETURN_TIME_SECONDS.getValue());
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(-1));
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowTabSwitcher(System.currentTimeMillis() - 10));

        // Sets to immediate return.
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        Assert.assertEquals(0, START_SURFACE_RETURN_TIME_SECONDS.getValue());
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(-1));
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(System.currentTimeMillis()));

        // Sets to an random time.
        int expectedReturnTimeSeconds = 60; // one minute
        int expectedReturnTimeMs = (int) (expectedReturnTimeSeconds * DateUtils.SECOND_IN_MILLIS);
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(expectedReturnTimeSeconds);
        Assert.assertEquals(
                expectedReturnTimeSeconds, START_SURFACE_RETURN_TIME_SECONDS.getValue());
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(-1));
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(
                System.currentTimeMillis() - expectedReturnTimeMs));
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(
                System.currentTimeMillis() - expectedReturnTimeSeconds));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.START_SURFACE_RETURN_TIME})
    public void testShouldShowTabSwitcherWithSegmentationReturnTime() {
        final SegmentId showStartId =
                SegmentId.OPTIMIZATION_TARGET_SEGMENTATION_CHROME_START_ANDROID_V2;
        Assert.assertTrue(ChromeFeatureList.sStartSurfaceReturnTime.isEnabled());

        // Verifies that when the preference key isn't stored, return
        // START_SURFACE_RETURN_TIME_SECONDS.getDefaultValue() as default value, i.e., 8 hours.
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        Assert.assertEquals(START_SURFACE_RETURN_TIME_SECONDS.getDefaultValue(),
                ReturnToChromeUtil.getReturnTimeFromSegmentation());

        // Verifies returning false if both flags haven't been set any value or any meaningful yet.
        START_SURFACE_RETURN_TIME_USE_MODEL.setForTesting(true);
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(-1));

        // Return time from segmentation model is enabled for 1 min:
        long returnTimeSeconds = 60; // One minute
        long returnTimeMs = returnTimeSeconds * DateUtils.SECOND_IN_MILLIS; // One minute
        sharedPreferencesManager = SharedPreferencesManager.getInstance();
        SegmentSelectionResult result =
                new SegmentSelectionResult(true, showStartId, (float) returnTimeSeconds);
        ReturnToChromeUtil.cacheReturnTimeFromSegmentationImpl(result);
        Assert.assertEquals(returnTimeMs, ReturnToChromeUtil.getReturnTimeFromSegmentation());

        // Returns false if it isn't immediate return but without last backgrounded time available:
        result = new SegmentSelectionResult(true, showStartId, (float) 1);
        ReturnToChromeUtil.cacheReturnTimeFromSegmentationImpl(result);
        Assert.assertEquals(
                1 * DateUtils.SECOND_IN_MILLIS, ReturnToChromeUtil.getReturnTimeFromSegmentation());
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(-1));

        // Verifies returning false if segmentation result is negative (not show).
        result = new SegmentSelectionResult(true, SegmentId.OPTIMIZATION_TARGET_UNKNOWN, null);
        ReturnToChromeUtil.cacheReturnTimeFromSegmentationImpl(result);
        Assert.assertEquals(-1, ReturnToChromeUtil.getReturnTimeFromSegmentation());
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(1));

        // Tests regular cases with last backgrounded time set:
        result = new SegmentSelectionResult(true, showStartId, (float) returnTimeSeconds);
        ReturnToChromeUtil.cacheReturnTimeFromSegmentationImpl(result);
        Assert.assertEquals(returnTimeMs, ReturnToChromeUtil.getReturnTimeFromSegmentation());

        int doubleReturnTimeMs = (int) (2 * returnTimeMs); // Two minutes
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(doubleReturnTimeMs);
        Assert.assertEquals(doubleReturnTimeMs, START_SURFACE_RETURN_TIME_SECONDS.getValue());

        // When segmentation platform's return time arrives, return true:
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(
                System.currentTimeMillis() - returnTimeMs - 1));

        // When segmentation platform's return times hasn't arrived, return false:
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(System.currentTimeMillis()));

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
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(expectedVisibleTime));
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(expectedLastBackgroundTime));
        Assert.assertTrue(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, intent, mTabModelSelector, mInactivityTracker, false /* isTablet */));

        // Verifies that Start will NOT show if the threshold of return time hasn't reached using
        // last visible time, while last background time is lost or not set.
        currentTime = System.currentTimeMillis();
        expectedVisibleTime = currentTime - returnTimeMS + 100; // doesn't reach
        doReturn(expectedVisibleTime).when(mInactivityTracker).getLastVisibleTimeMs();
        doReturn(expectedLastBackgroundTime).when(mInactivityTracker).getLastBackgroundedTimeMs();
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(expectedVisibleTime));
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(expectedLastBackgroundTime));
        Assert.assertFalse(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, intent, mTabModelSelector, mInactivityTracker, false /* isTablet */));

        // Verifies that Start will NOT show if the threshold of return time has reached using
        // last visible time, while hasn't using the last background time which is the max time.
        currentTime = System.currentTimeMillis();
        expectedVisibleTime = currentTime - returnTimeMS - 1; // has reached
        expectedLastBackgroundTime = currentTime - returnTimeMS + 100; // doesn't reach
        doReturn(expectedVisibleTime).when(mInactivityTracker).getLastVisibleTimeMs();
        doReturn(expectedLastBackgroundTime).when(mInactivityTracker).getLastBackgroundedTimeMs();
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(expectedVisibleTime));
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(expectedLastBackgroundTime));
        Assert.assertFalse(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, intent, mTabModelSelector, mInactivityTracker, false /* isTablet */));

        // Verifies that Start will show if the threshold of both return time has reached using
        // either last visible time or the last background time.
        currentTime = System.currentTimeMillis();
        expectedVisibleTime = currentTime - returnTimeMS - 2; // has reached
        expectedLastBackgroundTime = currentTime - returnTimeMS - 1; // has reached
        doReturn(expectedVisibleTime).when(mInactivityTracker).getLastVisibleTimeMs();
        doReturn(expectedLastBackgroundTime).when(mInactivityTracker).getLastBackgroundedTimeMs();
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(expectedVisibleTime));
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(expectedLastBackgroundTime));
        Assert.assertTrue(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, intent, mTabModelSelector, mInactivityTracker, false /* isTablet */));
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
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(0));

        // Tests the case when there isn't any Tab:
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(0).when(mTabModelSelector).getTotalTabCount();
        Assert.assertTrue(HomepagePolicyManager.isInitializedWithNative());
        Assert.assertTrue(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, intent, mTabModelSelector, mInactivityTracker, false /* isTablet */));

        // Tests the case when the total tab count > 0:
        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        Assert.assertFalse(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, intent, mTabModelSelector, mInactivityTracker, false /* isTablet */));

        // Sets background time to make the return time arrive:
        SharedPreferencesManager.getInstance().addToStringSet(
                ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF, "0");
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(0));

        // Verifies that Start will show since the return time has arrived.
        Assert.assertTrue(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, intent, mTabModelSelector, mInactivityTracker, false /* isTablet */));

        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF);
    }

    @Test
    @SmallTest
    public void testShouldShowStartSurfaceWithCustomizedHomePage() {
        START_SURFACE_OPEN_START_AS_HOMEPAGE.setForTesting(true);
        Assert.assertTrue(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));

        // Sets a customized homepage:
        ShadowHomepageManager.sHomepageUrl = "foo.com";
        Assert.assertFalse(ReturnToChromeUtil.useChromeHomepage());

        // Sets main intent from launcher:
        Intent intent = createMainIntentFromLauncher();

        // Sets background time to make the return time arrive:
        SharedPreferencesManager.getInstance().addToStringSet(
                ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF, "0");
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(0));

        // Tests the case when there isn't any Tab but with customized homepage:
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(0).when(mTabModelSelector).getTotalTabCount();
        Assert.assertFalse(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, intent, mTabModelSelector, mInactivityTracker, false /* isTablet */));

        // Tests the case when the total tab count > 0 and return time arrives, Start will show.
        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        Assert.assertTrue(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, intent, mTabModelSelector, mInactivityTracker, false /* isTablet */));

        ShadowHomepageManager.sHomepageUrl = UrlConstants.NTP_NON_NATIVE_URL;
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF);
    }

    @Test
    @SmallTest
    public void testShouldShowStartSurfaceAtStartupWithHomepageDisabled() {
        START_SURFACE_OPEN_START_AS_HOMEPAGE.setForTesting(true);
        Assert.assertTrue(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));

        // Sets main intent from launcher:
        Intent intent = createMainIntentFromLauncher();

        // Sets background time to make the return time arrive:
        SharedPreferencesManager.getInstance().addToStringSet(
                ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF, "0");
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(0));

        // When homepage is disabled, verifies that Start isn't shown when there isn't any Tab, even
        // if the return time has arrived.
        ShadowHomepageManager.sIsHomepageEnabled = false;
        Assert.assertFalse(HomepageManager.isHomepageEnabled());
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(0).when(mTabModelSelector).getTotalTabCount();
        Assert.assertFalse(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, intent, mTabModelSelector, mInactivityTracker, false /* isTablet */));
    }

    @Test
    @SmallTest
    public void testStartSurfaceIsDisabledOnTablet() {
        // Sets for !DeviceFormFactor.isNonMultiDisplayContextOnTablet()
        doReturn(mResources).when(mContext).getResources();
        doReturn(DeviceFormFactor.SCREEN_BUCKET_TABLET)
                .when(mResources)
                .getInteger(org.chromium.ui.R.integer.min_screen_width_bucket);
        Assert.assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext));

        Assert.assertFalse(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));
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
        SharedPreferencesManager.getInstance().addToStringSet(
                ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF, "0");
        START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(0));

        // Tests the case when there isn't any Tab. Verifies that Start isn't shown if
        // HomepagePolicyManager isn't initialized.
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(0).when(mTabModelSelector).getTotalTabCount();
        Assert.assertTrue(HomepageManager.isHomepageEnabled());
        Assert.assertFalse(ReturnToChromeUtil.useChromeHomepage());
        Assert.assertFalse(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, intent, mTabModelSelector, mInactivityTracker, false /* isTablet */));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Startup.Android.IsHomepagePolicyManagerInitialized"));

        // Tests the case when the total tab count > 0. Verifies that Start is shown when the return
        // time arrives.
        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        Assert.assertTrue(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, intent, mTabModelSelector, mInactivityTracker, false /* isTablet */));
        // Verifies that we don't record the histogram again.
        Assert.assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Startup.Android.IsHomepagePolicyManagerInitialized"));
    }

    private Intent createMainIntentFromLauncher() {
        Intent intent = new Intent();
        intent.setAction(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        Assert.assertTrue(IntentUtils.isMainIntentFromLauncher(intent));
        return intent;
    }
}
