// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Unit tests for {@link AutofillWalletReminderNoticeBottomSheetContent} */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillWalletReminderNoticeBottomSheetContentTest {
    private Activity mActivity;
    private AutofillWalletReminderNoticeBottomSheetContent mContent;
    private View mContentView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContentView = new View(mActivity);
        mContent = new AutofillWalletReminderNoticeBottomSheetContent(mContentView);
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
    public void testHasCustomLifecycle() {
        assertTrue(mContent.hasCustomLifecycle());
    }

    @Test
    public void testBottomSheetPriority() {
        assertThat(mContent.getPriority(), equalTo(BottomSheetContent.ContentPriority.HIGH));
    }

    @Test
    public void testSwipeToDismissEnabled() {
        assertTrue(mContent.swipeToDismissEnabled());
    }

    @Test
    public void testHalfHeightRatio() {
        assertThat(
                mContent.getHalfHeightRatio(),
                equalTo((float) BottomSheetContent.HeightMode.DISABLED));
    }

    @Test
    public void testFullHeightRatio() {
        assertThat(
                mContent.getFullHeightRatio(),
                equalTo((float) BottomSheetContent.HeightMode.WRAP_CONTENT));
    }

    @Test
    public void testBottomSheetFullHeightAccessibilityStringId() {
        assertThat(
                mContent.getSheetFullHeightAccessibilityStringId(),
                equalTo(R.string.autofill_wallet_reminder_notice_bottom_sheet_full_height));
    }

    @Test
    public void testBottomSheetClosedAccessibilityStringId() {
        assertThat(
                mContent.getSheetClosedAccessibilityStringId(),
                equalTo(R.string.autofill_wallet_reminder_notice_bottom_sheet_closed));
    }
}
