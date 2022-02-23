// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.app.Activity;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

/**
 * Tests for {@link ContinuousSearchChipView}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ContinuousSearchChipViewTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;
    private ContinuousSearchChipView mChipView;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { sActivity = sActivityTestRule.getActivity(); });
    }

    @Before
    public void setUpTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mChipView = sActivity.getLayoutInflater()
                                .inflate(R.layout.continuous_search_list_item, null)
                                .findViewById(R.id.csn_chip);
        });
    }

    @Test
    @SmallTest
    public void testTwoLineChipInitialized() {
        mChipView.initTwoLineChipView();
        TextView primaryText = mChipView.getPrimaryTextView();
        TextView secondaryText = mChipView.getSecondaryTextView();
        Assert.assertTrue(primaryText.getParent() instanceof LinearLayout);
        LinearLayout layout = (LinearLayout) primaryText.getParent();
        Assert.assertEquals(LinearLayout.VERTICAL, layout.getOrientation());
        Assert.assertEquals(layout, secondaryText.getParent());
        Assert.assertEquals("Secondary text should be placed right after the primary text", 1,
                layout.indexOfChild(secondaryText) - layout.indexOfChild(primaryText));
    }
}
