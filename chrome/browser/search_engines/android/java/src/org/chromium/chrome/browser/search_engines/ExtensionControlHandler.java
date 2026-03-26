// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.function.Supplier;

/** A handler providing extension related controls via JNI. */
@NullMarked
public class ExtensionControlHandler {
    private static @Nullable Supplier<ExtensionControlHandler> sFactoryForTesting;

    private long mNativeExtensionControlHandler;

    public static ExtensionControlHandler createForProfile(Profile profile) {
        if (sFactoryForTesting != null) {
            return sFactoryForTesting.get();
        }
        return new ExtensionControlHandler(profile);
    }

    private ExtensionControlHandler(Profile profile) {
        ThreadUtils.assertOnUiThread();
        mNativeExtensionControlHandler = ExtensionControlHandlerJni.get().init(profile);
    }

    public static void setFactoryForTesting(Supplier<ExtensionControlHandler> factory) {
        sFactoryForTesting = factory;
        ResettersForTesting.register(() -> sFactoryForTesting = null);
    }

    public void destroy() {
        ThreadUtils.assertOnUiThread();
        if (mNativeExtensionControlHandler != 0) {
            ExtensionControlHandlerJni.get().destroy(mNativeExtensionControlHandler);
            mNativeExtensionControlHandler = 0;
        }
    }

    /**
     * Disables the extension with the given ID.
     *
     * @param extensionId The ID of the extension to disable.
     */
    public void disableExtension(String extensionId) {
        ThreadUtils.assertOnUiThread();
        assert mNativeExtensionControlHandler != 0;
        ExtensionControlHandlerJni.get()
                .disableExtension(mNativeExtensionControlHandler, extensionId);
    }

    @NativeMethods
    public interface Natives {
        long init(@JniType("Profile*") Profile profile);

        void disableExtension(
                long nativeExtensionControlHandler, @JniType("std::string") String extensionId);

        void destroy(long nativeExtensionControlHandler);
    }
}
