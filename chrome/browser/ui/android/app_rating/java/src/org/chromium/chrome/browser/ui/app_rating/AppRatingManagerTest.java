// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.app_rating;

import android.app.Activity;

import com.google.android.play.core.review.testing.FakeReviewManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link AppRatingManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AppRatingManagerTest {
    private AppRatingManager mAppRatingManager;
    private Activity mActivity;
    private boolean mOnCompleteCalled;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        FakeReviewManager fakeReviewManager = new FakeReviewManager(mActivity);
        mAppRatingManager = new AppRatingManagerImpl(fakeReviewManager);
        mOnCompleteCalled = false;
    }

    @Test
    public void testRequestAndShowReviewFlow_Success() {
        mAppRatingManager.requestAndShowReviewFlow(mActivity, () -> mOnCompleteCalled = true);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        Assert.assertTrue("Callback should be invoked.", mOnCompleteCalled);
    }
}
