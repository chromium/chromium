// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.permissions;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

/**
 * Process-wide notifier for changes to app-level notification permission (via OS settings or the
 * runtime permission prompt) and individual notification channel blocked states. Must be accessed
 * on the UI thread.
 */
@NullMarked
public class NotificationPermissionChangeNotifier {
    /**
     * Observer for notification permission and channel state changes. Callbacks report the current
     * state, may fire repeatedly without a state transition (for example, repeated prompt denials
     * or an Android 13+ grant followed by {@code ACTION_APP_BLOCK_STATE_CHANGED}), and must be
     * idempotent.
     */
    public interface Observer {
        /**
         * Called when app-level notifications are enabled or disabled.
         *
         * @param enabled Whether app-level notifications are now enabled.
         */
        default void onAppNotificationPermissionChanged(boolean enabled) {}

        /**
         * Called when a notification channel's blocked state changes in OS settings.
         *
         * @param channelId The ID of the channel whose state changed.
         * @param enabled Whether the channel is now enabled (not blocked).
         */
        default void onNotificationChannelStateChanged(String channelId, boolean enabled) {}
    }

    private static final ObserverList<Observer> sObservers = new ObserverList<>();

    private NotificationPermissionChangeNotifier() {}

    /** Adds an observer to be notified of notification permission and channel state changes. */
    public static void addObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        sObservers.addObserver(observer);
    }

    /** Removes a previously added observer. */
    public static void removeObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        sObservers.removeObserver(observer);
    }

    static void notifyAppNotificationPermissionChanged(boolean enabled) {
        ThreadUtils.assertOnUiThread();
        for (Observer observer : sObservers) {
            observer.onAppNotificationPermissionChanged(enabled);
        }
    }

    static void notifyNotificationChannelStateChanged(String channelId, boolean enabled) {
        ThreadUtils.assertOnUiThread();
        for (Observer observer : sObservers) {
            observer.onNotificationChannelStateChanged(channelId, enabled);
        }
    }
}
