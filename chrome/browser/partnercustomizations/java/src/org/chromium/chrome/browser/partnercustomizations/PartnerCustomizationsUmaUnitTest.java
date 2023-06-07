// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import android.os.SystemClock;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureList.TestValues;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.CustomizationProviderDelegateType;
import org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.DelegateUnusedReason;

/**
 * Unit tests for {@link PartnerCustomizationsUma}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PartnerCustomizationsUmaUnitTest {
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcherMock;

    @Captor
    private ArgumentCaptor<LifecycleObserver> mLifeCycleObserverCaptor;

    private TestValues mEnabledTestValues;
    private TestValues mDisabledTestValues;

    private PartnerCustomizationsUma mPartnerCustomizationsUma;

    private boolean mDidCall;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mEnabledTestValues = new TestValues();
        mEnabledTestValues.addFeatureFlagOverride(
                ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA, true);
        mDisabledTestValues = new TestValues();
        mDisabledTestValues.addFeatureFlagOverride(
                ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA, false);
        mPartnerCustomizationsUma = new PartnerCustomizationsUma();
        mDidCall = false;
    }

    @Test
    public void testIsEnabled() {
        FeatureList.setTestValues(mEnabledTestValues);
        Assert.assertTrue(PartnerCustomizationsUma.isEnabled());
    }

    @Test
    public void testIsEnabled_false() {
        FeatureList.setTestValues(mDisabledTestValues);
        Assert.assertFalse(PartnerCustomizationsUma.isEnabled());
    }

    @Test
    public void testOnFinishNativeInitializationEnabled_alreadyEnabled() {
        FeatureList.setTestValues(mEnabledTestValues);
        mPartnerCustomizationsUma.onFinishNativeInitializationOrEnabled(
                mActivityLifecycleDispatcherMock, () -> mDidCall = true);
        verifyNoInteractions(mActivityLifecycleDispatcherMock);
        Assert.assertTrue(mDidCall);
    }

    @Test
    public void testOnFinishNativeInitializationEnabled_alreadyDisabled() {
        FeatureList.setTestValues(mDisabledTestValues);
        mPartnerCustomizationsUma.onFinishNativeInitializationOrEnabled(
                mActivityLifecycleDispatcherMock, () -> mDidCall = true);
        verifyNoInteractions(mActivityLifecycleDispatcherMock);
        Assert.assertFalse(mDidCall);
    }

    @Test
    public void testOnFinishNativeInitializationEnabled_beforeNativeInit() {
        mPartnerCustomizationsUma.onFinishNativeInitializationOrEnabled(
                mActivityLifecycleDispatcherMock, () -> mDidCall = true);
        verify(mActivityLifecycleDispatcherMock, times(1))
                .register(mLifeCycleObserverCaptor.capture());
        NativeInitObserver observer = (NativeInitObserver) mLifeCycleObserverCaptor.getValue();
        FeatureList.setTestValues(mEnabledTestValues);
        Assert.assertFalse(mDidCall);
        observer.onFinishNativeInitialization();
        Assert.assertTrue(mDidCall);
    }

    @Test
    public void testOnFinishNativeInitializationEnabled_beforeNativeInitDisabled() {
        mPartnerCustomizationsUma.onFinishNativeInitializationOrEnabled(
                mActivityLifecycleDispatcherMock, () -> mDidCall = true);
        verify(mActivityLifecycleDispatcherMock, times(1))
                .register(mLifeCycleObserverCaptor.capture());
        NativeInitObserver observer = (NativeInitObserver) mLifeCycleObserverCaptor.getValue();
        FeatureList.setTestValues(mDisabledTestValues);
        Assert.assertFalse(mDidCall);
        observer.onFinishNativeInitialization();
        Assert.assertFalse(mDidCall);
    }

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
                                PartnerCustomizationsUma.CustomizationUsage.HOMEPAGE)
                        .build();
        PartnerCustomizationsUma.logPartnerCustomizationUsage(
                PartnerCustomizationsUma.CustomizationUsage.HOMEPAGE);
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
