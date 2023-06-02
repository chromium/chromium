// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import android.os.SystemClock;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.CustomizationProviderDelegateType;
import org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.DelegateUnusedReason;

/**
 * Unit tests for {@link PartnerCustomizationsUma}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PartnerCustomizationsUmaUnitTest {
    @Test
    public void testLogPartnerCustomizationDelegate() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.PartnerHomepageCustomization.Delegate2",
                                CustomizationProviderDelegateType.G_SERVICE)
                        .build();
        PartnerCustomizationsUma.logPartnerCustomizationDelegate(
                CustomizationProviderDelegateType.G_SERVICE);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testLogPartnerCustomizationUsage() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.PartnerCustomization.Usage",
                                CustomizationProviderDelegateType.G_SERVICE)
                        .build();
        PartnerCustomizationsUma.logPartnerCustomizationUsage(
                CustomizationProviderDelegateType.G_SERVICE);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testLogDelegateUnusedReason() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.PartnerCustomization.DelegateUnusedReason",
                                DelegateUnusedReason.GSERVICES_TIMESTAMP_MISSING)
                        .build();
        PartnerCustomizationsUma.logDelegateUnusedReason(
                DelegateUnusedReason.GSERVICES_TIMESTAMP_MISSING);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testLogDelegateTryCreateDuration() {
        @CustomizationProviderDelegateType
        int delegate = CustomizationProviderDelegateType.G_SERVICE;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.PartnerCustomization.TrySucceededDuration.GService", 7)
                        .build();

        long startTime = SystemClock.elapsedRealtime();
        long endTime = startTime + 7;
        PartnerCustomizationsUma.logDelegateTryCreateDuration(delegate, startTime, endTime, true);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testLogDelegateTryCreateDuration_failed() {
        @CustomizationProviderDelegateType
        int delegate = CustomizationProviderDelegateType.PRELOAD_APK;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.PartnerCustomization.TryFailedDuration.PreloadApk", 9)
                        .build();

        long startTime = SystemClock.elapsedRealtime();
        long endTime = startTime + 9;
        PartnerCustomizationsUma.logDelegateTryCreateDuration(delegate, startTime, endTime, false);

        histogramWatcher.assertExpected();
    }
}
