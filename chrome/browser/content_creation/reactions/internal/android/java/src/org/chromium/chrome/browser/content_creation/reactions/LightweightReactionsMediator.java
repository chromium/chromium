// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.net.Uri;
import android.util.Size;

import org.chromium.base.Callback;
import org.chromium.base.StreamUtil;
import org.chromium.chrome.browser.content_creation.reactions.scene.SceneCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareImageFileUtils.FileOutputStreamWriter;
import org.chromium.components.content_creation.reactions.ReactionMetadata;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.third_party.glide.gif_encoder.AnimatedGifEncoder;

import java.util.List;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Mediator for the Lightweight Reactions component.
 */
public class LightweightReactionsMediator {
    /**
     * Interface for generating a GIF from a view.
     */
    public interface GifGeneratorHost {
        /**
         * Invoked when the encoder is ready to encode the next frame. The implementer should
         * perform all necessary work to have the view ready to draw the next GIF frame to a canvas,
         * and then invoke the provided callback when the frame is ready.
         */
        public void prepareFrame(Callback<Void> cb);

        /**
         * Invoked when the encoder wants the implementer to draw the next GIF frame to the provided
         * canvas.
         */
        public void drawFrame(Canvas canvas);
    }

    // Field trial params
    private static final String SHOULD_LOAD_REACTIONS_ON_DEMAND_PARAM =
            "should_load_reactions_on_demand";

    // GIF encoding constants
    private static final String GIF_FILE_EXT = ".gif";
    private static final int GIF_FPS = 24;
    private static final int GIF_QUALITY = 15; // Range is 1 (best) to 20 (worst/fastest).
    private static final int GIF_REPEAT = 0; // Infinite repeat.
    private static final int GIF_MAX_DIMENSION_PX = 900;

    private final ImageFetcher mImageFetcher;

    private boolean mAssetFetchCancelled;
    private boolean mGifGenerationCancelled;
    private int mFramesGenerated;

    public LightweightReactionsMediator(ImageFetcher imageFetcher) {
        mImageFetcher = imageFetcher;
    }

    /**
     * Fetches the image at the given URL and invokes the given callback with a {@link Bitmap}
     * representing it.
     */
    public void getBitmapForUrl(String url, Callback<Bitmap> callback) {
        mImageFetcher.fetchImage(
                ImageFetcher.Params.create(url, ImageFetcher.LIGHTWEIGHT_REACTIONS_UMA_CLIENT_NAME),
                bitmap -> { callback.onResult(bitmap); });
    }

    /**
     * Fetches the GIF at the given URL and invokes the given callback with the fetched asset
     * as a {@link BaseGifImage}.
     */
    public void getGifForUrl(String url, Callback<BaseGifImage> callback) {
        mImageFetcher.fetchGif(
                ImageFetcher.Params.create(url, ImageFetcher.LIGHTWEIGHT_REACTIONS_UMA_CLIENT_NAME),
                gifImage -> { callback.onResult(gifImage); });
    }

    /**
     * Fetches the thumbnail and GIF payload for each given reaction. When done, the given callback
     * is invoked with a list of thumbnails with the same order as the given reaction list. The GIF
     * assets are not returned, but the image fetcher will cache them on disk for instant access
     * when needed by the UI.
     */
    public void fetchAssetsAndGetThumbnails(
            List<ReactionMetadata> reactions, Callback<Bitmap[]> callback) {
        if (callback == null) {
            return;
        }

        if (reactions == null || reactions.isEmpty()) {
            callback.onResult(null);
            return;
        }

        mAssetFetchCancelled = false;
        boolean shouldLoadReactionsOnDemand =
                ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.LIGHTWEIGHT_REACTIONS,
                        SHOULD_LOAD_REACTIONS_ON_DEMAND_PARAM, false);

        // Keep track of the number of callbacks received. Need a final instance because the
        // counter is updated from within a callback. If the full reactions are not loaded on
        // demand (loaded here), multiply the number of reactions by 2 as both the thumbnails and
        // full reactions are loaded.
        int expectedCalls = shouldLoadReactionsOnDemand ? reactions.size() : reactions.size() * 2;
        final Counter counter = new Counter(expectedCalls);

        // Also use a final array to keep track of the thumbnails fetched so far. Initialize it with
        // null refs so the fetched bitmaps can be inserted at the right index.
        final Bitmap[] thumbnails = new Bitmap[reactions.size()];

