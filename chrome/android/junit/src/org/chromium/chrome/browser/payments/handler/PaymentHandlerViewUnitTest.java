// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.payments.ui.InputProtector;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link PaymentHandlerView} sheet height behavior. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaymentHandlerViewUnitTest {
    private static final float FLOAT_TOLERANCE = 0.001f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContents mPaymentHandlerWebContents;
    @Mock private InputProtector mInputProtector;

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
    }

    private PaymentHandlerView createView(boolean hasPreferredHeight) {
        return new PaymentHandlerView(
                mActivity,
                mPaymentHandlerWebContents,
                new View(mActivity),
                new View(mActivity),
                mInputProtector,
                hasPreferredHeight);
    }

    @Test
    public void testDefaultHeightRatios() {
        PaymentHandlerView view = createView(/* hasPreferredHeight= */ false);

        assertEquals(
                PaymentHandlerMediator.HALF_HEIGHT_RATIO,
                view.getHalfHeightRatio(),
                FLOAT_TOLERANCE);
        assertEquals(
                PaymentHandlerMediator.FULL_HEIGHT_RATIO,
                view.getFullHeightRatio(),
                FLOAT_TOLERANCE);
    }

    @Test
    public void testWrapContentDisablesHalfHeight() {
        PaymentHandlerView view = createView(/* hasPreferredHeight= */ true);

        assertEquals((float) HeightMode.DISABLED, view.getHalfHeightRatio(), FLOAT_TOLERANCE);
        assertEquals((float) HeightMode.WRAP_CONTENT, view.getFullHeightRatio(), FLOAT_TOLERANCE);
    }
}
