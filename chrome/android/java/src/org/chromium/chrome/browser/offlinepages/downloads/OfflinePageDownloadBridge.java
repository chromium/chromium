// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.downloads;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.offlinepages.OfflinePageOrigin;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Serves as an interface between Download Home UI and offline page related items that are to be
 * displayed in the downloads UI.
 */
@JNINamespace("offline_pages::android")
public class OfflinePageDownloadBridge {
    private static OfflinePageDownloadBridge sInstance;
    private static boolean sIsTesting;
    private long mNativeOfflinePageDownloadBridge;

    /**
     * @return An {@link OfflinePageDownloadBridge} instance singleton.  If one
     *         is not available this will create a new one.
     */
    public static OfflinePageDownloadBridge getInstance() {
        if (sInstance == null) {
            sInstance = new OfflinePageDownloadBridge();
        }
        return sInstance;
    }

    private OfflinePageDownloadBridge() {
        mNativeOfflinePageDownloadBridge = sIsTesting
                ? 0L
                : OfflinePageDownloadBridgeJni.get().init(OfflinePageDownloadBridge.this);
    }

    /** Destroys the native portion of the bridge. */
    public void destroy() {
        if (mNativeOfflinePageDownloadBridge != 0) {
            OfflinePageDownloadBridgeJni.get().destroy(
                    mNativeOfflinePageDownloadBridge, OfflinePageDownloadBridge.this);
            mNativeOfflinePageDownloadBridge = 0;
        }
    }

    /**
     * 'Opens' the offline page identified by the given URL and offlineId by navigating to the saved
     * local snapshot. No automatic redirection is happening based on the connection status. If the
     * item with specified GUID is not found or can't be opened, nothing happens.
     */
    @CalledByNative
    private static void openItem(final String url, final long offlineId, final int location,
            final boolean isIncognito, final boolean openInCct) {
    }

    /**
     * Starts download of the page currently open in the specified Tab.
     * If tab's contents are not yet loaded completely, we'll wait for it
     * to load enough for snapshot to be reasonable. If the Chrome is made
     * background and killed, the background request remains that will
     * eventually load the page in background and obtain its offline
     * snapshot.
     *
     * @param tab a tab contents of which will be saved locally.
     * @param origin the object encapsulating application origin of the request.
     */
    public static void startDownload(Tab tab, OfflinePageOrigin origin) {
        OfflinePageDownloadBridgeJni.get().startDownload(tab, origin.encodeAsJsonString());
    }

    /**
     * Shows a "Downloading ..." toast for the requested items already scheduled for download.
     */
    @CalledByNative
    public static void showDownloadingToast() {
    }

    @NativeMethods
    interface Natives {
        long init(OfflinePageDownloadBridge caller);
        void destroy(long nativeOfflinePageDownloadBridge, OfflinePageDownloadBridge caller);
        void startDownload(Tab tab, String origin);
    }
}
