// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.content;

import android.graphics.Bitmap;
import android.os.Handler;
import android.util.Size;
import android.view.PixelCopy;
import android.view.SurfaceView;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.compositor.CompositorView;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.ui.test.util.RenderTestRule;

/** Tests for the {@link TabContentManager}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TabContentManagerTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule.Builder()
                    .setCorpus(RenderTestRule.Corpus.ANDROID_RENDER_TESTS_PUBLIC)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_THUMBNAIL)
                    .setRevision(1)
                    .setDescription("Initial test creation")
                    .build();

    /**
     * With {@link ChromeFeatureList.THUMBNAIL_CACHE_REFACTOR} enabled the live layer is vended to
     * the compositor via a pull mechanism rather than a push mechanism. Ensure the tab still draws
     * its live layer.
     */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "crbug.com/331664814")
    public void testLiveLayerDraws() throws Exception {
        final String testHttpsUrl1 =
                sActivityTestRule.getTestServer().getURL("/chrome/test/data/android/test.html");
        sActivityTestRule.loadUrlInNewTab(testHttpsUrl1);
        mRenderTestRule.compareForResult(captureBitmap(), "contentViewTab1");

        final String testHttpsUrl2 =
                sActivityTestRule.getTestServer().getURL("/chrome/test/data/android/google.html");
        sActivityTestRule.loadUrlInNewTab(testHttpsUrl2);
        mRenderTestRule.compareForResult(captureBitmap(), "contentViewTab2");
    }

    @Test
    @MediumTest
    public void testJpegRefetch() throws Exception {
        final String testHttpsUrl1 =
                sActivityTestRule.getTestServer().getURL("/chrome/test/data/android/google.html");
        sActivityTestRule.loadUrlInNewTab(testHttpsUrl1);

        // Sometimes loadUrlInNewTab returns before the tab is actually loaded. Confirm again.
        final Tab currentTab = sActivityTestRule.getActivity().getActivityTab();
        CriteriaHelper.pollUiThread(() -> !currentTab.isLoading());

        final CallbackHelper helper = new CallbackHelper();
        final Bitmap[] bitmapHolder = new Bitmap[1];
        Callback<Bitmap> bitmapCallback =
                (bitmap) -> {
                    bitmapHolder[0] = bitmap;
                    helper.notifyCalled();
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final TabContentManager tabContentManager =
                            sActivityTestRule.getActivity().getTabContentManagerSupplier().get();
                    final int height = 100;
                    final int width =
                            Math.round(
                                    height
                                            * TabUtils.getTabThumbnailAspectRatio(
                                                    sActivityTestRule.getActivity(),
                                                    sActivityTestRule
                                                            .getActivity()
                                                            .getBrowserControlsManager()));
                    tabContentManager.cacheTabThumbnail(currentTab);
                    tabContentManager.getTabThumbnailWithCallback(
                            currentTab.getId(), new Size(width, height), bitmapCallback);
                });

        helper.waitForOnly();
        Assert.assertNotNull(bitmapHolder[0]);
    }

    /**
     * Generate a bitmap of the {@link SurfaceView} using {@link PixelCopy} as the normal path for
     * capturing bitmaps doesn't work for GPU accelerated content.
     */
    private Bitmap captureBitmap() throws Exception {
        CallbackHelper helper = new CallbackHelper();
        Bitmap[] bitmapHolder = new Bitmap[1];
        CompositorView compositorView =
                ((CompositorViewHolder)
                                sActivityTestRule
                                        .getActivity()
                                        .findViewById(R.id.compositor_view_holder))
                        .getCompositorView();
        Assert.assertNotNull(compositorView);
        // Put the compositor view in a mode that supports readback (this is used by the magnifier
        // normally). Note that this might fail if surface control isn't supported by the GPU under
        // test.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    compositorView.onSelectionHandlesStateChanged(true);
                });
        // TODO(crbug.com/40885026): It unfortunately may take time for the SurfaceView buffer to
        // contain anything and there is no signal to listen to.
        Thread.sleep(1000);
        // Capture the surface using PixelCopy repeating until it works.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SurfaceView surfaceView = (SurfaceView) compositorView.getActiveSurfaceView();
                    Assert.assertNotNull(surfaceView);
                    // Assume surface view size will be constant and only allocate the bitmap once.
                    bitmapHolder[0] =
                            Bitmap.createBitmap(
                                    surfaceView.getWidth(),
                                    surfaceView.getHeight(),
                                    Bitmap.Config.ARGB_8888);
                    captureBitmapInner(compositorView, bitmapHolder, helper, new Handler());
                });
        helper.waitForOnly();
        Assert.assertNotNull(bitmapHolder[0]);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    compositorView.onSelectionHandlesStateChanged(false);
                });
        return bitmapHolder[0];
    }

    private void captureBitmapInner(
            CompositorView compositorView,
            Bitmap[] bitmapHolder,
            CallbackHelper helper,
            Handler handler) {
        SurfaceView surfaceView = (SurfaceView) compositorView.getActiveSurfaceView();
        Assert.assertNotNull(surfaceView);
        PixelCopy.OnPixelCopyFinishedListener listener =
                new PixelCopy.OnPixelCopyFinishedListener() {
                    @Override
                    public void onPixelCopyFinished(int copyResult) {
                        if (copyResult == PixelCopy.SUCCESS) {
                            helper.notifyCalled();
                            return;
                        }
                        // Backoff if the surface buffer isn't working yet. The test will time out
                        // if this takes too long.
                        handler.postDelayed(
                                () -> {
                                    captureBitmapInner(
                                            compositorView, bitmapHolder, helper, handler);
                                },
                                500);
                    }
                };
        PixelCopy.request(surfaceView, bitmapHolder[0], listener, handler);
    }
}
