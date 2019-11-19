// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import android.graphics.Rect;
import android.os.Build;
import android.support.test.filters.SmallTest;
import android.view.SurfaceView;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.resources.ResourceManager;

/**
 * Integration tests for {@link org.chromium.chrome.browser.compositor.CompositorView}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
public class CompositorVisibilityTest {
    @Rule
    public ChromeActivityTestRule<? extends ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule(ChromeTabbedActivity.class);

    private CompositorView mCompositorView;

    private LayoutRenderHost mRenderHost = new LayoutRenderHost() {
        @Override
        public void requestRender() {}

        @Override
        public void onCompositorLayout() {}

        @Override
        public void didSwapFrame(int pendingFrameCount) {}

        @Override
        public void onSurfaceCreated() {}

        @Override
        public void onSurfaceResized(int width, int height) {}

        @Override
        public void pushDebugRect(Rect rect, int color) {}

        @Override
        public void loadPersitentTextureDataIfNeeded() {}

        @Override
        public int getBrowserControlsBackgroundColor() {
            return 0;
        }

        @Override
        public ResourceManager getResourceManager() {
            return null;
        }

        @Override
        public void invalidateAccessibilityProvider() {}
    };

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    // Verify that setVisibility on |mCompositorView| is transferred to its children.  Otherwise,
    // the underlying surface is not destroyed.  This can interfere with VR, which hides the
    // CompositorView and creates its own surfaces.  The compositor surfaces can show up when the VR
    // surfaces are supposed to be visible.
    @Test
    @SmallTest
    public void testSetVisibilityHidesSurfaces() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mCompositorView = new CompositorView(mActivityTestRule.getActivity(), mRenderHost);
                mCompositorView.setVisibility(View.VISIBLE);
                Assert.assertEquals(View.VISIBLE, mCompositorView.getChildAt(0).getVisibility());
                mCompositorView.setVisibility(View.INVISIBLE);
                Assert.assertEquals(View.INVISIBLE, mCompositorView.getChildAt(0).getVisibility());
            }
        });
    }

    // The surfaceview should be attached during construction, so that the application window knows
    // to set the blending hint correctly on the surface.  Otherwise, it will have to setFormat()
    // when the SurfaceView is attached to the CompositorView, which causes visual artifacts when
    // the surface is torn down and re-created (crbug.com/704866).
    @Test
    @SmallTest
    public void testSurfaceViewIsAttachedImmediately() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mCompositorView = new CompositorView(mActivityTestRule.getActivity(), mRenderHost);
                Assert.assertEquals(mCompositorView.getChildCount(), 1);
                Assert.assertTrue(mCompositorView.getChildAt(0) instanceof SurfaceView);
            }
        });
    }

    // CompositorView placeholder initial visibility should be true to show white placeholder when
    // needed, but SurfaceView initial visibility should be false to delay
    // surfaceChanged/surfaceCreated calls.
    @Test
    @SmallTest
    public void testInitialVisibility() throws Throwable {
        mActivityTestRule.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mCompositorView = new CompositorView(mActivityTestRule.getActivity(), mRenderHost);
                Assert.assertEquals(View.VISIBLE, mCompositorView.getVisibility());
                Assert.assertEquals(View.INVISIBLE, mCompositorView.getChildAt(0).getVisibility());
            }
        });
    }
}
