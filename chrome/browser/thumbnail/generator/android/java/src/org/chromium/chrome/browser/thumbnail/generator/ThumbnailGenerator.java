// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thumbnail.generator;

import android.graphics.Bitmap;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * This class generates thumbnails for a given {@link ThumbnailRequest} by calling the native
 * {@link ThumbnailProviderGenerator}, which is owned and destroyed by the Java class. Multiple
 * {@link ThumbnailProvider.ThumbnailRequest}s may be processed at a time.
 *
 * After {@link ThumbnailGenerator#destroy()}, assume that this class will not be called again.
 */
@NullMarked
public class ThumbnailGenerator {
    @JniType("::ThumbnailGenerator")
    interface NativeThumbnailGenerator extends JniTypeToken {}

    private @Nullable JniUniquePtr<NativeThumbnailGenerator> mNativeThumbnailGenerator;

    private JniPtr<NativeThumbnailGenerator> getNativeThumbnailGenerator() {
        if (mNativeThumbnailGenerator == null) {
            mNativeThumbnailGenerator = ThumbnailGeneratorJni.get().init(this);
        }
        return mNativeThumbnailGenerator;
    }

    /**
     * Asynchronously generates the requested thumbnail.
     * @param request The request for a thumbnail.
     * @param callback The class to call back to after thumbnail has been generated.
     */
    public void retrieveThumbnail(
            ThumbnailProvider.ThumbnailRequest request, ThumbnailGeneratorCallback callback) {
        ThreadUtils.assertOnUiThread();
        boolean hasFilePath = !TextUtils.isEmpty(request.getFilePath());
        assert hasFilePath;
        ThumbnailGeneratorJni.get()
                .retrieveThumbnail(
                        getNativeThumbnailGenerator(),
                        request.getContentId(),
                        request.getFilePath(),
                        request.getMimeType(),
                        request.getIconSize(),
                        callback);
    }

    /** Destroys the native {@link ThumbnailGenerator}. */
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        if (mNativeThumbnailGenerator == null) return;
        mNativeThumbnailGenerator.destroy();
        mNativeThumbnailGenerator = null;
    }

    /**
     * Called when thumbnail has been generated.
     *
     * @param contentId Content ID of the requested thumbnail.
     * @param requestedIconSizePx Requested size (maximum required dimension (pixel) of the smaller
     *     side) of the requested thumbnail.
     * @param bitmap The requested thumbnail.
     * @param callback The class to call back to after thumbnail has been generated.
     */
    @CalledByNative
    @VisibleForTesting
    void onThumbnailRetrieved(
            String contentId,
            int requestedIconSizePx,
            @JniType("SkBitmap") @Nullable Bitmap bitmap,
            ThumbnailGeneratorCallback callback) {
        // The bitmap returned here is retrieved from the native side. The image decoder there
        // scales down the image (if it is too big) so that one of its sides is smaller than or
        // equal to the required size. We check here that the returned image satisfies this
        // criteria.
        assert bitmap == null
                || Math.min(bitmap.getWidth(), bitmap.getHeight()) <= requestedIconSizePx;

        callback.onThumbnailRetrieved(contentId, bitmap, requestedIconSizePx);
    }

    @NativeMethods
    interface Natives {
        JniUniquePtr<NativeThumbnailGenerator> init(ThumbnailGenerator self);

        void retrieveThumbnail(
                JniPtr<NativeThumbnailGenerator> self,
                @Nullable String contentId,
                @JniType("std::string") @Nullable String filePath,
                @JniType("std::string") @Nullable String mimeType,
                int thumbnailSize,
                ThumbnailGeneratorCallback callback);
    }
}
