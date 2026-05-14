// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ui.autofill.AtMemoryBottomSheetView;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

/** Render tests for AtMemoryBottomSheet. */
@LargeTest
@Batch(Batch.UNIT_TESTS)
@RunWith(ChromeJUnit4ClassRunner.class)
public class AtMemoryBottomSheetRenderTest {
    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(Component.UI_BROWSER_AUTOFILL)
                    .build();

    private Activity mActivity;
    private AtMemoryBottomSheetView mView;

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(/* startIntent= */ null);
        runOnUiThreadBlocking(
                () -> {
                    mActivity = mActivityTestRule.getActivity();
                    mView = new AtMemoryBottomSheetView(mActivity);
                    mActivity.setContentView(mView.getContentView());
                });
    }

    @Test
    @Feature({"RenderTest"})
    public void testRenderBottomSheet() throws Exception {
        ViewGroup activityContentView = mActivity.findViewById(android.R.id.content);
        mRenderTestRule.render(activityContentView, "at_memory_bottom_sheet");
    }
}
