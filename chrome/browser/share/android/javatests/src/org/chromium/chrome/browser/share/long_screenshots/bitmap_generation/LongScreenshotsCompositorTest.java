// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.annotation.NonNull;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.UnguessableToken;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.components.paintpreview.player.CompositorStatus;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
import org.chromium.url.GURL;

/** Test for {@link LongScreenshotsCompositor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@SuppressWarnings("DoNotMock") // Mocks GURL.
public class LongScreenshotsCompositorTest {
    private TestPlayerCompositorDelegate mCompositorDelegate;
    private Bitmap mTestBitmap = Bitmap.createBitmap(512, 1024, Bitmap.Config.ARGB_8888);
    private Rect mRect = new Rect(0, 100, 200, 1100);
    private boolean mErrorThrown;

    @Mock private GURL mTestGurl;

    @Mock private NativePaintPreviewServiceProvider mNativePaintPreviewServiceProvider;

    /** Implementation of {@link PlayerCompositorDelegate.Factory} for tests. */
    class TestCompositorDelegateFactory implements PlayerCompositorDelegate.Factory {
        @Override
        public PlayerCompositorDelegate create(
                NativePaintPreviewServiceProvider service,
                GURL url,
                String directoryKey,
                boolean mainFrameMode,
                @NonNull PlayerCompositorDelegate.CompositorListener compositorListener,
                Callback<Integer> compositorErrorCallback) {
            Assert.fail("create shouldn't be called");
            return null;
        }

        @Override
        public PlayerCompositorDelegate createForCaptureResult(
                NativePaintPreviewServiceProvider service,
                long nativeCaptureResultPtr,
                GURL url,
                String directoryKey,
                boolean mainFrameMode,
                @NonNull PlayerCompositorDelegate.CompositorListener compositorListener,
                Callback<Integer> compositorErrorCallback) {
            return mCompositorDelegate;
        }
    }

    /** Implementation of {@link PlayerCompositorDelegate} for tests. */
    class TestPlayerCompositorDelegate implements PlayerCompositorDelegate {
        private boolean mRequestBitmapError;
        private boolean mWasDestroyed;

        public void setRequestBitmapError() {
            mRequestBitmapError = true;
        }

        public boolean wasDestroyed() {
            return mWasDestroyed;
        }

        @Override
        public void addMemoryPressureListener(Runnable runnable) {}

        @Override
        public int requestBitmap(
                UnguessableToken frameGuid,
                Rect clipRect,
                float scaleFactor,
                Callback<Bitmap> bitmapCallback,
                Runnable errorCallback) {
            Assert.fail("This version of requestBitmap should not be called");
            return 0;
        }

        @Override
        public int requestBitmap(
                Rect clipRect,
                float scaleFactor,
                Callback<Bitmap> bitmapCallback,
                Runnable errorCallback) {
            Assert.assertEquals(mRect, clipRect);
            Assert.assertEquals(1f, scaleFactor, 0);

            if (!mRequestBitmapError) {
                bitmapCallback.onResult(mTestBitmap);
            } else {
                errorCallback.run();
            }
            return 1;
        }

        @Override
        public boolean cancelBitmapRequest(int requestId) {
            return false;
        }

        @Override
        public void cancelAllBitmapRequests() {}

        @Override
        public GURL onClick(UnguessableToken frameGuid, int x, int y) {
            return null;
        }

        @Override
        public void destroy() {
            mWasDestroyed = true;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mCompositorDelegate = new TestPlayerCompositorDelegate();
        LongScreenshotsCompositor.overrideCompositorDelegateFactoryForTesting(
                new TestCompositorDelegateFactory());
        mErrorThrown = false;
    }

    @After
    public void tearDown() {
        LongScreenshotsCompositor.overrideCompositorDelegateFactoryForTesting(null);
    }

    @Test
    public void testSuccessfulCompositing() {
        Callback<Bitmap> onBitmapResult =
                new Callback<Bitmap>() {
                    @Override
                    public void onResult(Bitmap result) {
                        Assert.assertEquals(mTestBitmap, result);
                    }
                };

        Callback<Integer> compositorCallback =
                new Callback<Integer>() {
                    @Override
                    public void onResult(Integer result) {
                        Assert.assertEquals((Integer) CompositorStatus.OK, result);
                    }
                };

        Runnable onErrorCallback =
                new Runnable() {
                    @Override
                    public void run() {
                        Assert.fail("Error should not be thrown");
                    }
                };

        LongScreenshotsCompositor compositor =
                new LongScreenshotsCompositor(
                        mTestGurl,
                        mNativePaintPreviewServiceProvider,
                        "test_directory_key",
                        0,
                        compositorCallback);

        // Mimic the service calling onCompositorReady
        compositor.onCompositorReady(
                null, null, new int[] {1, 2}, new int[] {3, 4}, null, null, null, 0f, 0);
        Assert.assertEquals(1, compositor.getContentSize().getWidth());
        Assert.assertEquals(2, compositor.getContentSize().getHeight());
        Assert.assertEquals(3, compositor.getScrollOffset().x);
        Assert.assertEquals(4, compositor.getScrollOffset().y);

        // RequestBitmap in mCompositorDelegate should match
        compositor.requestBitmap(mRect, 1f, onErrorCallback, onBitmapResult);
        compositor.destroy();
    }

    @Test
    public void testRequestBitmapFailure() {
        mCompositorDelegate.setRequestBitmapError();
        Callback<Bitmap> onBitmapResult =
                new Callback<Bitmap>() {
                    @Override
                    public void onResult(Bitmap result) {
                        Assert.fail("Bitmap should not be returned");
                    }
                };

        Callback<Integer> compositorCallback =
                new Callback<Integer>() {
                    @Override
                    public void onResult(Integer result) {
                        Assert.assertEquals((Integer) CompositorStatus.OK, result);
                    }
                };

        Runnable onErrorCallback =
                new Runnable() {
                    @Override
                    public void run() {
                        mErrorThrown = true;
                    }
                };

        LongScreenshotsCompositor compositor =
                new LongScreenshotsCompositor(
                        mTestGurl,
                        mNativePaintPreviewServiceProvider,
                        "test_directory_key",
                        0,
                        compositorCallback);

        // Mimic the service calling onCompositorReady
        compositor.onCompositorReady(null, null, null, null, null, null, null, 0f, 0);
        Assert.assertEquals(0, compositor.getContentSize().getWidth());

        // RequestBitmap in mCompositorDelegate should match
        compositor.requestBitmap(mRect, 1f, onErrorCallback, onBitmapResult);
        Assert.assertTrue(mErrorThrown);
        compositor.destroy();
        Assert.assertTrue(mCompositorDelegate.wasDestroyed());
    }

    @Test
    public void testCompositorError() {
        Callback<Integer> compositorCallback =
                new Callback<Integer>() {
                    @Override
                    public void onResult(Integer result) {
                        Assert.assertEquals((Integer) CompositorStatus.INVALID_REQUEST, result);
                    }
                };

        LongScreenshotsCompositor compositor =
                new LongScreenshotsCompositor(
                        mTestGurl,
                        mNativePaintPreviewServiceProvider,
                        "test_directory_key",
                        0,
                        compositorCallback);

        compositor.onCompositorError(CompositorStatus.INVALID_REQUEST);
        compositor.destroy();
        Assert.assertTrue(mCompositorDelegate.wasDestroyed());
    }
}
