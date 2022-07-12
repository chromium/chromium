// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.tasks.ReturnToChromeUtil.TAB_SWITCHER_ON_RETURN_MS;
import static org.chromium.chrome.features.start_surface.StartSurfaceConfiguration.START_SURFACE_OPEN_NTP_INSTEAD_OF_START;

import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;

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

import org.chromium.base.CommandLine;
import org.chromium.base.IntentUtils;
import org.chromium.base.SysUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeInactivityTracker;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtilUnitTest.ShadowHomepageManager;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtilUnitTest.ShadowSysUtils;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ReturnToChromeUtil} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowHomepageManager.class, ShadowSysUtils.class})
public class ReturnToChromeUtilUnitTest {
    /** Shadow for {@link HomepageManager}. */
    @Implements(HomepageManager.class)
    static class ShadowHomepageManager {
        static String sHomepageUrl;

        @Implementation
        public static boolean isHomepageEnabled() {
            return true;
        }

        @Implementation
        public static String getHomepageUri() {
            return sHomepageUrl;
        }
    }

    @Implements(SysUtils.class)
    static class ShadowSysUtils {
        @Implementation
        public static boolean isLowEndDevice() {
            return false;
        }
    }

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
    private Intent mIntent;
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

        CachedFeatureFlags.setForTesting(ChromeFeatureList.START_SURFACE_ANDROID, true);

        // HomepageManager:
        ShadowHomepageManager.sHomepageUrl = UrlConstants.NTP_NON_NATIVE_URL;
        Assert.assertEquals(UrlConstants.NTP_NON_NATIVE_URL, HomepageManager.getHomepageUri());
        Assert.assertTrue(HomepageManager.isHomepageEnabled());

        // Low end devices:
        Assert.assertFalse(SysUtils.isLowEndDevice());

        // Sets accessibility:
        StartSurfaceConfiguration.SUPPORT_ACCESSIBILITY.setForTesting(true);
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
        CommandLine.reset();
        SysUtils.resetForTesting();
    }

    @Test
    @SmallTest
    public void testShouldShowTabSwitcher() {
        // Tests when the background time isn't set.
        // If tab switcher on return is disabled, returns false:
        TAB_SWITCHER_ON_RETURN_MS.setForTesting(-1);
        Assert.assertEquals(-1, TAB_SWITCHER_ON_RETURN_MS.getValue());
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(-1));
        // If tab switcher on return is enabled but NOT immediate, returns false:
        TAB_SWITCHER_ON_RETURN_MS.setForTesting(60000);
        Assert.assertEquals(60000, TAB_SWITCHER_ON_RETURN_MS.getValue());
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(-1));
        // If tab switcher on return immediate is enabled, returns true:
        TAB_SWITCHER_ON_RETURN_MS.setForTesting(0);
        Assert.assertEquals(0, TAB_SWITCHER_ON_RETURN_MS.getValue());
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(-1));

        // Tests the cases when the background time is set.
        // Tab switcher on return is disabled:
        TAB_SWITCHER_ON_RETURN_MS.setForTesting(-1);
        Assert.assertEquals(-1, TAB_SWITCHER_ON_RETURN_MS.getValue());
        Assert.assertFalse(
                ReturnToChromeUtil.shouldShowTabSwitcher(System.currentTimeMillis() - 1));

        // Tab switcher on return is enabled immediate:
        int returnTimeMs = 0;
        TAB_SWITCHER_ON_RETURN_MS.setForTesting(returnTimeMs);
        Assert.assertEquals(0, TAB_SWITCHER_ON_RETURN_MS.getValue());
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(System.currentTimeMillis() - 1));

        // Tab switcher on return is enabled for 1 min:
        returnTimeMs = 60000; // One minute
        TAB_SWITCHER_ON_RETURN_MS.setForTesting(returnTimeMs);
        // When return time arrives, return true:
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(
                System.currentTimeMillis() - returnTimeMs - 1));
        // When return times hasn't arrived, return false:
        Assert.assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(System.currentTimeMillis()));
    }

    @Test
    @SmallTest
    public void testShouldShowStartSurfaceAsTheHomePage() {
        START_SURFACE_OPEN_NTP_INSTEAD_OF_START.setForTesting(true);
        Assert.assertTrue(ReturnToChromeUtil.isStartSurfaceEnabled(mContext));

        // Sets main intent from launcher:
        doReturn(Intent.ACTION_MAIN).when(mIntent).getAction();
        doReturn(true).when(mIntent).hasCategory(Intent.CATEGORY_LAUNCHER);
        Assert.assertTrue(IntentUtils.isMainIntentFromLauncher(mIntent));

        // Sets background time:
        SharedPreferencesManager.getInstance().addToStringSet(
                ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF, "0");
        TAB_SWITCHER_ON_RETURN_MS.setForTesting(0);
        Assert.assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(0));

        // Tests the case when there isn't any Tab:
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(0).when(mTabModelSelector).getTotalTabCount();
        Assert.assertTrue(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, mIntent, mTabModelSelector, mInactivityTracker));

        // Tests the case when the total tab count > 0:
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        doReturn(1).when(mTabModelSelector).getTotalTabCount();
        Assert.assertTrue(ReturnToChromeUtil.shouldShowOverviewPageOnStart(
                mContext, mIntent, mTabModelSelector, mInactivityTracker));

        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF);
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
}
