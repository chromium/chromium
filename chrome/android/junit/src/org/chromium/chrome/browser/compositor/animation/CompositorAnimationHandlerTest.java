// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.animation;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.MockLayoutUpdateHost;

/**
 * Unit tests for {@link CompositorAnimator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CompositorAnimationHandlerTest {
    private static final long FAST_DURATION_MS = 100;
    private static final long SLOW_DURATION_MS = 1000;

    private CompositorAnimationHandler mAnimations;
    private LayoutUpdateHost mUpdateHost;

    @Test
    @SmallTest
    public void testConcurrentAnimationsFinishSeparately() {
        mUpdateHost = new MockLayoutUpdateHostWithAnimationHandler(mAnimations);
        mAnimations = new CompositorAnimationHandler(mUpdateHost);

        CompositorAnimator mFastAnimation =
                CompositorAnimator.ofFloat(mAnimations, 0.f, 1.f, FAST_DURATION_MS, null);
        CompositorAnimator mSlowAnimation =
                CompositorAnimator.ofFloat(mAnimations, 0.f, 1.f, SLOW_DURATION_MS, null);

        mFastAnimation.start();
        mSlowAnimation.start();

        CompositorAnimationHandler.setTestingMode(true);

        // Advances time to check that the fast animation will finish first.
        mAnimations.pushUpdateInTestingMode(1 + FAST_DURATION_MS);
        Assert.assertFalse(mFastAnimation.isRunning());
        Assert.assertTrue(mSlowAnimation.isRunning());

        // Advances time to check that all animations are finished.
        mAnimations.pushUpdateInTestingMode(1 + SLOW_DURATION_MS);
        Assert.assertFalse(mFastAnimation.isRunning());
        Assert.assertFalse(mSlowAnimation.isRunning());
    }

    /** A mock implementation of {@link LayoutUpdateHost} with animation handler. */
    private class MockLayoutUpdateHostWithAnimationHandler extends MockLayoutUpdateHost {
        private CompositorAnimationHandler mAnimationHandler;

        MockLayoutUpdateHostWithAnimationHandler(CompositorAnimationHandler animationHandler) {
            mAnimationHandler = animationHandler;
        }

        @Override
        public CompositorAnimationHandler getAnimationHandler() {
            return mAnimationHandler;
        }
    }
}