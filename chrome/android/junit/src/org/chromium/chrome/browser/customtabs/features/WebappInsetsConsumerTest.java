// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.WindowManager;

import androidx.core.graphics.Insets;
import androidx.core.view.DisplayCutoutCompat;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource;

/** Tests for {@link WebappInsetsConsumer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = {Build.VERSION_CODES.R, BaseRobolectricTestRunner.MAX_SDK})
public class WebappInsetsConsumerTest {
    private static final Insets STATUS_BAR_INSETS = Insets.of(0, 100, 0, 0);
    private static final Insets NAV_BAR_INSETS = Insets.of(0, 0, 0, 150);
    private static final Insets DISPLAY_CUTOUT_INSETS = Insets.of(0, 83, 0, 0);
    private static final Insets IME_INSETS = Insets.of(0, 0, 0, 741);
    private static final Insets TAPPABLE_INSETS = Insets.of(0, 100, 0, 150);
    private static final Insets GESTURE_INSETS = Insets.of(0, 100, 0, 150);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private InsetObserver mInsetObserver;
    private final View mView = new View(ContextUtils.getApplicationContext());

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
                        .setInsets(WindowInsetsCompat.Type.tappableElement(), TAPPABLE_INSETS)
                        .setInsets(
                                WindowInsetsCompat.Type.mandatorySystemGestures(), GESTURE_INSETS)
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
    public void consumesSystemBarsAndDisplayCutoutWhileTokenHeld() {
        mConsumer.acquireEdgeToEdgeToken();
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
        assertEquals(
                "Tappable element insets should be consumed: the edge-to-edge layout uses them"
                        + " as a navigation bar fallback.",
                Insets.NONE,
                result.getInsets(WindowInsetsCompat.Type.tappableElement()));
        assertEquals(
                "Mandatory system gesture insets should be consumed: the edge-to-edge layout"
                        + " uses them as a navigation bar fallback.",
                Insets.NONE,
                result.getInsets(WindowInsetsCompat.Type.mandatorySystemGestures()));
    }

    @Test
    public void restoresInsetsWhenLastTokenIsReleased() {
        int firstToken = mConsumer.acquireEdgeToEdgeToken();
        int secondToken = mConsumer.acquireEdgeToEdgeToken();

        mConsumer.releaseEdgeToEdgeToken(firstToken);
        assertTrue(
                "Insets stay withheld while another token is held.",
                mConsumer.getDrawEdgeToEdgeSupplier().get());

        mConsumer.releaseEdgeToEdgeToken(secondToken);

        WindowInsetsCompat result = mConsumer.onApplyWindowInsets(mView, mWindowInsets);

        assertEquals(
                "System bar insets should flow again.",
                mWindowInsets.getInsets(WindowInsetsCompat.Type.systemBars()),
                result.getInsets(WindowInsetsCompat.Type.systemBars()));
        verify(mInsetObserver, org.mockito.Mockito.times(2)).retriggerOnApplyWindowInsets();
    }

    @Test
    public void keepsDisplayCutoutInsetsWhenWindowNeverDrawsIntoCutout() {
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        activity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER;
        WindowInsetsCompat insetsWithCutout =
                new WindowInsetsCompat.Builder(mWindowInsets)
                        .setDisplayCutout(
                                new DisplayCutoutCompat(
                                        new Rect(0, 83, 0, 0), /* boundingRects= */ null))
                        .build();
        mConsumer.acquireEdgeToEdgeToken();

        WindowInsetsCompat result =
                mConsumer.onApplyWindowInsets(new View(activity), insetsWithCutout);

        assertEquals(
                "System bar insets should be consumed.",
                Insets.NONE,
                result.getInsets(WindowInsetsCompat.Type.systemBars()));
        assertEquals(
                "Display cutout insets should survive so the layout pads the cutout.",
                DISPLAY_CUTOUT_INSETS,
                result.getInsets(WindowInsetsCompat.Type.displayCutout()));
    }

    @Test
    public void defaultCutoutModeEvaluatedOnStrippedInsets() {
        // LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT is the default window attribute.
        Activity activity = Robolectric.buildActivity(Activity.class).get();
        // The cutout (83px) is fully contained within the status bar (100px). The downstream
        // layout only sees the stripped insets (zero-height system bars), so the cutout must
        // survive; deciding on the un-stripped insets would consume it and make the layout jump
        // once the system bars hide.
        WindowInsetsCompat insetsWithCutout =
                new WindowInsetsCompat.Builder(mWindowInsets)
                        .setDisplayCutout(
                                new DisplayCutoutCompat(
                                        new Rect(0, 83, 0, 0), /* boundingRects= */ null))
                        .build();
        mConsumer.acquireEdgeToEdgeToken();

        WindowInsetsCompat result =
                mConsumer.onApplyWindowInsets(new View(activity), insetsWithCutout);

        assertEquals(
                "System bar insets should be consumed.",
                Insets.NONE,
                result.getInsets(WindowInsetsCompat.Type.systemBars()));
        assertEquals(
                "Display cutout insets should survive so the layout pads the cutout.",
                DISPLAY_CUTOUT_INSETS,
                result.getInsets(WindowInsetsCompat.Type.displayCutout()));
    }

    @Test
    public void destroyUnregistersConsumer() {
        mConsumer.destroy();

        verify(mInsetObserver).removeInsetsConsumer(mConsumer);
    }
}
