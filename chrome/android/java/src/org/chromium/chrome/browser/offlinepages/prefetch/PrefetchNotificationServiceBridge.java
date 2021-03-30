// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.prefetch;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.download.DownloadOpenSource;
import org.chromium.chrome.browser.download.DownloadUtils;

/**
 * Glue @link{OfflineNotificationBackgroundTask} and PrefetchNotificationService in native side.
 */
@JNINamespace("offline_pages::prefetch")
public class PrefetchNotificationServiceBridge {
    private static final class LazyHolder {
        private static final PrefetchNotificationServiceBridge INSTANCE =
                new PrefetchNotificationServiceBridge();
    }

    private PrefetchNotificationServiceBridge() {}

    public static PrefetchNotificationServiceBridge getInstance() {
        return LazyHolder.INSTANCE;
    }

    /**
     * Schedules the prefetch notification using notification scheduler framework.
     * @param origin The origin of the prefetched page.
     */
    public void schedule(String origin) {
        Context context = ContextUtils.getApplicationContext();
        String title = String.format(
                context.getString(
                        org.chromium.chrome.R.string.offline_pages_prefetch_notification_title),
                context.getString(org.chromium.chrome.R.string.app_name));
        String text = String.format(
                context.getString(
                        org.chromium.chrome.R.string.offline_pages_prefetch_notification_text),
                origin);
        PrefetchNotificationServiceBridgeJni.get().schedule(title, text);
    }

    @NativeMethods
    interface Natives {
        /**
         * Schedules a prefetch notification through scheduling system.
         * @param title The title string of notification context.
         * @param message The body string of notification context.
         */
        void schedule(String title, String message);
    }

    /**
     * Launches Download Home and show offline pages panel.
     */
    @CalledByNative
    private static void launchDownloadHome() {
        DownloadUtils.showDownloadManager(null, null, null,
                DownloadOpenSource.OFFLINE_CONTENT_NOTIFICATION, true /*showPrefetchedContent*/);
    }
}
