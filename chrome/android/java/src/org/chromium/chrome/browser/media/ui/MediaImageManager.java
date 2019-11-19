// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.FileUtils;
import org.chromium.content_public.browser.ImageDownloadCallback;
import org.chromium.content_public.browser.WebContents;
import org.chromium.services.media_session.MediaImage;

import java.util.Iterator;
import java.util.List;

/**
 * A class for managing the MediaImage download process.
 *
 * The manager takes a list of {@link MediaMetadata.MediaImage} as input, and
 * selects one of them based on scoring and start download through
 * {@link WebContents} asynchronously. When the download successfully finishes,
 * the manager runs the callback function to notify the completion and pass the
 * downloaded Bitmap.
 *
 * The scoring works as follows:
 * - A image score is computed by multiplying the type score with the size score.
 * - The type score lies in [0, 1] and is based on the image MIME type/file extension.
 *   - PNG and JPEG are prefered than others.
 *   - If unspecified, use the default type score (0.6).
 * - The size score lies in [0, 1] and is computed by multiplying the dominant size score and aspect
 *   ratio score:
 *   - The dominant size score lies in [0, 1] and is computed using |mMinimumSize| and |mIdealSize|:
 *     - If size < |mMinimumSize| (too small), the size score is 0.
 *     - If |mMinimumSize| <= size <= |mIdealSize|, the score increases linearly from 0.2 to 1.
 *     - If size > |mIdealSize|, the score is |mIdealSize| / size, which drops from 1 to 0.
 *     - When the size is "any", the size score is 0.8.
 *     - If unspecified, use the default size score (0.4).
 *   - The aspect ratio score lies in [0, 1] and is computed by dividing the short edge length by
 *     the long edge.
 */
public class MediaImageManager implements ImageDownloadCallback {
    // The default score of unknown image size.
    private static final double DEFAULT_IMAGE_SIZE_SCORE = 0.4;
    // The scores for different image types. Keep them sorted by value.
    private static final double TYPE_SCORE_DEFAULT = 0.6;
    private static final double TYPE_SCORE_PNG = 1.0;
    private static final double TYPE_SCORE_JPEG = 0.7;
    private static final double TYPE_SCORE_BMP = 0.5;
    private static final double TYPE_SCORE_XICON = 0.4;
    private static final double TYPE_SCORE_GIF = 0.3;

    @VisibleForTesting
    static final int MAX_BITMAP_SIZE_FOR_DOWNLOAD = 2048;

    private WebContents mWebContents;
    // The minimum image size. Images that are smaller than |mMinimumSize| will be ignored.
    final int mMinimumSize;
    // The ideal image size. Images that are too large than |mIdealSize| will be ignored.
    final int mIdealSize;
    // The pending download image request id, which is set when calling
    // {@link WebContents#downloadImage()}, and reset when image download completes or
    // {@link #clearRequests()} is called.
    private int mRequestId;
    // The callback to be called when the pending download image request completes.
    private MediaImageCallback mCallback;

    // The last image src for download, used for avoiding fetching the same src when artwork is set
    // multiple times but the same src is chosen.
    //
    // Will be reset when initiating a new download request.
    private String mLastImageSrc;

    /**
     * MediaImageManager constructor.
     * @param minimumSize The minimum size of images to download.
     * @param idealSize The ideal size of images to download.
     */
    public MediaImageManager(int minimumSize, int idealSize) {
        mMinimumSize = minimumSize;
        mIdealSize = idealSize;
        clearRequests();
    }

    /**
     * Called when the WebContent changes.
     * @param contents The new WebContents.
     */
    public void setWebContents(WebContents contents) {
        mWebContents = contents;
        clearRequests();
    }

    /**
     * Select the best image from |images| and start download.
     * @param images The list of images to choose from. Null is equivalent to empty list.
     * @param callback The callback when image download completes.
     */
    public void downloadImage(List<MediaImage> images, MediaImageCallback callback) {
        if (mWebContents == null) return;

        mCallback = callback;
        MediaImage image = selectImage(images);
        if (image == null) {
            mLastImageSrc = null;
            mCallback.onImageDownloaded(null);
            clearRequests();
            return;
        }

        // Avoid fetching the same image twice.
        if (TextUtils.equals(image.getSrc(), mLastImageSrc)) return;
        mLastImageSrc = image.getSrc();

        // Limit |maxBitmapSize| to |MAX_BITMAP_SIZE_FOR_DOWNLOAD| to avoid passing huge bitmaps
        // through JNI. |maxBitmapSize| does not prevent huge images to be downloaded. It is used to
        // filter/rescale the download images. See documentation of
        // {@link WebContents#downloadImage()} for details.
        mRequestId = mWebContents.downloadImage(
                image.getSrc(),                // url
                false,                         // isFavicon
                MAX_BITMAP_SIZE_FOR_DOWNLOAD,  // maxBitmapSize
                false,                         // bypassCache
                this);                         // callback
    }

