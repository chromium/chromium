// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;


import android.graphics.Bitmap;
import android.graphics.Rect;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.paint_preview.common.proto.PaintPreview.PaintPreviewProto;
import org.chromium.components.paintpreview.browser.NativePaintPreviewServiceProvider;
import org.chromium.components.paintpreview.player.CompositorStatus;
import org.chromium.components.paintpreview.player.PlayerCompositorDelegate;
import org.chromium.url.GURL;

/**
 * Test for {@link LongScreenshotsCompositor}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures(ChromeFeatureList.CHROME_SHARE_LONG_SCREENSHOT)
public class LongScreenshotsCompositorTest {
    private TestPlayerCompositorDelegate mCompositorDelegate;
    private Bitmap mTestBitmap = Bitmap.createBitmap(512, 1024, Bitmap.Config.ARGB_8888);
    private Rect mRect = new Rect(0, 100, 200, 1100);
    private boolean mErrorThrown;

    @Mock
    private GURL mTestGurl;

    @Mock
    private NativePaintPreviewServiceProvider mNativePaintPreviewServiceProvider;

    /**
     * Implementation of {@link PlayerCompositorDelegate.Factory} for tests.
     */
    class TestCompositorDelegateFactory implements PlayerCompositorDelegate.Factory {
        @Override
        public PlayerCompositorDelegate create(NativePaintPreviewServiceProvider service, GURL url,
                String directoryKey, boolean mainFrameMode,
                @NonNull PlayerCompositorDelegate.CompositorListener compositorListener,
                Callback<Integer> compositorErrorCallback) {
            Assert.fail("create shouldn't be called");
            return null;
        }

        @Override
        public PlayerCompositorDelegate createForProto(NativePaintPreviewServiceProvider service,
                @Nullable PaintPreviewProto proto, GURL url, String directoryKey,
                boolean mainFrameMode,
                @NonNull PlayerCompositorDelegate.CompositorListener compositorListener,
                Callback<Integer> compositorErrorCallback) {
            return mCompositorDelegate;
        }
    }

    /**
     * Implementation of {@link PlayerCompositorDelegate} for tests. TODO(tgupta): Consider moving
     * this into its own class when it starts to get used more.
     */
    class TestPlayerCompositorDelegate implements PlayerCompositorDelegate {
        private boolean mRequestBitmapError;

        public void setRequestBitmapError() {
            mRequestBitmapError = true;
        }

        @Override
        public void addMemoryPressureListener(Runnable runnable) {}

        @Override
        public int requestBitmap(UnguessableToken frameGuid, Rect clipRect, float scaleFactor,
                Callback<Bitmap> bitmapCallback, Runnable errorCallback) {
            Assert.fail("This version of requestBitmap should not be called");
            return 0;
        }

        @Override
        public int requestBitmap(Rect clipRect, float scaleFactor, Callback<Bitmap> bitmapCallback,
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
    public void testSuccessfullCompositing() {
        Callback<Bitmap> onBitmapResult = new Callback<Bitmap>() {
            @Override
            public void onResult(Bitmap result) {
                Assert.assertEquals(mTestBitmap, result);
            }
        };

        Callback<Integer> compositorCallback = new Callback<Integer>() {
            @Override
            public void onResult(Integer result) {
                Assert.assertEquals((Integer) CompositorStatus.OK, result);
            }
        };

        Runnable onErrorCallback = new Runnable() {
            @Override
            public void run() {
                Assert.fail("Error should not be thrown");
            }
        };

        LongScreenshotsCompositor compositor = new LongScreenshotsCompositor(mTestGurl,
                mNativePaintPreviewServiceProvider, "test_directory_key",
                PaintPreviewProto.getDefaultInstance(), compositorCallback);

        // Mimic the service calling onCompositorReady
        compositor.onCompositorReady(null, null, null, null, null, null, null, 0);

        // RequestBitmap in mCompositorDelegate should match
        compositor.requestBitmap(mRect, onErrorCallback, onBitmapResult);
    }

    @Test
    public void testRequestBitmapFailure() {
        mCompositorDelegate.setRequestBitmapError();
        Callback<Bitmap> onBitmapResult = new Callback<Bitmap>() {
            @Override
            public void onResult(Bitmap result) {
                Assert.fail("Bitmap should not be returned");
            }
        };

        Callback<Integer> compositorCallback = new Callback<Integer>() {
            @Override
            public void onResult(Integer result) {
                Assert.assertEquals((Integer) CompositorStatus.OK, result);
            }
        };

        Runnable onErrorCallback = new Runnable() {
            @Override
            public void run() {
                mErrorThrown = true;
            }
        };

        LongScreenshotsCompositor compositor = new LongScreenshotsCompositor(mTestGurl,
                mNativePaintPreviewServiceProvider, "test_directory_key",
                PaintPreviewProto.getDefaultInstance(), compositorCallback);

        // Mimic the service calling onCompositorReady
        compositor.onCompositorReady(null, null, null, null, null, null, null, 0);

        // RequestBitmap in mCompositorDelegate should match
        compositor.requestBitmap(mRect, onErrorCallback, onBitmapResult);
        Assert.assertTrue(mErrorThrown);
    }
}
