// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.PackageManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabBaseStrategy.DeviceSpec;

/** Tests for {@link PartialCustomTabBaseStrategy}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PartialCustomTabBaseStrategyTest {

    @Mock ActivityManager mActivityManager;
    @Mock PackageManager mPackageManager;
    @Mock private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mContext.getSystemService(Context.ACTIVITY_SERVICE)).thenReturn(mActivityManager);
        when(mContext.getPackageManager()).thenReturn(mPackageManager);
    }

    @Test
    public void logDeviceSpecForPcct() {
        var histogramWatcher = buildHistogramWatcher(DeviceSpec.HIGHEND_NOPIP);
        PartialCustomTabBaseStrategy.logDeviceSpecForPcct(mContext);
        histogramWatcher.assertExpected();
        PartialCustomTabBaseStrategy.resetDeviceSpecLoggedForTesting();

        when(mActivityManager.isLowRamDevice()).thenReturn(true);
        histogramWatcher = buildHistogramWatcher(DeviceSpec.LOWEND_NOPIP);
        PartialCustomTabBaseStrategy.logDeviceSpecForPcct(mContext);
        histogramWatcher.assertExpected();
        PartialCustomTabBaseStrategy.resetDeviceSpecLoggedForTesting();

        when(mPackageManager.hasSystemFeature(eq(PackageManager.FEATURE_PICTURE_IN_PICTURE)))
                .thenReturn(true);
        histogramWatcher = buildHistogramWatcher(DeviceSpec.LOWEND_PIP);
        PartialCustomTabBaseStrategy.logDeviceSpecForPcct(mContext);
        histogramWatcher.assertExpected();
        PartialCustomTabBaseStrategy.resetDeviceSpecLoggedForTesting();

        when(mActivityManager.isLowRamDevice()).thenReturn(false);
        histogramWatcher = buildHistogramWatcher(DeviceSpec.HIGHEND_PIP);
        PartialCustomTabBaseStrategy.logDeviceSpecForPcct(mContext);
        histogramWatcher.assertExpected();
        PartialCustomTabBaseStrategy.resetDeviceSpecLoggedForTesting();
    }

    @Test
    public void logDeviceSpecForPcct_oncePerChromeSession() {
        when(mActivityManager.isLowRamDevice()).thenReturn(true);
        when(mPackageManager.hasSystemFeature(eq(PackageManager.FEATURE_PICTURE_IN_PICTURE)))
                .thenReturn(true);
        var histogramWatcher = buildHistogramWatcher(DeviceSpec.LOWEND_PIP);
        PartialCustomTabBaseStrategy.logDeviceSpecForPcct(mContext);
        histogramWatcher.assertExpected();

        // No more logging from subsequent launching.
        histogramWatcher = HistogramWatcher.newBuilder().build();
        PartialCustomTabBaseStrategy.logDeviceSpecForPcct(mContext);
        histogramWatcher.assertExpected();
    }

    private static HistogramWatcher buildHistogramWatcher(@DeviceSpec int spec) {
        return HistogramWatcher.newBuilder().expectIntRecord("CustomTabs.DeviceSpec", spec).build();
    }
}
