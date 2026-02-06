// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextual_tasks;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;

/**
 * Java bridge for Contextual Tasks. Owned by the activity's TabbedRootUiCoordinator. Owns its
 * native JNI counterpart.
 */
@JNINamespace("contextual_tasks")
@NullMarked
public class ContextualTasksBridge implements Destroyable {
    private long mNativeContextualTasksBridge;

    public ContextualTasksBridge() {
        mNativeContextualTasksBridge = ContextualTasksBridgeJni.get().init(this);
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
        long init(ContextualTasksBridge obj);

        void destroy(long nativeContextualTasksBridge);
    }
}
