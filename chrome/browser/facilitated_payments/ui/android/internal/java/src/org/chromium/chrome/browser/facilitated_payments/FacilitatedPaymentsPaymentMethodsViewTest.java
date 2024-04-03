// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertFalse;

import android.content.Context;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;

/** Unit test for {@link FacilitatedPaymentsPaymentMethodsView}. */
@SmallTest
public final class FacilitatedPaymentsPaymentMethodsViewTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private FacilitatedPaymentsPaymentMethodsView mContent;
    private View mView;

    @Mock private ManagedBottomSheetController mBottomSheetController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.getApplication().getApplicationContext();
        mView = new View(mContext);
        mContent = new FacilitatedPaymentsPaymentMethodsView(mContext, mBottomSheetController);
    }

    @Test
    public void testContentView() {
        assertThat(mContent.getContentView(), equalTo(mView));
    }

    @Test
    public void testBottomSheetHasNoToolbar() {
        assertThat(mContent.getToolbarView(), nullValue());
    }

    @Test
    public void testNoVerticalScrollOffset() {
        assertThat(mContent.getVerticalScrollOffset(), equalTo(0));
    }

    @Test
    public void testBottomSheetPriority() {
        assertThat(mContent.getPriority(), equalTo(BottomSheetContent.ContentPriority.HIGH));
    }

    @Test
    public void testCannotSwipeToDismissBottomSheet() {
        assertFalse(mContent.swipeToDismissEnabled());
    }

    @Test
    public void testBottomSheetAccessibilityContentDescription() {
        assertThat(mContent.getSheetContentDescriptionStringId(), equalTo(R.string.ok));
    }

    @Test
    public void testBottomSheetHalfHeightAccessibilityContentDescription() {
        assertThat(mContent.getSheetHalfHeightAccessibilityStringId(), equalTo(R.string.ok));
    }

    @Test
    public void testBottomSheetFullHeightAccessibilityContentDescription() {
        assertThat(mContent.getSheetFullHeightAccessibilityStringId(), equalTo(R.string.ok));
    }

    @Test
    public void testBottomSheetClosedAccessibilityContentDescription() {
        assertThat(mContent.getSheetClosedAccessibilityStringId(), equalTo(R.string.ok));
    }
}
