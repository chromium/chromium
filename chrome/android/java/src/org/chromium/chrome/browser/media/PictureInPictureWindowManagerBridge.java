// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;

/**
 * Bridge between C++ PictureInPictureWindowManager and Java components. Exposes an
 * ObservableSupplier that tracks whether a Picture-in-Picture window is active.
 */
@NullMarked
@JNINamespace("picture_in_picture")
public class PictureInPictureWindowManagerBridge {
    private static final SettableNonNullObservableSupplier<Boolean> sIsPipShowingSupplier =
            ObservableSuppliers.createNonNull(false);

    private PictureInPictureWindowManagerBridge() {}

    /** Returns an observable supplier indicating whether any PiP window is currently showing. */
    public static NonNullObservableSupplier<Boolean> getIsPictureInPictureShowingSupplier() {
        return sIsPipShowingSupplier;
    }

    /** Initializes the native observer and synchronizes initial PiP state. */
    public static void initializeWithNative() {
        ThreadUtils.assertOnUiThread();
        sIsPipShowingSupplier.set(
                PictureInPictureWindowManagerBridgeJni.get().isInPictureInPicture());
    }

    @CalledByNative
    static void onPictureInPictureStateChanged(boolean isInPip) {
        ThreadUtils.assertOnUiThread();
        sIsPipShowingSupplier.set(isInPip);
    }

    static void resetForTesting() {
        sIsPipShowingSupplier.set(false);
    }

    @NativeMethods
    public interface Natives {
        boolean isInPictureInPicture();
    }
}
