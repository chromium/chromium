// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

/**
 * A Java API for calling the native QR Code Generator service.
 *
 * An instance of this class must be created, used, and destroyed on the same thread.
 * Once created, this object will internally ensure that it stays alive until the QR code
 * generation request succeeds/fails.
 */
public class QRCodeGenerationRequest {
    // Pointer to the native request object.
    private long mNativeQRCodeGenerationRequest;
    // Stores the callback that will be invoked on request completion.
    private QRCodeServiceCallback mCallback;

    /**
     * Callback for use with this class.
     */
    public interface QRCodeServiceCallback {
        /**
         * Called when the QR Code is generated.
         *
         * @param bitmap The image, or null if none is available.
         */
        void onQRCodeAvailable(@Nullable Bitmap bitmap);
    }

    /**
     * Initializes the C++ side of this class and sends the request.
     *
     * @param data String of data for which to generate a code. If a URL, the caller should
     *         validate.
     * @param callback The callback to run when a new code is available, or on error.
     */
    public QRCodeGenerationRequest(String data, QRCodeServiceCallback callback) {
        mCallback = callback;
        mNativeQRCodeGenerationRequest =
                QRCodeGenerationRequestJni.get().init(QRCodeGenerationRequest.this, data);
    }

    @CalledByNative
    private void onQRCodeAvailable(@Nullable Bitmap bitmap) {
        if (mCallback != null) {
            mCallback.onQRCodeAvailable(bitmap);
            mCallback = null;
        }
        if (mNativeQRCodeGenerationRequest != 0) {
            QRCodeGenerationRequestJni.get().destroy(mNativeQRCodeGenerationRequest);
            mNativeQRCodeGenerationRequest = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long init(QRCodeGenerationRequest caller, String data);
        void destroy(long nativeQRCodeGenerationRequest);
    }
}
