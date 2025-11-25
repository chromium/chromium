// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.blink.mojom.PreferredDisplaySurface;
import org.chromium.blink.mojom.WindowAudioPreference;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/** Glue for the media capture picker dialog UI code and communication with the native backend. */
@NullMarked
public class MediaCapturePickerManagerBridge implements MediaCapturePickerManager.Delegate {
    private long mNativeMediaCapturePickerManagerBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @param nativeMediaCapturePickerManagerBridge Pointer to the native object.
     */
    private MediaCapturePickerManagerBridge(long nativeMediaCapturePickerManagerBridge) {
        mNativeMediaCapturePickerManagerBridge = nativeMediaCapturePickerManagerBridge;
    }

    /**
     * Creates a MediaCapturePickerManagerBridge, taking a pointer to the native instance.
     *
     * @param nativeMediaCapturePickerManagerBridge Pointer to the native object.
     */
    @CalledByNative
    public static MediaCapturePickerManagerBridge create(
            long nativeMediaCapturePickerManagerBridge) {
        return new MediaCapturePickerManagerBridge(nativeMediaCapturePickerManagerBridge);
    }

    /**
     * Shows the media capture picker dialog.
     *
     * @param webContents The {@link WebContents} to show the dialog on behalf of.
     * @param appName Name of the app that wants to share content.
     * @param requestAudio True if audio sharing is also requested.
     */
    @CalledByNative
    public void showDialog(
            WebContents webContents,
            @JniType("std::u16string") String appName,
            @JniType("std::u16string") String targetName,
            boolean requestAudio,
            boolean excludeSystemAudio,
            @WindowAudioPreference.EnumType int windowAudioPreference,
            @PreferredDisplaySurface.EnumType int preferredDisplaySurface,
            boolean captureThisTab,
            boolean excludeSelfBrowserSurface,
            boolean excludeMonitorTypeSurfaces) {
        MediaCapturePickerManager.Params params =
                new MediaCapturePickerManager.Params(
                        webContents,
                        appName,
                        targetName,
                        requestAudio,
                        excludeSystemAudio,
                        windowAudioPreference,
                        preferredDisplaySurface,
                        captureThisTab,
                        excludeSelfBrowserSurface,
                        excludeMonitorTypeSurfaces);
        MediaCapturePickerManager.showDialog(params, /* delegate= */ this);
    }

    @CalledByNative
    private void destroy() {
        mNativeMediaCapturePickerManagerBridge = 0;
    }

    @Override
    public void onPickTab(WebContents webContents, boolean audioShare) {
        // We know `mNativeMediaCapturePickerManagerBridge` is non-zero because
        // `destroy` will only be called after the dialog is dismissed.
        assert mNativeMediaCapturePickerManagerBridge != 0;
        MediaCapturePickerManagerBridgeJni.get()
                .onPickTab(mNativeMediaCapturePickerManagerBridge, webContents, audioShare);
    }

    @Override
    public void onPickWindow() {
        // We know `mNativeMediaCapturePickerManagerBridge` is non-zero because
        // `destroy` will only be called after the dialog is dismissed.
        assert mNativeMediaCapturePickerManagerBridge != 0;
        MediaCapturePickerManagerBridgeJni.get()
                .onPickWindow(mNativeMediaCapturePickerManagerBridge);
    }

    @Override
    public void onPickScreen() {
        // We know `mNativeMediaCapturePickerManagerBridge` is non-zero because
        // `destroy` will only be called after the dialog is dismissed.
        assert mNativeMediaCapturePickerManagerBridge != 0;
        MediaCapturePickerManagerBridgeJni.get()
                .onPickScreen(mNativeMediaCapturePickerManagerBridge);
    }

    @Override
    public void onCancel() {
        // We know `mNativeMediaCapturePickerManagerBridge` is non-zero because
        // `destroy` will only be called after the dialog is dismissed.
        assert mNativeMediaCapturePickerManagerBridge != 0;
        MediaCapturePickerManagerBridgeJni.get().onCancel(mNativeMediaCapturePickerManagerBridge);
    }

    @Override
    public boolean shouldFilterWebContents(WebContents webContents) {
        // We know `mNativeMediaCapturePickerManagerBridge` is non-zero because
        // `destroy` will only be called after the dialog is dismissed.
        assert mNativeMediaCapturePickerManagerBridge != 0;
        return MediaCapturePickerManagerBridgeJni.get()
                .shouldFilterWebContents(mNativeMediaCapturePickerManagerBridge, webContents);
    }

    @NativeMethods
    interface Natives {
        void onPickTab(
                long nativeMediaCapturePickerManagerBridge,
                @JniType("content::WebContents*") WebContents webContents,
                boolean audioShare);

        void onPickWindow(long nativeMediaCapturePickerManagerBridge);

        void onPickScreen(long nativeMediaCapturePickerManagerBridge);

        void onCancel(long nativeMediaCapturePickerManagerBridge);

        boolean shouldFilterWebContents(
                long nativeMediaCapturePickerManagerBridge,
                @JniType("content::WebContents*") WebContents webContents);
    }
}
