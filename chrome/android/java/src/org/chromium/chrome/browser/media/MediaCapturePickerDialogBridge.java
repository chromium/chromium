// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.app.Activity;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/** Glue for the media capture picker dialog UI code and communication with the native backend. */
public class MediaCapturePickerDialogBridge implements MediaCapturePickerDialog.Delegate {
    private long mNativeMediaCapturePickerDialogBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @param nativeMediaCapturePickerDialogBridge Pointer to the native object.
     */
    private MediaCapturePickerDialogBridge(long nativeMediaCapturePickerDialogBridge) {
        mNativeMediaCapturePickerDialogBridge = nativeMediaCapturePickerDialogBridge;
    }

    /**
     * Creates a MediaCapturePickerDialogBridge, taking a pointer to the native instance.
     *
     * @param nativeMediaCapturePickerDialogBridge Pointer to the native object.
     */
    @CalledByNative
    public static MediaCapturePickerDialogBridge create(long nativeMediaCapturePickerDialogBridge) {
        return new MediaCapturePickerDialogBridge(nativeMediaCapturePickerDialogBridge);
    }

    /**
     * Shows the media capture picker dialog.
     *
     * @param windowAndroid Window to show the dialog on.
     * @param appName Name of the app that wants to share content.
     * @param requestAudio True if audio sharing is also requested.
     */
    @CalledByNative
    public void showDialog(
            WindowAndroid windowAndroid,
            @JniType("std::u16string") String appName,
            boolean requestAudio) {
        Activity activity = windowAndroid.getActivity().get();
        MediaCapturePickerDialog.showDialog(
                activity,
                ((ModalDialogManagerHolder) activity).getModalDialogManager(),
                appName,
                requestAudio,
                this);
    }

    @CalledByNative
    private void destroy() {
        mNativeMediaCapturePickerDialogBridge = 0;
    }

    @Override
    public void onPickTab(@NonNull WebContents webContents, boolean audioShare) {
        // We know `mNativeMediaCapturePickerDialogBridge` is non-zero because
        // `destroy` will only be called after the dialog is dismissed.
        assert mNativeMediaCapturePickerDialogBridge != 0;
        MediaCapturePickerDialogBridgeJni.get()
                .onPickTab(mNativeMediaCapturePickerDialogBridge, webContents, audioShare);
    }

    @Override
    public void onPickWindow() {
        // We know `mNativeMediaCapturePickerDialogBridge` is non-zero because
        // `destroy` will only be called after the dialog is dismissed.
        assert mNativeMediaCapturePickerDialogBridge != 0;
        MediaCapturePickerDialogBridgeJni.get().onPickWindow(mNativeMediaCapturePickerDialogBridge);
    }

    @Override
    public void onPickScreen() {
        // We know `mNativeMediaCapturePickerDialogBridge` is non-zero because
        // `destroy` will only be called after the dialog is dismissed.
        assert mNativeMediaCapturePickerDialogBridge != 0;
        MediaCapturePickerDialogBridgeJni.get().onPickScreen(mNativeMediaCapturePickerDialogBridge);
    }

    @Override
    public void onCancel() {
        // We know `mNativeMediaCapturePickerDialogBridge` is non-zero because
        // `destroy` will only be called after the dialog is dismissed.
        assert mNativeMediaCapturePickerDialogBridge != 0;
        MediaCapturePickerDialogBridgeJni.get().onCancel(mNativeMediaCapturePickerDialogBridge);
    }

    @NativeMethods
    interface Natives {
        void onPickTab(
                long nativeMediaCapturePickerDialogBridge,
                WebContents webContents,
                boolean audioShare);

        void onPickWindow(long nativeMediaCapturePickerDialogBridge);

        void onPickScreen(long nativeMediaCapturePickerDialogBridge);

        void onCancel(long nativeMediaCapturePickerDialogBridge);
    }
}
