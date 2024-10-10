// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.gfx;

import android.graphics.Canvas;
import android.graphics.Picture;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.CleanupReference;

import java.io.OutputStream;

/**
 * A simple wrapper around a SkPicture, that allows final rendering to be performed using the
 * chromium skia library.
 */
@JNINamespace("android_webview")
public class AwPicture extends Picture {
    private long mNativeAwPicture;

    // There is no explicit destroy method on Picture base-class, so cleanup is always
    // handled via the CleanupReference.
    private static final class DestroyRunnable implements Runnable {
        private long mNativeAwPicture;

        private DestroyRunnable(long nativeAwPicture) {
            mNativeAwPicture = nativeAwPicture;
        }

        @Override
        public void run() {
            AwPictureJni.get().destroy(mNativeAwPicture);
        }
    }

    /**
     * @param nativeAwPicture is an instance of the AwPicture native class. Ownership is taken by
     *     this java instance.
     */
    public AwPicture(long nativeAwPicture) {
        mNativeAwPicture = nativeAwPicture;
        // Constructor has side-effects, so no need to store this in a field.
        new CleanupReference(this, new DestroyRunnable(nativeAwPicture));
    }

    @Override
    public Canvas beginRecording(int width, int height) {
        unsupportedOperation();
        return null;
    }

    @Override
    public void endRecording() {
        // Intentional no-op. The native picture ended recording prior to java c'tor call.
    }

    @Override
    public int getWidth() {
        return AwPictureJni.get().getWidth(mNativeAwPicture, AwPicture.this);
    }

    @Override
    public int getHeight() {
        return AwPictureJni.get().getHeight(mNativeAwPicture, AwPicture.this);
    }

    @Override
    public void draw(Canvas canvas) {
        AwPictureJni.get().draw(mNativeAwPicture, AwPicture.this, canvas);
    }

    @SuppressWarnings("deprecation")
    public void writeToStream(OutputStream stream) {
        unsupportedOperation();
    }

    private void unsupportedOperation() {
        throw new IllegalStateException("Unsupported in AwPicture");
    }

    @NativeMethods
    interface Natives {
        void destroy(long nativeAwPicture);

        int getWidth(long nativeAwPicture, AwPicture caller);

        int getHeight(long nativeAwPicture, AwPicture caller);

        void draw(long nativeAwPicture, AwPicture caller, Canvas canvas);
    }
}
