// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import androidx.annotation.Nullable;

import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

/** Bridge between the C++ and Java Chrome Memories HistoryClustersTabHelper. */
public class HistoryClustersTabHelper {
    /**
     * Notify Memories service that the current tab's URL has been copied.
     * @param webContents WebContents object associated with the current tab.
     */
    public static void onCurrentTabUrlCopied(@Nullable WebContents webContents) {
        // Note: WebContents requires native libraries to be loaded and initialized.
        if (webContents != null && !webContents.isDestroyed()) {
            HistoryClustersTabHelperJni.get().onCurrentTabUrlCopied(webContents);
        }
    }

    /**
     * Notify Memories service that the current tab's URL has been shared.
     * @param webContents WebContents object associated with the current tab.
     */
    public static void onCurrentTabUrlShared(@Nullable WebContents webContents) {
        // Note: WebContents requires native libraries to be loaded and initialized.
        if (webContents != null && !webContents.isDestroyed()) {
            HistoryClustersTabHelperJni.get().onCurrentTabUrlShared(webContents);
        }
    }

    @NativeMethods
    interface Natives {
        void onCurrentTabUrlCopied(WebContents contents);

        void onCurrentTabUrlShared(WebContents contents);
    }
}
