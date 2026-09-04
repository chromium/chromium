// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/** Glue for the tab sharing toolbar UI code and communication with the native backend. */
@NullMarked
public class TabSharingUiBridge {
    private final WebContents mCapturer;
    private final WebContents mCapturee;
    private final WebContentsObserver mCapturerObserver;
    private final WebContentsObserver mCaptureeObserver;
    private final boolean mIsSourceSwitchingSupported;
    private final boolean mAppPreferredCurrentTab;

    private long mNativeTabSharingUIAndroid;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @param nativeTabSharingUIAndroid Pointer to the native object.
     * @param capturer The {@link WebContents} that is performing tab sharing.
     * @param capturee The {@link WebContents} that is being shared.
     */
    private TabSharingUiBridge(
            long nativeTabSharingUIAndroid,
            WebContents capturer,
            WebContents capturee,
            boolean isSourceSwitchingSupported,
            boolean appPreferredCurrentTab) {
        mNativeTabSharingUIAndroid = nativeTabSharingUIAndroid;
        mCapturer = capturer;
        mCapturee = capturee;
        mIsSourceSwitchingSupported = isSourceSwitchingSupported;
        mAppPreferredCurrentTab = appPreferredCurrentTab;
        mCapturerObserver =
                new WebContentsObserver(mCapturer) {
                    @Override
                    public void webContentsDestroyed() {
                        stopSharing();
                    }
                };
        mCaptureeObserver =
                new WebContentsObserver(mCapturee) {
                    @Override
                    public void webContentsDestroyed() {
                        stopSharing();
                    }
                };
    }

    /**
     * Creates a TabSharingUiBridge.
     *
     * @param nativePtr Pointer to the native object.
     * @param capturer The {@link WebContents} that is performing tab sharing.
     * @param capturee The {@link WebContents} that is being shared.
     */
    @CalledByNative
    static TabSharingUiBridge create(
            long nativePtr,
            @JniType("content::WebContents*") WebContents capturer,
            @JniType("content::WebContents*") WebContents capturee,
            boolean isSourceSwitchingSupported,
            boolean appPreferredCurrentTab) {
        TabSharingUiBridge bridge =
                new TabSharingUiBridge(
                        nativePtr,
                        capturer,
                        capturee,
                        isSourceSwitchingSupported,
                        appPreferredCurrentTab);
        TabSharingUiManager.getInstance().addBridge(bridge);
        return bridge;
    }

    @CalledByNative
    void destroy() {
        if (mNativeTabSharingUIAndroid == 0) return;
        mCapturerObserver.observe(null);
        mCaptureeObserver.observe(null);
        TabSharingUiManager.getInstance().removeBridge(this);
        mNativeTabSharingUIAndroid = 0;
    }

    /** Stops the sharing session associated with this bridge. */
    public void stopSharing() {
        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mCapturer, false);
        if (mNativeTabSharingUIAndroid == 0) return;
        TabSharingUiBridgeJni.get().stopSharing(mNativeTabSharingUIAndroid);
    }

    /**
     * Changes the source of the sharing session to the new source.
     *
     * @param newSource The new {@link WebContents} source to be shared.
     */
    public void changeSource(WebContents newSource) {
        if (mNativeTabSharingUIAndroid == 0) return;
        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mCapturer, true);
        TabSharingUiBridgeJni.get().changeSource(mNativeTabSharingUIAndroid, newSource);
    }

    /** Returns the {@link WebContents} that is performing the tab sharing. */
    public WebContents getCapturer() {
        return mCapturer;
    }

    /** Returns the {@link WebContents} that is being shared. */
    public WebContents getCapturee() {
        return mCapturee;
    }

    /** Returns true if source switching is supported for this capture session. */
    public boolean isSourceSwitchingSupported() {
        return mIsSourceSwitchingSupported;
    }

    /** Returns true if the capturing application preferred capturing the current tab. */
    public boolean appPreferredCurrentTab() {
        return mAppPreferredCurrentTab;
    }

    @NativeMethods
    interface Natives {
        void stopSharing(long nativeTabSharingUIAndroid);

        void changeSource(
                long nativeTabSharingUIAndroid,
                @JniType("content::WebContents*") WebContents newSource);
    }
}
