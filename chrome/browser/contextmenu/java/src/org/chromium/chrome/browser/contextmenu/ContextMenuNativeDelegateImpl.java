// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.graphics.Bitmap;
import android.net.Uri;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.embedder_support.contextmenu.ContextMenuImageFormat;
import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;

class ContextMenuNativeDelegateImpl implements ContextMenuNativeDelegate {
    private static final int MAX_SHARE_DIMEN_PX = 2048;

    private final RenderFrameHost mRenderFrameHost;

    private long mNativePtr;

    /** See function for details. */
    private static byte[] sHardcodedImageBytesForTesting;

    private static String sHardcodedImageExtensionForTesting;

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
                            ContextMenuNativeDelegateImpl.this,
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
                        mNativePtr,
                        ContextMenuNativeDelegateImpl.this,
                        mRenderFrameHost,
                        callback,
                        maxWidthPx,
                        maxHeightPx);
    }

    @Override
    public void startDownload(boolean isLink) {
        if (mNativePtr == 0) return;

        ContextMenuNativeDelegateImplJni.get()
                .startDownload(mNativePtr, ContextMenuNativeDelegateImpl.this, isLink);
    }

    @Override
    public void searchForImage() {
        if (mNativePtr == 0) return;

        ContextMenuNativeDelegateImplJni.get()
                .searchForImage(mNativePtr, ContextMenuNativeDelegateImpl.this, mRenderFrameHost);
    }

    @Override
    public RenderFrameHost getRenderFrameHost() {
        return mRenderFrameHost;
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
        return new ImageCallbackResult(
                sHardcodedImageBytesForTesting, sHardcodedImageExtensionForTesting);
    }

    @CalledByNative
    private static ImageCallbackResult createImageCallbackResult(
            byte[] imageData, String extension) {
        return new ImageCallbackResult(imageData, extension);
    }

    @NativeMethods
    interface Natives {
        long init(WebContents webContents, ContextMenuParams contextMenuParams);

        void retrieveImageForShare(
                long nativeContextMenuNativeDelegateImpl,
                ContextMenuNativeDelegateImpl caller,
                RenderFrameHost renderFrameHost,
                Callback<ImageCallbackResult> callback,
                int maxWidthPx,
                int maxHeightPx,
                @ContextMenuImageFormat int imageFormat);

        void retrieveImageForContextMenu(
                long nativeContextMenuNativeDelegateImpl,
                ContextMenuNativeDelegateImpl caller,
                RenderFrameHost renderFrameHost,
                Callback<Bitmap> callback,
                int maxWidthPx,
                int maxHeightPx);

        void startDownload(
                long nativeContextMenuNativeDelegateImpl,
                ContextMenuNativeDelegateImpl caller,
                boolean isLink);

        void searchForImage(
                long nativeContextMenuNativeDelegateImpl,
                ContextMenuNativeDelegateImpl caller,
                RenderFrameHost renderFrameHost);
    }
}
