// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.test.util.ChromeRestriction.RESTRICTION_TYPE_SVR;

import android.os.Build;
import android.support.test.filters.MediumTest;
import android.view.ViewGroup;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.vr.rules.ChromeTabbedActivityVrTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.atomic.AtomicInteger;

/**
 * End-to-end tests for the CompositorViewHolder's behavior while in VR.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(RESTRICTION_TYPE_SVR)
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // VR is only supported on L+.
public class VrBrowserCompositorViewHolderTest {
    // We explicitly instantiate a rule here instead of using parameterization since this class
    // only ever runs in ChromeTabbedActivity.
    @Rule
    public ChromeTabbedActivityVrTestRule mVrTestRule = new ChromeTabbedActivityVrTestRule();

    /**
     * Verify that resizing the CompositorViewHolder does not cause the current tab to resize while
     * the CompositorViewHolder is detached from the TabModelSelector. See crbug.com/680240.
     */
    @Test
    @MediumTest
    public void testResizeWithCompositorViewHolderDetached() {
        final AtomicInteger oldWidth = new AtomicInteger();
        final AtomicInteger oldHeight = new AtomicInteger();
        final int testWidth = 123;
        final int testHeight = 456;
        final WebContents webContents = mVrTestRule.getWebContents();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            CompositorViewHolder compositorViewHolder =
                    (CompositorViewHolder) mVrTestRule.getActivity().findViewById(
                            R.id.compositor_view_holder);
            compositorViewHolder.onEnterVr();

            oldWidth.set(webContents.getWidth());
            oldHeight.set(webContents.getHeight());

            ViewGroup.LayoutParams layoutParams = compositorViewHolder.getLayoutParams();
            layoutParams.width = testWidth;
            layoutParams.height = testHeight;
            compositorViewHolder.requestLayout();
        });
        CriteriaHelper.pollUiThread(() -> {
            return mVrTestRule.getActivity()
                           .findViewById(R.id.compositor_view_holder)
                           .getMeasuredWidth()
                    == testWidth;
        }, "CompositorViewHolder width did not match the requested layout width");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(
                    "Viewport width changed when resizing a detached CompositorViewHolder",
                    webContents.getWidth(), oldWidth.get());
            Assert.assertEquals(
                    "Viewport height changed when resizing a detached CompositorViewHolder",
                    webContents.getHeight(), oldHeight.get());

            CompositorViewHolder compositorViewHolder =
                    (CompositorViewHolder) mVrTestRule.getActivity().findViewById(
                            R.id.compositor_view_holder);
            compositorViewHolder.onExitVr();
            Assert.assertNotEquals(
                    "Viewport width did not change after CompositorViewHolder re-attached",
                    webContents.getHeight(), oldHeight.get());
            Assert.assertNotEquals(
                    "Viewport height did not change after CompositorViewHolder re-attached",
                    webContents.getWidth(), oldWidth.get());
        });
    }
}
