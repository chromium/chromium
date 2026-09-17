// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.nullValue;
import static org.junit.Assert.assertTrue;

import android.view.View;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link AutofillPaymentsChurnedUsersBottomSheetContent}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillPaymentsChurnedUsersBottomSheetContentTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private AutofillPaymentsChurnedUsersBottomSheetContent mContent;
    private View mContentView;

    @Before
    public void setUp() {
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        activity -> {
                            mContentView = new View(activity);
                            mContent =
                                    new AutofillPaymentsChurnedUsersBottomSheetContent(
                                            mContentView);
                        });
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
        assertThat(mContent.getSheetFullHeightAccessibilityStringId(), equalTo(R.string.ok));
    }

    @Test
    public void testBottomSheetClosedAccessibilityStringId() {
        assertThat(mContent.getSheetClosedAccessibilityStringId(), equalTo(R.string.ok));
    }
}
