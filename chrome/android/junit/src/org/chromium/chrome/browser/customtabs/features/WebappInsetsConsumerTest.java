// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.os.Build;
import android.view.View;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource;

/** Tests for {@link WebappInsetsConsumer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        sdk = {Build.VERSION_CODES.R, BaseRobolectricTestRunner.MAX_SDK},
        manifest = Config.NONE)
public class WebappInsetsConsumerTest {
    private static final Insets STATUS_BAR_INSETS = Insets.of(0, 100, 0, 0);
    private static final Insets NAV_BAR_INSETS = Insets.of(0, 0, 0, 150);
    private static final Insets DISPLAY_CUTOUT_INSETS = Insets.of(0, 83, 0, 0);
    private static final Insets IME_INSETS = Insets.of(0, 0, 0, 741);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private InsetObserver mInsetObserver;
    @Mock private View mView;

    private WebappInsetsConsumer mConsumer;
    private WindowInsetsCompat mWindowInsets;

    @Before
    public void setUp() {
        mConsumer = new WebappInsetsConsumer(mInsetObserver);
        mWindowInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                        .setInsets(WindowInsetsCompat.Type.navigationBars(), NAV_BAR_INSETS)
                        .setInsets(WindowInsetsCompat.Type.displayCutout(), DISPLAY_CUTOUT_INSETS)
                        .setInsets(WindowInsetsCompat.Type.ime(), IME_INSETS)
                        .build();
    }

    @Test
    public void registersAtWebAppEdgeToEdgeSlot() {
        verify(mInsetObserver)
                .addInsetsConsumer(mConsumer, InsetConsumerSource.WEB_APP_EDGE_TO_EDGE);
    }

    @Test
    public void passesInsetsThroughWhileNotConsuming() {
        WindowInsetsCompat result = mConsumer.onApplyWindowInsets(mView, mWindowInsets);

        assertSame("Insets should be unchanged while not consuming.", mWindowInsets, result);
        verify(mInsetObserver, never()).retriggerOnApplyWindowInsets();
    }

    @Test
    public void consumesSystemBarsAndDisplayCutoutWhileEdgeToEdge() {
        mConsumer.drawEdgeToEdge(true);
        verify(mInsetObserver).retriggerOnApplyWindowInsets();

        WindowInsetsCompat result = mConsumer.onApplyWindowInsets(mView, mWindowInsets);

        assertEquals(
                "System bar insets should be consumed.",
                Insets.NONE,
                result.getInsets(WindowInsetsCompat.Type.systemBars()));
        assertEquals(
                "Display cutout insets should be consumed.",
                Insets.NONE,
                result.getInsets(WindowInsetsCompat.Type.displayCutout()));
        assertEquals(
                "IME insets should never be consumed.",
                IME_INSETS,
                result.getInsets(WindowInsetsCompat.Type.ime()));
    }

    @Test
    public void restoresInsetsWhenConsumptionStops() {
        mConsumer.drawEdgeToEdge(true);
        mConsumer.drawEdgeToEdge(false);

        WindowInsetsCompat result = mConsumer.onApplyWindowInsets(mView, mWindowInsets);

        assertEquals(
                "System bar insets should flow again.",
                mWindowInsets.getInsets(WindowInsetsCompat.Type.systemBars()),
                result.getInsets(WindowInsetsCompat.Type.systemBars()));
        verify(mInsetObserver, org.mockito.Mockito.times(2)).retriggerOnApplyWindowInsets();
    }

    @Test
    public void destroyUnregistersConsumer() {
        mConsumer.destroy();

        verify(mInsetObserver).removeInsetsConsumer(mConsumer);
    }
}
