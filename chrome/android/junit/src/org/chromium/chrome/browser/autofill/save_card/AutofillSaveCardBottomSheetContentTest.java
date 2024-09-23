// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertEquals;
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

/** Unit tests for {@link AutofillSaveCardBottomSheetContent} */
@SmallTest
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillSaveCardBottomSheetContentTest {
    private static final String PROPERTY_ROBOLECTRIC_USE_REAL_SCROLLING
        = "robolectric.useRealScrolling";

    private View mContentView;
    private ScrollView mScrollView;
    private AutofillSaveCardBottomSheetContent mContent;
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
        mContent = new AutofillSaveCardBottomSheetContent(mContentView, mScrollView);
    }

    @After
    public void tearDown() {
        System.setProperty(PROPERTY_ROBOLECTRIC_USE_REAL_SCROLLING,
            mIsRealScrollingEnabled ? "true" : "false");
    }

    @Test
    public void testContentView() {
        assertEquals(mContentView, mContent.getContentView());
    }

    @Test
    public void testNoToolbarView() {
        assertThat(mContent.getToolbarView(), nullValue());
    }

    @Test
    public void testVerticalScrollOffset() {
        mScrollView.setScrollY(24);

        assertEquals(24, mContent.getVerticalScrollOffset());
    }

    @Test
    public void testVerticalScrollOffset_whenNotSet() {
        assertEquals(0, mContent.getVerticalScrollOffset());
    }

    @Test
    public void testCustomLifecycle() {
        assertTrue(mContent.hasCustomLifecycle());
    }

    @Test
    public void testSwipeToDismissEnabled() {
        assertTrue(mContent.swipeToDismissEnabled());
    }

    @Test
    public void testPriority() {
        assertEquals(BottomSheetContent.ContentPriority.HIGH, mContent.getPriority());
    }

    @Test
    public void testFullHeightRatio() {
        assertEquals(
                BottomSheetContent.HeightMode.WRAP_CONTENT,
                mContent.getFullHeightRatio(),
                /* delta= */ 0);
    }

    @Test
    public void testHalfHeightRatio() {
        assertEquals(
                BottomSheetContent.HeightMode.DISABLED,
                mContent.getHalfHeightRatio(),
                /* delta= */ 0);
    }

    @Test
    public void testPeekHeight() {
        assertEquals(
                BottomSheetContent.HeightMode.DISABLED, mContent.getPeekHeight(), /* delta= */ 0);
    }

    @Test
    public void testHideOnScroll() {
        assertTrue(mContent.hideOnScroll());
    }

    @Test
    public void testSheetContentDescription() {
        assertEquals(
                R.string.autofill_save_card_prompt_bottom_sheet_content_description,
                mContent.getSheetContentDescriptionStringId());
    }

    @Test
    public void testSheetFullHeightAccessibilityString() {
        assertEquals(
                R.string.autofill_save_card_prompt_bottom_sheet_full_height,
                mContent.getSheetFullHeightAccessibilityStringId());
    }

    @Test
    public void testSheetClosedAccessibilityString() {
        assertEquals(
                R.string.autofill_save_card_prompt_bottom_sheet_closed,
                mContent.getSheetClosedAccessibilityStringId());
    }
}
