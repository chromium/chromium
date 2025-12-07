// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabwindow;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

/** An observer that forwards events from {@link TabWindowManager} to a native observer. */
@NullMarked
@JNINamespace("tab_window")
public class TabWindowManagerObserver implements TabWindowManager.Observer {
    private long mNativeTabWindowManagerObserver;
    private final TabWindowManager mTabWindowManager;

    private TabWindowManagerObserver(long nativeObserver) {
        mNativeTabWindowManagerObserver = nativeObserver;
        mTabWindowManager = TabWindowManagerSingleton.getInstance();
        mTabWindowManager.addObserver(this);
    }

    @CalledByNative
    private static TabWindowManagerObserver create(long nativeObserver) {
        return new TabWindowManagerObserver(nativeObserver);
    }

    @CalledByNative
    private void destroy() {
        mTabWindowManager.removeObserver(this);
        mNativeTabWindowManagerObserver = 0;
    }

    @Override
    public void onTabStateInitialized() {
        if (mNativeTabWindowManagerObserver != 0) {
            TabWindowManagerObserverJni.get()
                    .onTabStateInitialized(mNativeTabWindowManagerObserver);
        }
    }

    @NativeMethods
    interface Natives {
        void onTabStateInitialized(long nativeTabWindowManagerObserver);
    }
}
