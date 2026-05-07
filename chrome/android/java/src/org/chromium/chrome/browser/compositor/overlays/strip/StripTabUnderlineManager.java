// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashSet;
import java.util.Set;

/** Manages the native C++ TabUnderlineController objects for the strip. */
@JNINamespace("android")
@NullMarked
public class StripTabUnderlineManager {
    private long mNativePtr;
    private final StripLayoutHelper mStripLayoutHelper;
    private final WindowAndroid mWindowAndroid;
    private final Set<Tab> mTabsPendingContextualTasksBridge = new HashSet<>();
    private final Callback<ContextualTasksBridge> mContextualTasksBridgeObserver;

    private boolean mContextualTasksBridgeInitialized;

    public StripTabUnderlineManager(
            StripLayoutHelper stripLayoutHelper, WindowAndroid windowAndroid) {
        mStripLayoutHelper = stripLayoutHelper;
        mWindowAndroid = windowAndroid;
        mContextualTasksBridgeObserver = this::onContextualTasksBridgeReady;
        mNativePtr = StripTabUnderlineManagerJni.get().init(this);

        if (ChromeFeatureList.sContextualTasks.isEnabled()) {
            ContextualTasksBridge.getSupplier(mWindowAndroid)
                    .addSyncObserverAndCallIfNonNull(mContextualTasksBridgeObserver);
        } else {
            mContextualTasksBridgeInitialized = true;
        }
    }

    public void destroy() {
        if (ChromeFeatureList.sContextualTasks.isEnabled()) {
            ContextualTasksBridge.getSupplier(mWindowAndroid)
                    .removeObserver(mContextualTasksBridgeObserver);
        }
        if (mNativePtr != 0) {
            StripTabUnderlineManagerJni.get().destroy(mNativePtr);
            mNativePtr = 0;
        }
    }

    private void onContextualTasksBridgeReady(ContextualTasksBridge bridge) {
        mContextualTasksBridgeInitialized = true;
        for (Tab tab : mTabsPendingContextualTasksBridge) {
            registerTab(tab);
        }
        mTabsPendingContextualTasksBridge.clear();
        ContextualTasksBridge.getSupplier(mWindowAndroid)
                .removeObserver(mContextualTasksBridgeObserver);
    }

    /** Track a tab in the native manager. */
    public void registerTab(Tab tab) {
        if (mNativePtr == 0 || tab == null) return;
        if (!mContextualTasksBridgeInitialized) {
            mTabsPendingContextualTasksBridge.add(tab);
            return;
        }
        StripTabUnderlineManagerJni.get().registerTab(mNativePtr, tab);
    }

    /** Stop tracking a tab. */
    public void unregisterTab(int tabId) {
        if (mNativePtr == 0) return;
        mTabsPendingContextualTasksBridge.removeIf(tab -> tab.getId() == tabId);
        if (mContextualTasksBridgeInitialized) {
            StripTabUnderlineManagerJni.get().unregisterTab(mNativePtr, tabId);
        }
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
