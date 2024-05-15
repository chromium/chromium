// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;
import android.widget.ScrollView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Unit test for {@link AutofillVcnEnrollBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
@SmallTest
public final class AutofillVcnEnrollBottomSheetContentTest {
    private static final String PROPERTY_ROBOLECTRIC_USE_REAL_SCROLLING
        = "robolectric.useRealScrolling";

    private View mContentView;
    private ScrollView mScrollView;
    private boolean mDismissed;
    private AutofillVcnEnrollBottomSheetContent mContent;
    private boolean mIsRealScrollingEnabled;

    @Before
    public void setUp() {
        mIsRealScrollingEnabled = Boolean.parseBoolean(
            System.getProperty(PROPERTY_ROBOLECTRIC_USE_REAL_SCROLLING));
        // Disable Robolectric's real scrolling for legacy compatibility.
        // We need to remove it after migrating tests to real scrolling.
        System.setProperty(PROPERTY_ROBOLECTRIC_USE_REAL_SCROLLING, "false");
        Activity activity = Robolectric.buildActivity(Activity.class).create().get();
        mContentView = new View(activity);
        mScrollView = new ScrollView(activity);
        mDismissed = false;
        mContent =
                new AutofillVcnEnrollBottomSheetContent(mContentView, mScrollView, this::onDismiss);
    }

    @After
    public void tearDown() {
        System.setProperty(PROPERTY_ROBOLECTRIC_USE_REAL_SCROLLING,
            mIsRealScrollingEnabled ? "true" : "false");
    }

    private void onDismiss() {
        mDismissed = true;
    }

    @Test
    public void testContentView() {
        assertThat(mContent.getContentView(), equalTo(mContentView));
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
    public void testVerticalScrollOffset() {
        mScrollView.setScrollY(1337);

        assertThat(mContent.getVerticalScrollOffset(), equalTo(1337));
    }

    @Test
    public void testDismissBottomSheet() {
        mContent.destroy();

        assertTrue(mDismissed);
    }

    @Test
    public void testBottomSheetPriority() {
        assertThat(mContent.getPriority(), equalTo(BottomSheetContent.ContentPriority.HIGH));
    }

    @Test
    public void testHasCustomLifecycle() {
        assertTrue(mContent.hasCustomLifecycle());
    }

    @Test
    public void testCannotSwipeToDismissBottomSheet() {
        assertThat(mContent.swipeToDismissEnabled(), equalTo(false));
    }

    @Test
    public void testBottomSheetAccessibilityContentDescription() {
        assertThat(
                mContent.getSheetContentDescriptionStringId(),
                equalTo(R.string.autofill_virtual_card_enroll_content_description));
    }

    @Test
    public void testBottomSheetFullHeightAccessibilityDescription() {
        assertThat(
                mContent.getSheetFullHeightAccessibilityStringId(),
                equalTo(R.string.autofill_virtual_card_enroll_full_height_content_description));
    }

    @Test
    public void testBottomSheetClosedAccessibilityDescription() {
        assertThat(
                mContent.getSheetClosedAccessibilityStringId(),
                equalTo(R.string.autofill_virtual_card_enroll_closed_description));
    }

    @Test
    public void testBottomSheetCannotPeek() {
        assertThat(mContent.getPeekHeight(), equalTo(BottomSheetContent.HeightMode.DISABLED));
    }

    @Test
    public void testContentDeterminesBottomSheetHeight() {
        assertThat(
                mContent.getFullHeightRatio(),
                equalTo((float) BottomSheetContent.HeightMode.WRAP_CONTENT));
    }
}
