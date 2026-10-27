// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabwindow;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

/** An observer that forwards events from {@link TabWindowManager} to a native observer. */
@NullMarked
@JNINamespace("tab_window")
public class TabWindowManagerObserver implements TabWindowManager.Observer {
    @JniType("::tab_window::TabWindowManagerObserver")
    interface NativeTabWindowManagerObserver extends JniTypeToken {}

    private @Nullable JniRawPtr<NativeTabWindowManagerObserver> mNativeTabWindowManagerObserver;
    private final TabWindowManager mTabWindowManager;

    private TabWindowManagerObserver(JniRawPtr<NativeTabWindowManagerObserver> nativeObserver) {
        mNativeTabWindowManagerObserver = nativeObserver;
        mTabWindowManager = TabWindowManagerSingleton.getInstance();
        mTabWindowManager.addObserver(this);
    }

    @CalledByNative
    private static TabWindowManagerObserver create(
            JniRawPtr<NativeTabWindowManagerObserver> nativeObserver) {
        return new TabWindowManagerObserver(nativeObserver);
    }

    @CalledByNative
    private void destroy() {
        mTabWindowManager.removeObserver(this);
        if (mNativeTabWindowManagerObserver != null) {
            mNativeTabWindowManagerObserver.release();
            mNativeTabWindowManagerObserver = null;
        }
    }

    @Override
    public void onTabStateInitialized() {
        if (mNativeTabWindowManagerObserver != null) {
            TabWindowManagerObserverJni.get()
                    .onTabStateInitialized(mNativeTabWindowManagerObserver);
        }
    }

    @NativeMethods
    interface Natives {
        void onTabStateInitialized(JniPtr<NativeTabWindowManagerObserver> self);
    }
}