    /**
     * ImageDownloadCallback implementation. This method is called when an download image request is
     * completed. The class will only keep the latest request. If some call to this method is
     * corresponding to a previous request, it will be ignored.
     */
    @Override
    public void onFinishDownloadImage(int id, int httpStatusCode, String imageUrl,
            List<Bitmap> bitmaps, List<Rect> originalImageSizes) {
        if (id != mRequestId) return;

        Iterator<Bitmap> iterBitmap = bitmaps.iterator();
        Iterator<Rect> iterSize = originalImageSizes.iterator();

        Bitmap bestBitmap = null;
        double bestScore = 0;
        while (iterBitmap.hasNext() && iterSize.hasNext()) {
            Bitmap bitmap = iterBitmap.next();
            Rect size = iterSize.next();
            double newScore = getImageSizeScore(size);
            if (bestScore < newScore) {
                bestBitmap = bitmap;
                bestScore = newScore;
            }
        }
        mCallback.onImageDownloaded(bestBitmap);
        clearRequests();
    }

    /**
     * Select the best image from the |images|.
     * @param images The list of images to select from. Null is equivalent to empty list.
     */
    private MediaImage selectImage(List<MediaImage> images) {
        if (images == null) return null;

        MediaImage selectedImage = null;
        double bestScore = 0;
        for (MediaImage image : images) {
            double newScore = getImageScore(image);
            if (newScore > bestScore) {
                bestScore = newScore;
                selectedImage = image;
            }
        }
        return selectedImage;
    }

    private void clearRequests() {
        mRequestId = -1;
        mCallback = null;
    }

    private double getImageScore(MediaImage image) {
        if (image == null) return 0;
        if (image.getSizes().isEmpty()) return DEFAULT_IMAGE_SIZE_SCORE;

        double bestSizeScore = 0;
        for (Rect size : image.getSizes()) {
            bestSizeScore = Math.max(bestSizeScore, getImageSizeScore(size));
        }
        double typeScore = getImageTypeScore(image.getSrc(), image.getType());
        return bestSizeScore * typeScore;
    }

    private double getImageSizeScore(Rect size) {
        return getImageDominantSizeScore(size.width(), size.height())
                * getImageAspectRatioScore(size.width(), size.height());
    }

    private double getImageDominantSizeScore(int width, int height) {
        int dominantSize = Math.max(width, height);
        // When the size is "any".
        if (dominantSize == 0) return 0.8;
        // Ignore images that are too small.
        if (dominantSize < mMinimumSize) return 0;

        if (dominantSize <= mIdealSize) {
            return 0.8 * (dominantSize - mMinimumSize) / (mIdealSize - mMinimumSize) + 0.2;
        }
        return 1.0 * mIdealSize / dominantSize;
    }

    private double getImageAspectRatioScore(int width, int height) {
        double longEdge = Math.max(width, height);
        double shortEdge = Math.min(width, height);
        return shortEdge / longEdge;
    }

    private double getImageTypeScore(String url, String type) {
        String extension = FileUtils.getExtension(url);

        if ("bmp".equals(extension) || "image/bmp".equals(type)) {
            return TYPE_SCORE_BMP;
        } else if ("gif".equals(extension) || "image/gif".equals(type)) {
            return TYPE_SCORE_GIF;
        } else if ("icon".equals(extension) || "image/x-icon".equals(type)) {
            return TYPE_SCORE_XICON;
        } else if ("png".equals(extension) || "image/png".equals(type)) {
            return TYPE_SCORE_PNG;
        } else if ("jpeg".equals(extension) || "jpg".equals(extension)
                || "image/jpeg".equals(type)) {
            return TYPE_SCORE_JPEG;
        }
        return TYPE_SCORE_DEFAULT;
    }
}
