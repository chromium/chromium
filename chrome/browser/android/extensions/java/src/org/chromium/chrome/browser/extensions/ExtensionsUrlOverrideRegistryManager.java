// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Listens to changes to the Native-level extensions URL registry and handles updates to Android
 * classes.
 */
@NullMarked
@JNINamespace("extensions")
public class ExtensionsUrlOverrideRegistryManager {
    private long mNativePtr;

    public ExtensionsUrlOverrideRegistryManager(Profile profile) {
        mNativePtr = ExtensionsUrlOverrideRegistryManagerJni.get().initialize(this, profile);
    }

    public void destroy() {
        if (mNativePtr != 0) {
            ExtensionsUrlOverrideRegistryManagerJni.get().destroy(mNativePtr);
            mNativePtr = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long initialize(
                ExtensionsUrlOverrideRegistryManager javaObject,
                @JniType("Profile*") Profile profile);

        void destroy(long nativeExtensionsUrlOverrideRegistryManager);
    }
}
