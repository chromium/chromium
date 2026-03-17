// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;

/**
 * Java bridge for Contextual Tasks. Owned by the activity's TabbedRootUiCoordinator. Owns its
 * native JNI counterpart.
 */
@JNINamespace("contextual_tasks")
@NullMarked
public class ContextualTasksBridge implements Destroyable {
    private long mNativeContextualTasksBridge;

    public ContextualTasksBridge(@Nullable Profile profile, @Nullable ChromeAndroidTask task) {
        assert profile != null : "Profile should not be null";
        assert task != null : "ChromeAndroidTask should not be null";
        long browserWindowPtr = task.getOrCreateNativeBrowserWindowPtr(profile);
        mNativeContextualTasksBridge =
                ContextualTasksBridgeJni.get().init(this, browserWindowPtr, profile);
    }

    @Override
    public void destroy() {
        if (mNativeContextualTasksBridge != 0) {
            ContextualTasksBridgeJni.get().destroy(mNativeContextualTasksBridge);
            mNativeContextualTasksBridge = 0;
        }
    }

    @NativeMethods
    interface Natives {
        long init(
                ContextualTasksBridge obj,
                long browserWindowPtr,
                @JniType("Profile*") Profile profile);

        void destroy(long nativeContextualTasksBridge);
    }
}
