// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo.bottom_sheet;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;

/** Unit tests for {@link SafetyPromoBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SafetyPromoBottomSheetContentUnitTest {
    private static final float FLOATING_POINT_DELTA = 0.1f;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mOnDestroyRunnable;

    private View mContentView;
    private SafetyPromoBottomSheetContent mBottomSheetContent;

    @Before
    public void setUp() {
        Context context = ApplicationProvider.getApplicationContext();
        mContentView = new View(context);
        mBottomSheetContent = new SafetyPromoBottomSheetContent(mContentView, mOnDestroyRunnable);
    }

    @Test
    public void testBasics() {
        assertSame(mContentView, mBottomSheetContent.getContentView());
        assertNull(mBottomSheetContent.getToolbarView());
        assertEquals(ContentPriority.HIGH, mBottomSheetContent.getPriority());
        assertTrue(mBottomSheetContent.swipeToDismissEnabled());
        assertEquals(
                HeightMode.DISABLED,
                mBottomSheetContent.getHalfHeightRatio(),
                FLOATING_POINT_DELTA);
        assertEquals(
                HeightMode.WRAP_CONTENT,
                mBottomSheetContent.getFullHeightRatio(),
                FLOATING_POINT_DELTA);
    }

    @Test
    public void testDestroy() {
        mBottomSheetContent.destroy();

        verify(mOnDestroyRunnable).run();
    }
}
