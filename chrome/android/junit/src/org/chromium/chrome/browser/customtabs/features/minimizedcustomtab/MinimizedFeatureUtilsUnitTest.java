// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ActivityType.CUSTOM_TAB;
import static org.chromium.chrome.browser.flags.ActivityType.WEBAPP;

import android.app.AppOpsManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.SysUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils.MinimizedFeatureAvailability;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtilsUnitTest.ShadowSysUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Unit tests for {@link MinimizedFeatureUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSysUtils.class})
@EnableFeatures({ChromeFeatureList.CCT_MINIMIZED})
public class MinimizedFeatureUtilsUnitTest {
    @Implements(SysUtils.class)
    static class ShadowSysUtils {
        public static boolean sIsLowEndDevice;

        @Implementation
        public static boolean isLowEndDevice() {
            return sIsLowEndDevice;
        }
    }

    private static final String NAME = "Chrome";
    private static final int UID = 101;
    private static final String HISTOGRAM = "CustomTabs.MinimizedFeatureAvailability";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private PackageManager mPackageManager;
    @Mock private AppOpsManager mAppOpsManager;
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock private Intent mIntent;

    private final ApplicationInfo mApplicationInfo = new ApplicationInfo();

    @Before
    public void setUp() {
        ShadowSysUtils.sIsLowEndDevice = false;
        mApplicationInfo.uid = UID;
        when(mContext.getApplicationInfo()).thenReturn(mApplicationInfo);
        when(mContext.getPackageName()).thenReturn(NAME);
        when(mPackageManager.hasSystemFeature(eq(PackageManager.FEATURE_PICTURE_IN_PICTURE)))
                .thenReturn(true);
        when(mContext.getPackageManager()).thenReturn(mPackageManager);
        when(mAppOpsManager.checkOpNoThrow(
                        eq(AppOpsManager.OPSTR_PICTURE_IN_PICTURE), eq(UID), eq(NAME)))
                .thenReturn(AppOpsManager.MODE_ALLOWED);
        when(mContext.getSystemService(eq(Context.APP_OPS_SERVICE))).thenReturn(mAppOpsManager);
    }

    @After
    public void tearDown() {
        ShadowSysUtils.sIsLowEndDevice = false;
    }

    @Test
    public void testSdkLevel_OAndAbove() {
        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM, MinimizedFeatureAvailability.AVAILABLE)) {
            assertTrue(MinimizedFeatureUtils.isMinimizedCustomTabAvailable(mContext, null));
        }
    }

    @Test
    public void testLowEndDevice() {
        ShadowSysUtils.sIsLowEndDevice = true;
        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM, MinimizedFeatureAvailability.UNAVAILABLE_LOW_END_DEVICE)) {
            assertFalse(MinimizedFeatureUtils.isMinimizedCustomTabAvailable(mContext, null));
        }
    }

    @Test
    public void testHasSystemFeatureFalse() {
        when(mPackageManager.hasSystemFeature(eq(PackageManager.FEATURE_PICTURE_IN_PICTURE)))
                .thenReturn(false);
        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM, MinimizedFeatureAvailability.UNAVAILABLE_SYSTEM_FEATURE)) {
            assertFalse(MinimizedFeatureUtils.isMinimizedCustomTabAvailable(mContext, null));
        }
    }

    @Test
    public void testPipAllowedFalse() {
        when(mAppOpsManager.checkOpNoThrow(
                        eq(AppOpsManager.OPSTR_PICTURE_IN_PICTURE), eq(UID), eq(NAME)))
                .thenReturn(AppOpsManager.MODE_IGNORED);
        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM, MinimizedFeatureAvailability.UNAVAILABLE_PIP_PERMISSION)) {
            assertFalse(MinimizedFeatureUtils.isMinimizedCustomTabAvailable(mContext, null));
        }
    }

    @Test
    public void testShouldEnableMinimizedCustomTabs() {
        // True for regular Custom Tabs
        when(mIntentDataProvider.getIntent()).thenReturn(mIntent);
        when(mIntent.getIntExtra(eq(IntentHandler.EXTRA_FEDCM_ID), anyInt())).thenReturn(-1);
        when(mIntentDataProvider.getActivityType()).thenReturn(CUSTOM_TAB);
        assertTrue(MinimizedFeatureUtils.shouldEnableMinimizedCustomTabs(mIntentDataProvider));
        // False for Webapps
        when(mIntentDataProvider.getActivityType()).thenReturn(WEBAPP);
        assertFalse(MinimizedFeatureUtils.shouldEnableMinimizedCustomTabs(mIntentDataProvider));
        // False for FedCM
        when(mIntentDataProvider.getActivityType()).thenReturn(CUSTOM_TAB);
        when(mIntentDataProvider.isTrustedIntent()).thenReturn(true);
        when(mIntent.getIntExtra(eq(IntentHandler.EXTRA_FEDCM_ID), anyInt())).thenReturn(100);
        assertFalse(MinimizedFeatureUtils.shouldEnableMinimizedCustomTabs(mIntentDataProvider));
        // False for AuthTab
        when(mIntent.getIntExtra(eq(IntentHandler.EXTRA_FEDCM_ID), anyInt())).thenReturn(-1);
        when(mIntentDataProvider.isTrustedIntent()).thenReturn(false);
        when(mIntentDataProvider.isAuthTab()).thenReturn(true);
        assertFalse(MinimizedFeatureUtils.shouldEnableMinimizedCustomTabs(mIntentDataProvider));
    }
}
