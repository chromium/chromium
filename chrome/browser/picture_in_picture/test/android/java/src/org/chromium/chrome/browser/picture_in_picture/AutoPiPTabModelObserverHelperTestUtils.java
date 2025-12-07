// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.picture_in_picture;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.content_public.browser.WebContents;

/** Utility class for testing AutoPictureInPictureTabModelObserverHelper C++ logic via JNI. */
@JNINamespace("picture_in_picture")
public class AutoPiPTabModelObserverHelperTestUtils {
    private AutoPiPTabModelObserverHelperTestUtils() {}

    public static void initialize(WebContents webContents, Callback<Boolean> callback) {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        AutoPiPTabModelObserverHelperTestUtilsJni.get()
                                .initialize(webContents, callback));
    }

    public static void startObserving() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> AutoPiPTabModelObserverHelperTestUtilsJni.get().startObserving());
    }

    public static void stopObserving() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> AutoPiPTabModelObserverHelperTestUtilsJni.get().stopObserving());
    }

    public static void destroy() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> AutoPiPTabModelObserverHelperTestUtilsJni.get().destroy());
    }

    @NativeMethods
    interface Natives {
        void initialize(
                @JniType("content::WebContents*") WebContents webContents,
                Callback<Boolean> onActivationChanged);

        void startObserving();

        void stopObserving();

        void destroy();
    }
}
