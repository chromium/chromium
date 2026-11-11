// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiAttributionVerificationSheetProperties.DISMISS_HANDLER;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetType;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Unit tests for {@link AutofillAiAttributionVerificationSheetView} and {@link
 * AutofillAiAttributionVerificationSheetViewBinder}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillAiAttributionVerificationSheetViewTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Runnable mDismissHandler;

    private TestActivity mActivity;
    private PropertyModel mModel;
    private AutofillAiAttributionVerificationSheetView mView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mModel =
                new PropertyModel.Builder(AutofillAiAttributionVerificationSheetProperties.ALL_KEYS)
                        .with(DISMISS_HANDLER, mDismissHandler)
                        .build();
        mView = new AutofillAiAttributionVerificationSheetView(mActivity);
        PropertyModelChangeProcessor.create(
                mModel, mView, AutofillAiAttributionVerificationSheetViewBinder::bind);
    }

    @Test
    public void testContentViewAndToolbar() {
        assertNotNull(mView.getContentView());
        assertNull(mView.getToolbarView());
        assertEquals(0, mView.getVerticalScrollOffset());
    }

    @Test
    public void testSheetTypeAndLifecycle() {
        BottomSheetType expectedType =
                new BottomSheetType.Builder().setUserInitiated(true).setModal(false).build();
        assertEquals(expectedType, mView.getSheetType());
        assertFalse(mView.hasCustomLifecycle());
        assertTrue(mView.hasCustomScrimLifecycle());
        assertEquals(BottomSheetContent.ContentPriority.HIGH, mView.getPriority());
        assertFalse(mView.swipeToDismissEnabled());
        assertTrue(mView.showHandlebar());
    }

    @Test
    public void testHeightModesAndPeekHeight() {
        int expectedPeekHeight =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.autofill_ai_attribution_verification_sheet_peek_height);
        assertEquals(expectedPeekHeight, mView.getPeekHeight());
        assertEquals(
                (float) BottomSheetContent.HeightMode.WRAP_CONTENT,
                mView.getFullHeightRatio(),
                /* delta= */ 0.0f);
        assertEquals(
                (float) BottomSheetContent.HeightMode.DISABLED,
                mView.getHalfHeightRatio(),
                /* delta= */ 0.0f);
    }

    @Test
    public void testAccessibilityStringIds() {
        assertEquals(
                R.string.autofill_ai_attribution_verification_sheet_full_height,
                mView.getSheetFullHeightAccessibilityStringId());
        assertEquals(
                R.string.autofill_ai_attribution_verification_sheet_closed,
                mView.getSheetClosedAccessibilityStringId());
        assertEquals(
                R.string.autofill_ai_attribution_verification_sheet_hidden,
                mView.getSheetHiddenAccessibilityStringId());
        assertNotEquals(
                mView.getSheetClosedAccessibilityStringId(),
                mView.getSheetHiddenAccessibilityStringId());
    }

    @Test
    public void testDestroy_notifiesBoundDismissHandler() {
        mView.destroy();

        verify(mDismissHandler, times(1)).run();
    }
}
