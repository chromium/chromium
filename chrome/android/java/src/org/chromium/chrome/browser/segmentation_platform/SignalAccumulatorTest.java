// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

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

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController.ActionProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;

import java.util.HashMap;
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
        HashMap<Integer, ActionProvider> actionProviders = new HashMap<>();
        actionProviders.put(
                AdaptiveToolbarButtonVariant.PRICE_TRACKING,
                (tab, signalAccumulator) ->
                        signalAccumulator.setSignal(
                                AdaptiveToolbarButtonVariant.PRICE_TRACKING, true));
        actionProviders.put(
                AdaptiveToolbarButtonVariant.READER_MODE,
                (tab, signalAccumulator) ->
                        signalAccumulator.setSignal(
                                AdaptiveToolbarButtonVariant.READER_MODE, false));
        actionProviders.put(
                AdaptiveToolbarButtonVariant.PRICE_INSIGHTS,
                (tab, signalAccumulator) ->
                        signalAccumulator.setSignal(
                                AdaptiveToolbarButtonVariant.PRICE_INSIGHTS, true));
        actionProviders.put(
                AdaptiveToolbarButtonVariant.DISCOUNTS,
                (tab, signalAccumulator) ->
                        signalAccumulator.setSignal(AdaptiveToolbarButtonVariant.DISCOUNTS, true));
        final CallbackHelper callbackHelper = new CallbackHelper();
        int callCount = callbackHelper.getCallCount();
        SignalAccumulator accumulator = new SignalAccumulator(mHandler, mMockTab, actionProviders);
        accumulator.getSignals(() -> callbackHelper.notifyCalled());
        callbackHelper.waitForCallback(callCount);
        Assert.assertTrue(accumulator.getSignal(AdaptiveToolbarButtonVariant.PRICE_TRACKING));
        Assert.assertFalse(accumulator.getSignal(AdaptiveToolbarButtonVariant.READER_MODE));
        Assert.assertTrue(accumulator.getSignal(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS));
        Assert.assertTrue(accumulator.getSignal(AdaptiveToolbarButtonVariant.DISCOUNTS));
    }

    @Test
    public void testTimeoutBeforeAllSignals() throws TimeoutException {
        final CallbackHelper callbackHelper = new CallbackHelper();
        int callCount = callbackHelper.getCallCount();
        HashMap<Integer, ActionProvider> actionProviders = new HashMap<>();
        actionProviders.put(
                AdaptiveToolbarButtonVariant.PRICE_TRACKING, (tab, signalAccumulator) -> {});
        actionProviders.put(
                AdaptiveToolbarButtonVariant.READER_MODE, (tab, signalAccumulator) -> {});
        actionProviders.put(
                AdaptiveToolbarButtonVariant.PRICE_INSIGHTS, (tab, signalAccumulator) -> {});
        actionProviders.put(AdaptiveToolbarButtonVariant.DISCOUNTS, (tab, signalAccumulator) -> {});
        SignalAccumulator accumulator = new SignalAccumulator(mHandler, mMockTab, actionProviders);
        accumulator.getSignals(() -> callbackHelper.notifyCalled());
        callbackHelper.waitForCallback(callCount);
        Assert.assertFalse(accumulator.getSignal(AdaptiveToolbarButtonVariant.PRICE_TRACKING));
        Assert.assertFalse(accumulator.getSignal(AdaptiveToolbarButtonVariant.READER_MODE));
        Assert.assertFalse(accumulator.getSignal(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS));
        Assert.assertFalse(accumulator.getSignal(AdaptiveToolbarButtonVariant.DISCOUNTS));
    }

    @Test
    public void testTimeout() {
        HashMap<Integer, ActionProvider> actionProviders = new HashMap<>();
        ActionProvider actionProvider =
                (tab, signalAccumulator) -> {
                    // Supply all signals and notify controller.
                    signalAccumulator.setSignal(AdaptiveToolbarButtonVariant.READER_MODE, false);
                };
        actionProviders.put(AdaptiveToolbarButtonVariant.READER_MODE, actionProvider);
        SignalAccumulator accumulator = new SignalAccumulator(mHandler, mMockTab, actionProviders);
        accumulator.getSignals(CallbackUtils.emptyRunnable());
        verify(mHandler).postDelayed(any(), eq(300L));
    }
}
