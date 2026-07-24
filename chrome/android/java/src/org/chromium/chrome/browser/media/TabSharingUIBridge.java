// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/** Glue for the tab sharing toolbar UI code and communication with the native backend. */
@NullMarked
public class TabSharingUIBridge {
    private static final String TAG = "TabSharing";

    private final WebContents mCapturer;
    private final WebContents mCapturee;
    private long mNativeTabSharingUIAndroid;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @param nativeTabSharingUIAndroid Pointer to the native object.
     * @param capturer The {@link WebContents} that is performing tab sharing.
     * @param capturee The {@link WebContents} that is being shared.
     */
    private TabSharingUIBridge(
            long nativeTabSharingUIAndroid, WebContents capturer, WebContents capturee) {
        mNativeTabSharingUIAndroid = nativeTabSharingUIAndroid;
        mCapturer = capturer;
        mCapturee = capturee;
    }

    /**
     * Creates a TabSharingUIBridge.
     *
     * @param nativePtr Pointer to the native object.
     * @param capturer The {@link WebContents} that is performing tab sharing.
     * @param capturee The {@link WebContents} that is being shared.
     */
    @CalledByNative
    private static TabSharingUIBridge create(
            long nativePtr,
            @JniType("content::WebContents*") WebContents capturer,
            @JniType("content::WebContents*") WebContents capturee) {
        TabSharingUIBridge bridge = new TabSharingUIBridge(nativePtr, capturer, capturee);
        Log.d(
                TAG,
                "UIBridge#create: for capturer %s and capturee %s, created instance %s",
                capturer,
                capturee,
                bridge);
        TabSharingUIManager.getInstance().addBridge(bridge);
        return bridge;
    }

    @CalledByNative
    private void destroy() {
        Log.d(TAG, "UIBridge#destroy: %s", this);
        TabSharingUIManager.getInstance().removeBridge(this);
        mNativeTabSharingUIAndroid = 0;
    }

    /** Stops the sharing session associated with this bridge. */
    public void stopSharing() {
        if (mNativeTabSharingUIAndroid == 0) return;
        TabSharingUIBridgeJni.get().stopSharing(mNativeTabSharingUIAndroid);
    }

    /**
     * Changes the source of the sharing session to the new source.
     *
     * @param newSource The new {@link WebContents} source to be shared.
     */
    public void changeSource(WebContents newSource) {
        if (mNativeTabSharingUIAndroid == 0) return;
        TabSharingUIBridgeJni.get().changeSource(mNativeTabSharingUIAndroid, newSource);
    }

    /** Returns the {@link WebContents} that is performing the tab sharing. */
    public WebContents getCapturer() {
        return mCapturer;
    }

    /** Returns the {@link WebContents} that is being shared. */
    public WebContents getCapturee() {
        return mCapturee;
    }

    @NativeMethods
    interface Natives {
        void stopSharing(long nativeTabSharingUIAndroid);

        void changeSource(
                long nativeTabSharingUIAndroid,
                @JniType("content::WebContents*") WebContents newSource);
    }
}
