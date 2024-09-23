// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;

import android.os.Handler;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController.ActionProvider;
import org.chromium.chrome.browser.tab.Tab;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Unit tests for {@link SignalAccumulator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SignalAccumulatorTest {
    @Mock private Tab mMockTab;

    @Mock private Handler mHandler;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
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
}
