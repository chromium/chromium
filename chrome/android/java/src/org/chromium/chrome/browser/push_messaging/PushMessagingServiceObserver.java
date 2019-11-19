// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.push_messaging;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Observes events and changes in the PushMessagingService.
 *
 * Threading model: UI thread only.
 *
 * TODO(peter): Delete this class once delivery receipts are implemented and use those instead. This
 *              really only exists for test purposes.
 */
@JNINamespace("chrome::android")
public class PushMessagingServiceObserver {
    /**
     * The listener that needs to be notified of events and changes observed by the
     * PushMessagingServiceObserver. May be null.
     */
    @Nullable private static Listener sListener;

    /**
     * Interface for the listener that needs to be notified of events and changes observed by the
     * PushMessagingServiceObserver.
     */
    public interface Listener {
        /**
         * Called when a push message has been handled.
         */
        void onMessageHandled();
    }

    @VisibleForTesting
    public static void setListenerForTesting(@Nullable Listener listener) {
        ThreadUtils.assertOnUiThread();
        sListener = listener;
    }

    @CalledByNative
    private static void onMessageHandled() {
        ThreadUtils.assertOnUiThread();
        if (sListener != null) {
            sListener.onMessageHandled();
        }
    }

    private PushMessagingServiceObserver() {} // Private. Not meant to be instantiated.
}
