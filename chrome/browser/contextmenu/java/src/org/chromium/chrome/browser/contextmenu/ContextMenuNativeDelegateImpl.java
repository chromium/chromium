// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.graphics.Bitmap;
import android.net.Uri;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.JniUniquePtr;
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
    @JniType("::ContextMenuNativeDelegateImpl")
    interface NativeContextMenuNativeDelegateImpl extends JniTypeToken {}

    private static final int MAX_SHARE_DIMEN_PX = 2048;

    private final RenderFrameHost mRenderFrameHost;

    private @Nullable JniUniquePtr<NativeContextMenuNativeDelegateImpl> mNativePtr;

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
        if (mNativePtr == null) return;
        mNativePtr.destroy();
        mNativePtr = null;
    }

    @Override
    public void retrieveImageForShare(int imageFormat, Callback<Uri> callback) {
        if (mNativePtr == null) return;

        Callback<ImageCallbackResult> imageRetrieveCallback =
                (ImageCallbackResult result) ->
                        ShareImageFileUtils.generateTemporaryUriFromData(
                                result.imageData, result.extension, callback);

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
        if (mNativePtr == null) return;

        ContextMenuNativeDelegateImplJni.get()
                .retrieveImageForContextMenu(
                        mNativePtr, mRenderFrameHost, callback, maxWidthPx, maxHeightPx);
    }

    @Override
    public void startDownload(GURL url, boolean isMedia) {
        if (mNativePtr == null) return;

        ContextMenuNativeDelegateImplJni.get()
                .startDownload(mNativePtr, mRenderFrameHost, url, isMedia);
    }

    @Override
    public void searchForImage() {
        if (mNativePtr == null) return;

        ContextMenuNativeDelegateImplJni.get().searchForImage(mNativePtr, mRenderFrameHost);
    }

    @Override
    public void inspectElement(int x, int y) {
        if (mNativePtr == null) return;

        ContextMenuNativeDelegateImplJni.get().inspectElement(mNativePtr, mRenderFrameHost, x, y);
    }

    @Override
    public RenderFrameHost getRenderFrameHost() {
        return mRenderFrameHost;
    }

    @Override
    public void setPictureInPicture(boolean enterPip) {
        if (mNativePtr == null) return;

        ContextMenuNativeDelegateImplJni.get()
                .setPictureInPicture(mNativePtr, mRenderFrameHost, enterPip);
    }

    @Override
    public void copyVideoFrame() {
        if (mNativePtr == null) return;

        ContextMenuNativeDelegateImplJni.get().copyVideoFrame(mNativePtr, mRenderFrameHost);
    }

    @Override
    public void downloadVideoFrame() {
        if (mNativePtr == null) return;

        ContextMenuNativeDelegateImplJni.get().downloadVideoFrame(mNativePtr, mRenderFrameHost);
    }

    @Override
    public void showInterestInElement() {
        if (mNativePtr == null) return;

        ContextMenuNativeDelegateImplJni.get().showInterestInElement(mNativePtr);
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
        JniUniquePtr<NativeContextMenuNativeDelegateImpl> init(
                @JniType("content::WebContents*") WebContents webContents,
                ContextMenuParams contextMenuParams);

        void retrieveImageForShare(
                JniPtr<NativeContextMenuNativeDelegateImpl> self,
                @JniType("content::RenderFrameHost*") RenderFrameHost renderFrameHost,
                Callback<ImageCallbackResult> callback,
                int maxWidthPx,
                int maxHeightPx,
                @ContextMenuImageFormat int imageFormat);

        void retrieveImageForContextMenu(
                JniPtr<NativeContextMenuNativeDelegateImpl> self,
                @JniType("content::RenderFrameHost*") RenderFrameHost renderFrameHost,
                Callback<Bitmap> callback,
                int maxWidthPx,
                int maxHeightPx);

        void startDownload(
                JniPtr<NativeContextMenuNativeDelegateImpl> self,
                @JniType("content::RenderFrameHost*") RenderFrameHost renderFrameHost,
                @JniType("GURL") GURL url,
                boolean isMedia);

        void searchForImage(
                JniPtr<NativeContextMenuNativeDelegateImpl> self,
                @JniType("content::RenderFrameHost*") RenderFrameHost renderFrameHost);

        void inspectElement(
                JniPtr<NativeContextMenuNativeDelegateImpl> self,
                @JniType("content::RenderFrameHost*") RenderFrameHost renderFrameHost,
                int x,
                int y);

        void setPictureInPicture(
                JniPtr<NativeContextMenuNativeDelegateImpl> self,
                @JniType("content::RenderFrameHost*") RenderFrameHost renderFrameHost,
                boolean enterPip);

        void copyVideoFrame(
                JniPtr<NativeContextMenuNativeDelegateImpl> self,
                @JniType("content::RenderFrameHost*") RenderFrameHost renderFrameHost);

        void downloadVideoFrame(
                JniPtr<NativeContextMenuNativeDelegateImpl> self,
                @JniType("content::RenderFrameHost*") RenderFrameHost renderFrameHost);

        void showInterestInElement(JniPtr<NativeContextMenuNativeDelegateImpl> self);
    }
}