        for (int i = 0; i < reactions.size(); ++i) {
            // Capture the loop index to a final instance for use in the callback.
            final int index = i;

            ReactionMetadata reaction = reactions.get(i);
            getBitmapForUrl(reaction.thumbnailUrl, bitmap -> {
                if (mAssetFetchCancelled) {
                    return;
                }
                if (bitmap == null) {
                    mAssetFetchCancelled = true;
                    callback.onResult(null);
                    return;
                }

                thumbnails[index] = bitmap;
                counter.increment();

                if (counter.isDone()) {
                    callback.onResult(thumbnails);
                }
            });
            if (!shouldLoadReactionsOnDemand) {
                getGifForUrl(reaction.assetUrl, gif -> {
                    if (mAssetFetchCancelled) {
                        return;
                    }
                    if (gif == null) {
                        mAssetFetchCancelled = true;
                        callback.onResult(null);
                        return;
                    }

                    counter.increment();

                    if (counter.isDone()) {
                        callback.onResult(thumbnails);
                    }
                });
            }
        }
    }

    /**
     * Encodes a GIF based on the given {@link GifGeneratorHost}, and invokes the given callback
     * with the URI to the temporary GIF file for sharing.
     *
     * @param host The {@link GifGeneratorHost} to use for generating the GIF frames.
     * @param fileName The shared GIF's file name.
     * @param sceneCoordinator The {@link SceneCoordinator} to restart the animations when
     *         cancelling and to get the dimensions.
     * @param progressDialog The {@link LightweightReactionsProgressDialog} to update the progress
     *         dialog.
     * @param doneCallback The callback to invoke when the final GIF is ready. The callback is
     *                     passed the Uri to the temporary GIF file that was generated.
     */
    public void generateGif(GifGeneratorHost host, String fileName,
            SceneCoordinator sceneCoordinator, LightweightReactionsProgressDialog progressDialog,
            Callback<Uri> doneCallback) {
        mGifGenerationCancelled = false;
        final long generationStartTime = System.currentTimeMillis();
        progressDialog.setCancelProgressListener(view -> {
            mGifGenerationCancelled = true;
            int frameCount = sceneCoordinator.getFrameCount();
            assert frameCount != 0;
            int completion = (int) (100.0 * mFramesGenerated / sceneCoordinator.getFrameCount());
            LightweightReactionsMetrics.recordGifGenerationCancelled(
                    System.currentTimeMillis() - generationStartTime, completion);
        });
        FileOutputStreamWriter gifWriter = (fos, gifCallback) -> {
            AnimatedGifEncoder encoder = new AnimatedGifEncoder();
            encoder.setFrameRate(GIF_FPS);
            encoder.setQuality(GIF_QUALITY);
            encoder.setRepeat(GIF_REPEAT);
            encoder.start(fos);

            // The encoder will keep invoking the host's prepareFrame() and drawFrame() until this
            // many frames have been generated.
            int frameCount = sceneCoordinator.getFrameCount();
            assert frameCount != 0;
            mFramesGenerated = 0;

            // For performance reasons, the result might need to be scaled down for devices with
            // very large display sizes. Determine the scale factor based on the largest dimension
            // of the background screenshot and the maximum output dimension.
            Size screenshotSize = sceneCoordinator.getScreenshotDisplaySize();
            int screenshotWidth = screenshotSize.getWidth();
            int screenshotHeight = screenshotSize.getHeight();
            int largestDimension = Math.max(screenshotWidth, screenshotHeight);
            float scaleFactor = largestDimension <= GIF_MAX_DIMENSION_PX
                    ? 1f
                    : (float) GIF_MAX_DIMENSION_PX / largestDimension;
            int scaledScreenshotWidth = (int) (screenshotWidth * scaleFactor);
            int scaledScreenshotHeight = (int) (screenshotHeight * scaleFactor);

            // The raw frames need to be cropped on the sides to account for the grey background
            // bars. Calculate the X offset at which to start the crop. Also remember the scene
            // dimensions for drawing the raw frames.
            int sceneWidth = sceneCoordinator.getSceneWidth();
            int sceneHeight = sceneCoordinator.getSceneHeight();
            int scaledSceneWidth = (int) (sceneWidth * scaleFactor);
            int scaledSceneHeight = (int) (sceneHeight * scaleFactor);
            int cropOffsetX = (scaledSceneWidth - scaledScreenshotWidth) / 2;

            Callback<Void> prepareFrameCallback = new Callback<Void>() {
                @Override
                public void onResult(Void v) {
                    if (mGifGenerationCancelled) {
                        if (progressDialog.getDialog().isShowing()) {
                            progressDialog.getDialog().dismiss();
                            sceneCoordinator.startAnimations();
                        }
                        encoder.finish();
                        StreamUtil.closeQuietly(fos);
                        return;
                    }

                    // The next frame is ready to be drawn and encoded. Use ARGB_8888 config for the
                    // bitmap, which is a standard configuration that allows transparency.
                    Bitmap rawFrame = Bitmap.createBitmap(
                            scaledSceneWidth, scaledSceneHeight, Bitmap.Config.ARGB_8888);
                    Canvas canvas = new Canvas(rawFrame);
                    canvas.scale(scaleFactor, scaleFactor);
                    host.drawFrame(canvas);
                    Bitmap croppedFrame = Bitmap.createBitmap(rawFrame, cropOffsetX, 0,
                            scaledScreenshotWidth, scaledScreenshotHeight);
                    encoder.addFrame(croppedFrame);
                    ++mFramesGenerated;
                    progressDialog.setProgress((int) (100.0 * mFramesGenerated / frameCount));

                    if (mFramesGenerated >= frameCount) {
                        boolean success = encoder.finish();
                        gifCallback.onResult(success);
                    } else {
                        host.prepareFrame(this);
                    }
                }
            };

            host.prepareFrame(prepareFrameCallback);
        };

        ShareImageFileUtils.generateTemporaryUriFromStream(
                fileName, gifWriter, GIF_FILE_EXT, doneCallback);
    }

    /**
     * Simple counter class used to keep track of the number of images being
     * asynchronously loaded.
     */
    private class Counter {
        private int mRemainingCalls;

        Counter(int expectedCalls) {
            mRemainingCalls = expectedCalls;
        }

        void increment() {
            --mRemainingCalls;
        }

        boolean isDone() {
            return mRemainingCalls == 0;
        }
    }
}
