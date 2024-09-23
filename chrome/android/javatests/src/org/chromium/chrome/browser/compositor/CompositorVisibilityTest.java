// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor;

import android.view.SurfaceView;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.ui.resources.ResourceManager;

/** Integration tests for {@link org.chromium.chrome.browser.compositor.CompositorView}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class CompositorVisibilityTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private CompositorView mCompositorView;

    private LayoutRenderHost mRenderHost =
            new LayoutRenderHost() {
                @Override
                public void requestRender() {}

                @Override
                public void onCompositorLayout() {}

                @Override
                public void didSwapFrame(int pendingFrameCount) {}

                @Override
                public void onSurfaceResized(int width, int height) {}

                @Override
                public ResourceManager getResourceManager() {
                    return null;
                }

                @Override
                public void invalidateAccessibilityProvider() {}
            };

    // Verify that setVisibility on |mCompositorView| is transferred to its children.  Otherwise,
    // the underlying surface is not destroyed.  This can interfere with VR, which hides the
    // CompositorView and creates its own surfaces.  The compositor surfaces can show up when the VR
    // surfaces are supposed to be visible.
    @Test
    @SmallTest
    public void testSetVisibilityHidesSurfaces() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        mCompositorView =
                                new CompositorView(sActivityTestRule.getActivity(), mRenderHost);
                        mCompositorView.setVisibility(View.VISIBLE);
                        Assert.assertEquals(
                                View.VISIBLE, mCompositorView.getChildAt(0).getVisibility());
                        mCompositorView.setVisibility(View.INVISIBLE);
                        Assert.assertEquals(
                                View.INVISIBLE, mCompositorView.getChildAt(0).getVisibility());
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
        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        mCompositorView =
                                new CompositorView(sActivityTestRule.getActivity(), mRenderHost);
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
        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        mCompositorView =
                                new CompositorView(sActivityTestRule.getActivity(), mRenderHost);
                        Assert.assertEquals(View.VISIBLE, mCompositorView.getVisibility());
                        Assert.assertEquals(
                                View.INVISIBLE, mCompositorView.getChildAt(0).getVisibility());
                    }
                });
    }
}
