// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tab.Tab;

/** Manages the native C++ TabUnderlineController objects for the strip. */
@JNINamespace("android")
@NullMarked
public class StripTabUnderlineManager {
    private long mNativePtr;
    private final StripLayoutHelper mStripLayoutHelper;

    public StripTabUnderlineManager(StripLayoutHelper stripLayoutHelper) {
        mStripLayoutHelper = stripLayoutHelper;
        mNativePtr = StripTabUnderlineManagerJni.get().init(this);
    }

    public void destroy() {
        if (mNativePtr != 0) {
            StripTabUnderlineManagerJni.get().destroy(mNativePtr);
            mNativePtr = 0;
        }
    }

    /** Track a tab in the native manager. */
    public void registerTab(Tab tab) {
        if (mNativePtr == 0 || tab == null) return;
        StripTabUnderlineManagerJni.get().registerTab(mNativePtr, tab);
    }

    /** Stop tracking a tab. */
    public void unregisterTab(int tabId) {
        if (mNativePtr == 0) return;
        StripTabUnderlineManagerJni.get().unregisterTab(mNativePtr, tabId);
    }

    @CalledByNative
    void setUnderlineState(int tabId, boolean isUnderlined) {
        mStripLayoutHelper.setTabUnderline(tabId, isUnderlined);
    }

    @NativeMethods
    interface Natives {
        long init(StripTabUnderlineManager caller);

        void destroy(long nativeStripTabUnderlineManager);

        void registerTab(long nativeStripTabUnderlineManager, Tab tab);

        void unregisterTab(long nativeStripTabUnderlineManager, int tabId);
    }
}
