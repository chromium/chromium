// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

import java.util.HashSet;
import java.util.Set;

/**
 * Singleton manager for offscreen rendering. Responsible for managing the offscreen window and
 * compositor.
 */
@JNINamespace("actor")
@NullMarked
public class OffscreenRenderingManager {
    private static @Nullable OffscreenRenderingManager sInstance;

    private long mNativePtr;
    private @Nullable WindowAndroid mOffscreenWindow;
    private final Set<Tab> mOffscreenTabs = new HashSet<>();
    private final ManagerTabObserver mTabObserver = new ManagerTabObserver(this);

    private static class ManagerTabObserver extends EmptyTabObserver {
        private final OffscreenRenderingManager mManager;

        ManagerTabObserver(OffscreenRenderingManager manager) {
            mManager = manager;
        }

        @Override
        public void onDestroyed(Tab tab) {
            mManager.stopOffscreenRendering(tab);
        }
    }

    /** Returns the singleton instance of OffscreenRenderingManager. */
    public static OffscreenRenderingManager getInstance() {
        if (sInstance == null) {
            sInstance = new OffscreenRenderingManager();
        }
        return sInstance;
    }

    private OffscreenRenderingManager() {}

    private void ensureNativeInitialized() {
        if (mNativePtr != 0) return;

        mOffscreenWindow =
                new WindowAndroid(
                        ContextUtils.getApplicationContext(),
                        /* occlusionTrackingAllowed= */ false);

        // Initialize native with an arbitrary size. It doesn't need to match the tab size
        // as long as we don't do readback from the root layer.
        mNativePtr = OffscreenRenderingManagerJni.get().init(mOffscreenWindow, 1, 1);
    }

    /**
     * Starts offscreen rendering for the given Tab.
     *
     * @param tab The Tab to render offscreen.
     * @param width The width of the offscreen surface.
     * @param height The height of the offscreen surface.
     */
    public void startOffscreenRendering(Tab tab, int width, int height) {
        ensureNativeInitialized();
        if (mNativePtr == 0 || mOffscreenTabs.contains(tab)) return;

        WebContents webContents = tab.getWebContents();
        if (webContents == null || webContents.isDestroyed()) return;
        tab.addObserver(mTabObserver);

        webContents.setSize(width, height);
        OffscreenRenderingManagerJni.get()
                .startOffscreenRendering(mNativePtr, webContents, width, height);
        tab.startOffscreenRendering();
        webContents.setTopLevelNativeWindow(mOffscreenWindow);

        mOffscreenTabs.add(tab);
    }

    /**
     * Stops offscreen rendering for the given Tab.
     *
     * @param tab The Tab to stop rendering offscreen.
     */
    public void stopOffscreenRendering(Tab tab) {
        if (!mOffscreenTabs.contains(tab)) return;

        WebContents webContents = tab.getWebContents();
        if (webContents == null || webContents.isDestroyed()) {
            throw new IllegalStateException(
                    "WebContents should not be null or destroyed when stopping offscreen"
                        + " rendering.");
        }

        OffscreenRenderingManagerJni.get().stopOffscreenRendering(mNativePtr, webContents);

        tab.removeObserver(mTabObserver);
        // Handles window restoration.
        tab.stopOffscreenRendering();
        mOffscreenTabs.remove(tab);

        // If this was the last tab, clean up the native manager and offscreen window.
        if (mOffscreenTabs.isEmpty()) {
            destroy();
        }
    }

    /** Destroy the native counterpart and resources. */
    public void destroy() {
        // Stop all active offscreen rendering sessions.
        HashSet<Tab> remainingOffscreenTabs = new HashSet<>(mOffscreenTabs);
        for (Tab tab : remainingOffscreenTabs) {
            stopOffscreenRendering(tab);
        }

        if (mNativePtr != 0) {
            OffscreenRenderingManagerJni.get().destroy(mNativePtr);
            mNativePtr = 0;
        }
        if (mOffscreenWindow != null) {
            mOffscreenWindow.destroy();
            mOffscreenWindow = null;
        }
        sInstance = null;
    }

    public static void setInstanceForTesting(OffscreenRenderingManager instance) {
        var oldInstance = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldInstance);
    }

    @NativeMethods
    interface Natives {
        long init(@JniType("ui::WindowAndroid*") WindowAndroid window, int width, int height);

        void destroy(long nativeOffscreenRenderingManagerAndroid);

        void startOffscreenRendering(
                long nativeOffscreenRenderingManagerAndroid,
                @JniType("content::WebContents*") WebContents webContents,
                int width,
                int height);

        void stopOffscreenRendering(
                long nativeOffscreenRenderingManagerAndroid,
                @JniType("content::WebContents*") WebContents webContents);
    }
}
