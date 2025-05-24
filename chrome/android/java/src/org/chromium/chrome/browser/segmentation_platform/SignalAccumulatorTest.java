// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;

import android.os.Handler;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController.ActionProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Unit tests for {@link SignalAccumulator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures(DomDistillerFeatures.READER_MODE_IMPROVEMENTS)
public class SignalAccumulatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tab mMockTab;

    @Mock private Handler mHandler;

    @Before
    public void setUp() {
        Mockito.doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mHandler)
                .postDelayed(any(), anyLong());
    }

    @Test
    public void testAllSignalsBeforeTimeout() throws TimeoutException {
        List<ActionProvider> actionProviders = new ArrayList<>();
        ActionProvider actionProvider =
                (tab, accumulator) -> {
                    accumulator.setHasPriceTracking(true);
                    accumulator.setHasReaderMode(false);
                    accumulator.setHasPriceInsights(true);
                    accumulator.setHasDiscounts(true);
                };
        actionProviders.add(actionProvider);
        final CallbackHelper callbackHelper = new CallbackHelper();
        int callCount = callbackHelper.getCallCount();
        SignalAccumulator accumulator = new SignalAccumulator(mHandler, mMockTab, actionProviders);
        accumulator.getSignals(() -> callbackHelper.notifyCalled());
        callbackHelper.waitForCallback(callCount);
        Assert.assertTrue(accumulator.hasPriceTracking());
        Assert.assertFalse(accumulator.hasReaderMode());
        Assert.assertTrue(accumulator.hasPriceInsights());
        Assert.assertTrue(accumulator.hasDiscounts());
    }

    @Test
    public void testTimeoutBeforeAllSignals() throws TimeoutException {
        final CallbackHelper callbackHelper = new CallbackHelper();
        int callCount = callbackHelper.getCallCount();
        SignalAccumulator accumulator =
                new SignalAccumulator(mHandler, mMockTab, new ArrayList<ActionProvider>());
        accumulator.getSignals(() -> callbackHelper.notifyCalled());
        callbackHelper.waitForCallback(callCount);
        Assert.assertFalse(accumulator.hasPriceTracking());
        Assert.assertFalse(accumulator.hasReaderMode());
        Assert.assertFalse(accumulator.hasPriceInsights());
        Assert.assertFalse(accumulator.hasDiscounts());
    }

    @Test
    public void testSetReaderModeRecordsTime() throws TimeoutException {
        List<ActionProvider> actionProviders = new ArrayList<>();
        ActionProvider actionProvider =
                (tab, accumulator) -> {
                    accumulator.setHasReaderMode(false);
                };
        actionProviders.add(actionProvider);
        final CallbackHelper callbackHelper = new CallbackHelper();
        SignalAccumulator accumulator = new SignalAccumulator(mHandler, mMockTab, actionProviders);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(
                                SignalAccumulator.READER_MODE_SIGNAL_TIME_HISTOGRAM, 1)
                        .build();
        accumulator.getSignals(() -> callbackHelper.notifyCalled());
        callbackHelper.waitForNext();
        watcher.assertExpected();
    }

    @Test
    public void testTimeout() throws TimeoutException {
        List<ActionProvider> actionProviders = new ArrayList<>();
        ActionProvider actionProvider =
                (tab, accumulator) -> {
                    accumulator.setHasReaderMode(false);
                };
        actionProviders.add(actionProvider);
        SignalAccumulator accumulator = new SignalAccumulator(mHandler, mMockTab, actionProviders);
        assertEquals(100, accumulator.getActionProviderTimeoutForTesting());
    }

    @Test
    @EnableFeatures(
            DomDistillerFeatures.READER_MODE_IMPROVEMENTS
                    + ":custom_cpa_timeout_enabled/true/custom_cpa_timeout/300")
    public void testIncreasedTimeoutWithFeature() throws TimeoutException {
        List<ActionProvider> actionProviders = new ArrayList<>();
        ActionProvider actionProvider =
                (tab, accumulator) -> {
                    accumulator.setHasReaderMode(false);
                };
        actionProviders.add(actionProvider);
        SignalAccumulator accumulator = new SignalAccumulator(mHandler, mMockTab, actionProviders);
        assertEquals(300, accumulator.getActionProviderTimeoutForTesting());
    }
}
