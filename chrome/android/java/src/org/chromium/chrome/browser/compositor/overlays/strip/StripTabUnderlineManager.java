// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.contextual_tasks.ContextualTasksBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashSet;
import java.util.Set;

/**
 * Manages the native C++ TabUnderlineController objects for Android tab UI surfaces.
 *
 * <p>TODO(crbug.com/509226293): Consider renaming to GlicTabIndicatorManager since this class
 * broadcasts Glic indicator state to HTS, Vertical Tabs, and GTS.
 */
@JNINamespace("android")
@NullMarked
public class StripTabUnderlineManager {
    /** An observer for Glic tab indicator state changes across Android UI surfaces. */
    public interface Observer {
        /**
         * Called when the Glic indicator state changes for a tab.
         *
         * @param tabId The ID of the tab whose indicator state changed.
         * @param isUnderlined Whether the Glic indicator should be active/visible.
         */
        void onIndicatorStateChanged(int tabId, boolean isUnderlined);

        /**
         * Called when the Glic indicator animation cycle should be reset for a tab.
         *
         * @param tabId The ID of the tab whose animation cycle should be reset.
         */
        void onResetAnimationCycle(int tabId);
    }

    private final WindowAndroid mWindowAndroid;
    private final Set<Tab> mTabsPendingContextualTasksBridge = new HashSet<>();
    private final Callback<ContextualTasksBridge> mContextualTasksBridgeObserver;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    private long mNativePtr;
    private boolean mContextualTasksBridgeInitialized;

    public StripTabUnderlineManager(WindowAndroid windowAndroid) {
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

    /**
     * Adds an observer to be notified of tab indicator state changes.
     *
     * @param observer The observer to add.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes a previously registered observer.
     *
     * @param observer The observer to remove.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    public void destroy() {
        mObservers.clear();
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
        for (Observer observer : mObservers) {
            observer.onIndicatorStateChanged(tabId, isUnderlined);
        }
    }

    @CalledByNative
    void resetAnimationCycle(int tabId) {
        for (Observer observer : mObservers) {
            observer.onResetAnimationCycle(tabId);
        }
    }

    @NativeMethods
    interface Natives {
        long init(StripTabUnderlineManager caller);

        void destroy(long nativeStripTabUnderlineManager);

        void registerTab(long nativeStripTabUnderlineManager, Tab tab);

        void unregisterTab(long nativeStripTabUnderlineManager, int tabId);
    }
}
