// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import android.app.Notification;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Interface for controlling the ActorForegroundService lifecycle and interaction from the browser
 * layer.
 */
@NullMarked
public interface ActorForegroundServiceController {
    /**
     * Starts the service and binds to it.
     *
     * @param onConnected Runnable to be called when the service is connected.
     */
    void startAndBindService(Runnable onConnected);

    /** Unbinds from the service. */
    void unbindService();

    /** Returns whether the service is currently bound and connected. */
    boolean isConnected();

    /**
     * Starts or updates the foreground service with a notification.
     *
     * @param newNotificationId The ID for the new notification.
     * @param newNotification The notification to display.
     * @param oldNotificationId The ID of the previous notification, or -1 if none.
     * @param killOldNotification Whether to remove the old notification.
     */
    void startOrUpdateForegroundService(
            int newNotificationId,
            Notification newNotification,
            int oldNotificationId,
            boolean killOldNotification);

    /** Proxies the stopActorForegroundService call to the bound service. */
    void stopActorForegroundService(int flags);

    /** Returns the singleton instance. */
    static ActorForegroundServiceController get() {
        if (Holder.sInstanceForTesting != null) return Holder.sInstanceForTesting;
        ActorForegroundServiceController ret =
                ServiceLoaderUtil.maybeCreate(ActorForegroundServiceController.class);
        if (ret != null) return ret;
        return NoOpActorForegroundServiceController.getInstance();
    }

    static void setInstanceForTesting(ActorForegroundServiceController controller) {
        Holder.sInstanceForTesting = controller;
        ResettersForTesting.register(() -> Holder.sInstanceForTesting = null);
    }

    class Holder {
        static @Nullable ActorForegroundServiceController sInstanceForTesting;
    }
}
