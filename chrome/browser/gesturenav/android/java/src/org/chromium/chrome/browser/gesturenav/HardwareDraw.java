// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.HardwareRenderer;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RenderNode;
import android.media.Image;
import android.media.ImageReader;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.Surface;
import android.view.View;

import androidx.annotation.RequiresApi;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.resources.dynamics.CaptureObserver;
import org.chromium.ui.resources.dynamics.CaptureUtils;

import java.nio.ByteBuffer;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;

/**
 * Uses a {@link RenderNode} to perform bitmap capture of a java View. This walks the View hierarchy
 * synchronously, populating a list of instructions. Then, on a separate thread,the instructions are
 * executed to paint colors onto a {@link Bitmap}. Uses functionality that requires Android Q+.
 */
@RequiresApi(Build.VERSION_CODES.Q)
@NullMarked
public class HardwareDraw {

    /**
     * A Renderer manages a single draw request. It uses a HardwareRenderer to produce a frame from
     * a RenderNode. The Renderer then consumes the frame via onImageAvailable, converting it to a
     * bitmap and posting the callback to the UI thread. An ImageReader pipes the producer to the
     * consumer. On the thread pool, the Renderer requests and blocks waiting the producer to
     * produce a frame, but the producer itself runs on a hidden Android render thread. The consumer
     * part of the Renderer runs on a dedicated thread.
     *
     * <p>RenderNode was added in API level 29 (Android 10). So restrict Renderer as well.
     */
    @RequiresApi(Build.VERSION_CODES.Q)
    private static class Renderer implements ImageReader.OnImageAvailableListener {
        private final ThreadUtils.ThreadChecker mUiThreadChecker;

        // An ImageReader requires a listener to run in a separate thread.
        // Ideally, we would just post to the thread pool, but it doesn't implement a Handler like
        // the ImageReader requires.
        private static @Nullable Handler sHardwareCallbackThreadHandler;

        // A dedicated thread to issue HardwareRenderer requests from.
        //
        // If task issuing the hardware rendering request is posted to the global thread pool, it
        // results in the worker thread running the task getting added to the ADPF session and since
        // Android assumes that these requests originate from the UI thread. Android might then
        // prioritize the worker thread as though it were the UI thread, which is undesirable for
        // performance. Having a dedicated thread to issue HardwareRenderer requests from mitigates
        // this problem by preventing contamination of the global thread pool.
        private static @Nullable Executor sHardwareRequestThreadExecutor;

        // Only ever recreated in the UI thread.
        private final ImageReader mImageReader;

        // Set in the UI thread before enqueuing a request.
        // Cleared in the hardware thread after posting the task back to the UI thread.
        private @Nullable Callback<@Nullable Bitmap> mOnBitmapCapture;

        /**
         * Each instance should be called by external clients only on the thread it is created. The
         * first instance created will also create a thread to acquire rendered images and a thread
         * to issue hardware accelerated render requests to the OS.
         */
        private Renderer(ThreadUtils.ThreadChecker uiThreadChecker, int width, int height) {
            mUiThreadChecker = uiThreadChecker;
            if (sHardwareCallbackThreadHandler == null) {
                HandlerThread thread = new HandlerThread("HardwareDrawCallbackThread");
                thread.start();
                sHardwareCallbackThreadHandler = new Handler(thread.getLooper());
            }

            if (sHardwareRequestThreadExecutor == null) {
                sHardwareRequestThreadExecutor = Executors.newSingleThreadExecutor();
            }

            try (TraceEvent e = TraceEvent.scoped("Renderer::initImageReader")) {
                // maxAcquiredImages is the number of images that can be acquired concurrently, and
                // it doesn't correlate to the number of buffers available to the producer to write
                // to.
                // When selecting a safe value for this, we considered two things:
                // (a) How the producer behaves when the consumer is late to acquire and close
                // maxAcquiredImages. We experimented by sending multiple draw requests to the
                // HardwareRenderer while holding the only acquirable image in the ImageReader and
                // we didn't observe any crashes or UI freezes.
                // (b) How many draw requests we want to handle in a single Renderer. We decided
                // to only handle one and free up all of resources after posting the bitmap.
                // As long as we are acquiring and closing an image in a single thread before ever
                // acquiring another one, we only need one image in the ImageReader.
                final int maxAcquiredImages = 1;
                mImageReader =
                        ImageReader.newInstance(
                                width, height, PixelFormat.RGBA_8888, maxAcquiredImages);
                mImageReader.setOnImageAvailableListener(this, sHardwareCallbackThreadHandler);
            }
        }

