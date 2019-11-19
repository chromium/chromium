// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.support.test.filters.SmallTest;
import android.support.test.rule.ActivityTestRule;
import android.view.View;
import android.widget.FrameLayout;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivity;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.CountDownLatch;

/** Tests for {@link FirstDrawDetector}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class FirstDrawDetectorTest {
    @Rule
    public ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    @Test
    @SmallTest
    public void testFirstDraw() throws Exception {
        final CountDownLatch firstDrawEvent = new CountDownLatch(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            DummyUiActivity activity = mActivityTestRule.getActivity();
            View view = new FrameLayout(activity);
            activity.setContentView(view);

            FirstDrawDetector.waitForFirstDraw(view, () -> firstDrawEvent.countDown());
        });
        firstDrawEvent.await();
    }
}
