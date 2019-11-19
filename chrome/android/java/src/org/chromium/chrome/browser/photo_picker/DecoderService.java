// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.app.Service;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.os.SystemClock;

import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.FileDescriptor;
import java.io.IOException;

/**
 * A service to accept requests to take image file contents and decode them.
 */
@MainDex
public class DecoderService extends Service {
    // The keys for the bundle when passing data to and from this service.
    static final String KEY_FILE_DESCRIPTOR = "file_descriptor";
    static final String KEY_FILE_PATH = "file_path";
    static final String KEY_IMAGE_BITMAP = "image_bitmap";
    static final String KEY_SIZE = "size";
    static final String KEY_SUCCESS = "success";
    static final String KEY_DECODE_TIME = "decode_time";

    // A tag for logging error messages.
    private static final String TAG = "ImageDecoder";

    // Whether the native library and the sandbox have been initialized.
    private boolean mNativeLibraryAndSandboxInitialized;

    @Override
    public void onCreate() {
        // DecoderService does not require flags, but LibraryLoader.ensureInitialized() checks for
        // --enable-low-end-device-mode. Rather than forwarding the flags from the browser process,
        // just assume no flags.
        if (!CommandLine.isInitialized()) {
            CommandLine.init(null);
        }

        // The decoder service relies on PathUtils.
        PostTask.runSynchronously(UiThreadTaskTraits.DEFAULT, () -> {
            PathUtils.setPrivateDataDirectorySuffix(
                    ChromeApplication.PRIVATE_DATA_DIRECTORY_SUFFIX);
        });

        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_CHILD);
        DecoderServiceJni.get().initializePhotoPickerSandbox();

        mNativeLibraryAndSandboxInitialized = true;

        super.onCreate();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    private final IDecoderService.Stub mBinder = new IDecoderService.Stub() {
        @Override
        public void decodeImage(Bundle payload, IDecoderServiceCallback callback) {
            Bundle bundle = null;
            String filePath = "";
            int size = 0;
            try {
                filePath = payload.getString(KEY_FILE_PATH);
                ParcelFileDescriptor pfd = payload.getParcelable(KEY_FILE_DESCRIPTOR);
                size = payload.getInt(KEY_SIZE);

                // Setup a minimum viable response to parent process. Will be fleshed out
                // further below.
                bundle = new Bundle();
                bundle.putString(KEY_FILE_PATH, filePath);
                bundle.putBoolean(KEY_SUCCESS, false);

                if (!mNativeLibraryAndSandboxInitialized) {
                    Log.e(TAG, "Decode failed %s (size: %d): no sandbox", filePath, size);
                    sendReply(callback, bundle); // Sends SUCCESS == false;
                    return;
                }

                FileDescriptor fd = pfd.getFileDescriptor();

                long begin = SystemClock.elapsedRealtime();
                Bitmap bitmap = BitmapUtils.decodeBitmapFromFileDescriptor(fd, size);
                long decodeTime = SystemClock.elapsedRealtime() - begin;

                try {
                    pfd.close();
                } catch (IOException e) {
                    Log.e(TAG, "Closing failed " + filePath + " (size: " + size + ") " + e);
                }

                if (bitmap == null) {
                    Log.e(TAG, "Decode failed " + filePath + " (size: " + size + ")");
                    sendReply(callback, bundle); // Sends SUCCESS == false;
                    return;
                }

                // The most widely supported, easiest, and reasonably efficient method is to
                // decode to an immutable bitmap and just return the bitmap over binder. It
                // will internally memcpy itself to ashmem and then just send over the file
                // descriptor. In the receiving process it will just leave the bitmap on
                // ashmem since it's immutable and carry on.
                bundle.putParcelable(KEY_IMAGE_BITMAP, bitmap);
                bundle.putBoolean(KEY_SUCCESS, true);
                bundle.putLong(KEY_DECODE_TIME, decodeTime);
                sendReply(callback, bundle);
                bitmap.recycle();
            } catch (Exception e) {
                // This service has no UI and maintains no state so if it crashes on
                // decoding a photo, it is better UX to eat the exception instead of showing
                // a crash dialog and discarding other requests that have already been sent.
                Log.e(TAG,
                        "Unexpected error during decoding " + filePath + " (size: " + size + ") "
                                + e);

                if (bundle != null) sendReply(callback, bundle);
            }
        }

        private void sendReply(IDecoderServiceCallback callback, Bundle bundle) {
            try {
                callback.onDecodeImageDone(bundle);
            } catch (RemoteException remoteException) {
                Log.e(TAG, "Remote error while replying: " + remoteException);
            }
        }
    };

    @NativeMethods
    interface Natives {
        // Initializes the seccomp-bpf sandbox when it's supported by the device. Records the
        // sandbox status to the Android.SeccompStatus.PhotoPickerSandbox histogram.
        void initializePhotoPickerSandbox();
    }
}