        // Posts a single draw request to the thread pool. It should only be called once.
        private void requestDraw(
                RenderNode renderNode, Callback<@Nullable Bitmap> onBitmapCapture) {
            mUiThreadChecker.assertOnValidThread();
            assert mOnBitmapCapture == null;
            mOnBitmapCapture = onBitmapCapture;
            assumeNonNull(sHardwareRequestThreadExecutor)
                    .execute(
                            () -> {
                                try (TraceEvent e =
                                        TraceEvent.scoped("Renderer::requestDraw::task")) {
                                    HardwareRenderer renderer = new HardwareRenderer();
                                    Surface s = mImageReader.getSurface();
                                    renderer.setContentRoot(renderNode);
                                    renderer.setSurface(s);
                                    HardwareRenderer.FrameRenderRequest request =
                                            renderer.createRenderRequest();
                                    // Block until the frame is submitted to the surface, so that it
                                    // is safe to discard all resources afterwards.
                                    request.setWaitForPresent(true);
                                    request.syncAndDraw();
                                    renderer.stop();
                                    renderer.destroy();
                                    renderNode.discardDisplayList();
                                }
                            });
        }

        /**
         * This method runs on the sHardwareCallbackThreadHandler. It acquires an image and releases
         * it after copying it to a bitmap. Then posts the callback to the UI thread. Ignores any
         * subsequent calls.
         */
        @Override
        public void onImageAvailable(ImageReader reader) {
            try (TraceEvent e = TraceEvent.scoped("Renderer::onImageAvailable")) {
                if (mOnBitmapCapture == null) {
                    // Sometimes Android posts more than one onImageAvailable for a single draw
                    // request. If this happens, we can ignore the subsequent calls.
                    return;
                }
                Image image = reader.acquireNextImage();
                Bitmap result = null;
                if (image != null) {
                    result = toBitmap(image);
                }
                PostTask.postTask(TaskTraits.UI_USER_VISIBLE, mOnBitmapCapture.bind(result));
                // Allow new draw requests to come through.
                mOnBitmapCapture = null;
                // We won't reuse the image reader, so it's safe to free resources now.
                mImageReader.close();
            }
        }

        private static Bitmap toBitmap(Image image) {
            Image.Plane[] planes = image.getPlanes();
            assert planes.length != 0;
            ByteBuffer buffer = planes[0].getBuffer();
            assert buffer != null;

            final int width = image.getWidth();
            final int height = image.getHeight();
            final int pixelStride = planes[0].getPixelStride();
            final int rowPaddingInBytes = planes[0].getRowStride() - pixelStride * width;
            final int rowPaddingInPixels = rowPaddingInBytes / pixelStride;
            // Set bitmap to use the correct padding so it will copy from the buffer.
            Bitmap snapshot = CaptureUtils.createBitmap(width + rowPaddingInPixels, height);
            snapshot.copyPixelsFromBuffer(buffer);
            // Free up the buffer as soon as possible.
            image.close();
            if (rowPaddingInPixels == 0) {
                return snapshot;
            }
            // Crop out the row padding that the Image contained.
            return Bitmap.createBitmap(snapshot, 0, 0, width, height);
        }
    }

    private final ThreadUtils.ThreadChecker mUiThreadChecker = new ThreadUtils.ThreadChecker();

    private @Nullable Renderer mRenderer;

    private boolean mPendingDraw;

    /**
     * Each instance should be called by external clients only on the thread it is created. The
     * first instance created will also create a thread to acquire rendered images.
     */
    public HardwareDraw() {}

    /**
     * This uses a RecordingNode to store all the required draw instructions without doing them
     * upfront. And then on a threadpool task we grab a hardware canvas (required to use a
     * RenderNode) and draw it using the hardware accelerated canvas.
     *
     * @return If draw instructions were recorded.
     */
    public boolean startBitmapCapture(
            View view,
            int height,
            float scale,
            CaptureObserver observer,
            Callback<@Nullable Bitmap> onBitmapCapture) {
        try (TraceEvent e = TraceEvent.scoped("HardwareDraw::startBitmapCapture")) {
            mUiThreadChecker.assertOnValidThread();
            if (view.getWidth() == 0 || view.getHeight() == 0) {
                // We haven't actually laid out this view yet no point in requesting a screenshot.
                return false;
            }
            if (mPendingDraw) {
                // Only allow one request at a time.
                return false;
            }
            int scaledWidth = (int) (view.getWidth() * scale);
            int scaledHeight = (int) (height * scale);
            mRenderer = new Renderer(mUiThreadChecker, scaledWidth, scaledHeight);

            RenderNode renderNode = new RenderNode("bitmapRenderNode");
            renderNode.setPosition(0, 0, view.getWidth(), height);
            Canvas canvas = renderNode.beginRecording();
            boolean captureSuccess =
                    CaptureUtils.captureCommon(
                            canvas,
                            view,
                            /* dirtyRect= */ new Rect(),
                            scale,
                            /* drawWhileDetached= */ false,
                            observer);
            renderNode.endRecording();

            if (captureSuccess) {
                mPendingDraw = true;
                mRenderer.requestDraw(
                        renderNode,
                        (@Nullable Bitmap bitmap) -> {
                            mUiThreadChecker.assertOnValidThread();
                            onBitmapCapture.onResult(bitmap);
                            mPendingDraw = false;
                        });
            }
            return captureSuccess;
        }
    }
}
