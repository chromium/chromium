// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Bitmap;
import android.net.Uri;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.embedder_support.contextmenu.ContextMenuImageFormat;
import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

@NullMarked
class ContextMenuNativeDelegateImpl implements ContextMenuNativeDelegate {
    private static final int MAX_SHARE_DIMEN_PX = 2048;

    private final RenderFrameHost mRenderFrameHost;

    private long mNativePtr;

    /** See function for details. */
    private static byte @Nullable [] sHardcodedImageBytesForTesting;

    private static @Nullable String sHardcodedImageExtensionForTesting;

    /**
     * The tests trigger the context menu via JS rather than via a true native call which means
     * the native code does not have a reference to the image's RenderFrameHost. Instead allow
     * test cases to hardcode the test image bytes that will be shared.
     * @param hardcodedImageBytes The hard coded image bytes to fake or null if image should not be
     *         faked.
     * @param hardcodedImageExtension The hard coded image extension.
     */
    public static void setHardcodedImageBytesForTesting(
            byte[] hardcodedImageBytes, String hardcodedImageExtension) {
        sHardcodedImageBytesForTesting = hardcodedImageBytes;
        sHardcodedImageExtensionForTesting = hardcodedImageExtension;
    }

    public ContextMenuNativeDelegateImpl(
            WebContents webContents,
            RenderFrameHost renderFrameHost,
            ContextMenuParams contextMenuParams) {
        mRenderFrameHost = renderFrameHost;
        mNativePtr = ContextMenuNativeDelegateImplJni.get().init(webContents, contextMenuParams);
    }

    @Override
    public void destroy() {
        mNativePtr = 0;
    }

    @Override
    public void retrieveImageForShare(int imageFormat, Callback<Uri> callback) {
        if (mNativePtr == 0) return;

        Callback<ImageCallbackResult> imageRetrieveCallback =
                (result) -> {
                    ShareImageFileUtils.generateTemporaryUriFromData(
                            result.imageData, result.extension, callback);
                };

        if (sHardcodedImageBytesForTesting != null) {
            imageRetrieveCallback.onResult(createImageCallbackResultForTesting());
        } else {
            ContextMenuNativeDelegateImplJni.get()
                    .retrieveImageForShare(
                            mNativePtr,
                            mRenderFrameHost,
                            imageRetrieveCallback,
                            MAX_SHARE_DIMEN_PX,
                            MAX_SHARE_DIMEN_PX,
                            imageFormat);
        }
    }

    @Override
    public void retrieveImageForContextMenu(
            int maxWidthPx, int maxHeightPx, Callback<Bitmap> callback) {
        if (mNativePtr == 0) return;

        ContextMenuNativeDelegateImplJni.get()
                .retrieveImageForContextMenu(
                        mNativePtr, mRenderFrameHost, callback, maxWidthPx, maxHeightPx);
    }

    @Override
    public void startDownload(GURL url, boolean isMedia) {
        if (mNativePtr == 0) return;

        ContextMenuNativeDelegateImplJni.get().startDownload(mNativePtr, url, isMedia);
    }

    @Override
    public void searchForImage() {
        if (mNativePtr == 0) return;

        ContextMenuNativeDelegateImplJni.get().searchForImage(mNativePtr, mRenderFrameHost);
    }

    @Override
    public void inspectElement(int x, int y) {
        if (mNativePtr == 0) return;

        ContextMenuNativeDelegateImplJni.get().inspectElement(mNativePtr, mRenderFrameHost, x, y);
    }

    @Override
    public RenderFrameHost getRenderFrameHost() {
        return mRenderFrameHost;
    }

    @Override
    public void setPictureInPicture(boolean enterPip) {
        if (mNativePtr == 0) return;

        ContextMenuNativeDelegateImplJni.get()
                .setPictureInPicture(mNativePtr, mRenderFrameHost, enterPip);
    }

    /** The class hold the |retrieveImageForShare| callback result. */
    @VisibleForTesting
    static class ImageCallbackResult {
        public byte[] imageData;
        public String extension;

        public ImageCallbackResult(byte[] imageData, String extension) {
            this.imageData = imageData;
            this.extension = extension;
        }
    }

    private static ImageCallbackResult createImageCallbackResultForTesting() {
        assumeNonNull(sHardcodedImageBytesForTesting);
        assumeNonNull(sHardcodedImageExtensionForTesting);
        return new ImageCallbackResult(
                sHardcodedImageBytesForTesting, sHardcodedImageExtensionForTesting);
    }

    @CalledByNative
    private static ImageCallbackResult createImageCallbackResult(
            byte[] imageData, @JniType("std::string") String extension) {
        return new ImageCallbackResult(imageData, extension);
    }

    @NativeMethods
    interface Natives {
        long init(
                @JniType("content::WebContents*") WebContents webContents,
                ContextMenuParams contextMenuParams);

        void retrieveImageForShare(
                long nativeContextMenuNativeDelegateImpl,
                @JniType("content::RenderFrameHost*") RenderFrameHost renderFrameHost,
                Callback<ImageCallbackResult> callback,
                int maxWidthPx,
                int maxHeightPx,
                @ContextMenuImageFormat int imageFormat);

        void retrieveImageForContextMenu(
                long nativeContextMenuNativeDelegateImpl,
                @JniType("content::RenderFrameHost*") RenderFrameHost renderFrameHost,
                Callback<Bitmap> callback,
                int maxWidthPx,
                int maxHeightPx);

        void startDownload(
                long nativeContextMenuNativeDelegateImpl,
                @JniType("GURL") GURL url,
                boolean isMedia);

        void searchForImage(
                long nativeContextMenuNativeDelegateImpl,
                @JniType("content::RenderFrameHost*") RenderFrameHost renderFrameHost);

        void inspectElement(
                long nativeContextMenuNativeDelegateImpl,
                @JniType("content::RenderFrameHost*") RenderFrameHost renderFrameHost,
                int x,
                int y);

        void setPictureInPicture(
                long nativeContextMenuNativeDelegateImpl,
                @JniType("content::RenderFrameHost*") RenderFrameHost renderFrameHost,
                boolean enterPip);
    }
}
