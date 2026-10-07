// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tips.TipsPromoProperties.FeatureTipPromoData;
import org.chromium.ui.base.WindowAndroid;

import java.util.Map;

/** Unit tests for {@link TipsPromoHandlerFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TipsPromoHandlerFactoryUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TipsPromoHandler.Provider mProviderA;
    @Mock private TipsPromoHandler.Provider mProviderB;

    private WindowAndroid mWindowAndroid;

    /** A minimal handler whose only behaviour is returning a fixed set of custom signals. */
    private static class SignalOnlyHandler implements TipsPromoHandler {
        private final Map<String, Float> mSignals;

        SignalOnlyHandler(Map<String, Float> signals) {
            mSignals = signals;
        }

        @Override
        public FeatureTipPromoData getPromoData(Context context) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void onPromoAccepted() {}

        @Override
        public Map<String, Float> getCustomSignals(WindowAndroid windowAndroid) {
            return mSignals;
        }
    }

    /** A handler that relies entirely on the interface defaults. */
    private static class DefaultHandler implements TipsPromoHandler {
        @Override
        public FeatureTipPromoData getPromoData(Context context) {
            throw new UnsupportedOperationException();
        }

        @Override
        public void onPromoAccepted() {}
    }

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mWindowAndroid = new WindowAndroid(activity, /* occlusionTrackingAllowed= */ false);
    }

    @After
    public void tearDown() {
        TipsPromoHandlerFactory.resetForTesting();
        mWindowAndroid.destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testGetHandler_Registered() {
        TipsPromoHandler handler = new DefaultHandler();
        when(mProviderA.get()).thenReturn(handler);
        TipsPromoHandlerFactory.register(TipsNotificationsFeatureType.GOOGLE_LENS, mProviderA);

        assertEquals(
                handler,
                TipsPromoHandlerFactory.getHandler(TipsNotificationsFeatureType.GOOGLE_LENS));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testGetHandler_Unregistered() {
        assertNull(TipsPromoHandlerFactory.getHandler(TipsNotificationsFeatureType.GOOGLE_LENS));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testGetHandler_SelfServiceDisabled() {
        when(mProviderA.get()).thenReturn(new DefaultHandler());
        TipsPromoHandlerFactory.register(TipsNotificationsFeatureType.GOOGLE_LENS, mProviderA);

        assertNull(TipsPromoHandlerFactory.getHandler(TipsNotificationsFeatureType.GOOGLE_LENS));
        verify(mProviderA, never()).get();
    }

    @Test
    public void testGetCustomSignals_DefaultIsEmpty() {
        Map<String, Float> signals = new DefaultHandler().getCustomSignals(mWindowAndroid);
        assertNotNull(signals);
        assertTrue(signals.isEmpty());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testCollectCustomSignals_NoProviders() {
        Map<String, Float> signals = TipsPromoHandlerFactory.collectCustomSignals(mWindowAndroid);
        assertNotNull(signals);
        assertTrue(signals.isEmpty());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testCollectCustomSignals_MergesAcrossProviders() {
        // Keys are required to be unique per feature, so this exercises merging of distinct keys
        // (including multiple keys from one handler), not collision precedence.
        when(mProviderA.get())
                .thenReturn(new SignalOnlyHandler(Map.of("signal_a1", 1.0f, "signal_a2", 2.0f)));
        when(mProviderB.get()).thenReturn(new SignalOnlyHandler(Map.of("signal_b", 3.0f)));
        TipsPromoHandlerFactory.register(TipsNotificationsFeatureType.GOOGLE_LENS, mProviderA);
        TipsPromoHandlerFactory.register(TipsNotificationsFeatureType.BOTTOM_OMNIBOX, mProviderB);

        Map<String, Float> signals = TipsPromoHandlerFactory.collectCustomSignals(mWindowAndroid);

        assertEquals(Map.of("signal_a1", 1.0f, "signal_a2", 2.0f, "signal_b", 3.0f), signals);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testCollectCustomSignals_SkipsHandlersWithoutSignals() {
        when(mProviderA.get()).thenReturn(new DefaultHandler());
        when(mProviderB.get()).thenReturn(new SignalOnlyHandler(Map.of("signal_b", 3.0f)));
        TipsPromoHandlerFactory.register(TipsNotificationsFeatureType.GOOGLE_LENS, mProviderA);
        TipsPromoHandlerFactory.register(TipsNotificationsFeatureType.BOTTOM_OMNIBOX, mProviderB);

        Map<String, Float> signals = TipsPromoHandlerFactory.collectCustomSignals(mWindowAndroid);

        assertEquals(Map.of("signal_b", 3.0f), signals);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testCollectCustomSignals_ForwardsWindowAndroid() {
        TipsPromoHandler handler = mock(TipsPromoHandler.class);
        when(handler.getCustomSignals(mWindowAndroid)).thenReturn(Map.of("signal_a", 1.0f));
        when(mProviderA.get()).thenReturn(handler);
        TipsPromoHandlerFactory.register(TipsNotificationsFeatureType.GOOGLE_LENS, mProviderA);

        Map<String, Float> signals = TipsPromoHandlerFactory.collectCustomSignals(mWindowAndroid);

        verify(handler).getCustomSignals(mWindowAndroid);
        assertEquals(Map.of("signal_a", 1.0f), signals);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testCollectCustomSignals_SelfServiceDisabled() {
        when(mProviderA.get()).thenReturn(new SignalOnlyHandler(Map.of("signal_a", 1.0f)));
        TipsPromoHandlerFactory.register(TipsNotificationsFeatureType.GOOGLE_LENS, mProviderA);

        Map<String, Float> signals = TipsPromoHandlerFactory.collectCustomSignals(mWindowAndroid);

        assertTrue(signals.isEmpty());
        verify(mProviderA, never()).get();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testCollectCustomSignals_SkipsProviderReturningNullHandler() {
        // Mocks are outside NullAway's reach and can violate the non-null contract.
        when(mProviderA.get()).thenReturn(null);
        when(mProviderB.get()).thenReturn(new SignalOnlyHandler(Map.of("signal_b", 3.0f)));
        TipsPromoHandlerFactory.register(TipsNotificationsFeatureType.GOOGLE_LENS, mProviderA);
        TipsPromoHandlerFactory.register(TipsNotificationsFeatureType.BOTTOM_OMNIBOX, mProviderB);

        Map<String, Float> signals = TipsPromoHandlerFactory.collectCustomSignals(mWindowAndroid);

        // The null handler is skipped without throwing and other providers still contribute.
        assertEquals(Map.of("signal_b", 3.0f), signals);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TIPS_SELF_SERVICE)
    public void testCollectCustomSignals_SkipsHandlerReturningNullSignals() {
        // Mocks are outside NullAway's reach and can violate the non-null contract.
        TipsPromoHandler nullSignalsHandler = mock(TipsPromoHandler.class);
        when(nullSignalsHandler.getCustomSignals(mWindowAndroid)).thenReturn(null);
        when(mProviderA.get()).thenReturn(nullSignalsHandler);
        when(mProviderB.get()).thenReturn(new SignalOnlyHandler(Map.of("signal_b", 3.0f)));
        TipsPromoHandlerFactory.register(TipsNotificationsFeatureType.GOOGLE_LENS, mProviderA);
        TipsPromoHandlerFactory.register(TipsNotificationsFeatureType.BOTTOM_OMNIBOX, mProviderB);

        Map<String, Float> signals = TipsPromoHandlerFactory.collectCustomSignals(mWindowAndroid);

        // The null signal map is skipped without throwing and other providers still contribute.
        assertEquals(Map.of("signal_b", 3.0f), signals);
    }
}
