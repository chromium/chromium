// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/**
 * The {@link NotificationTriggerScheduler} singleton is responsible for scheduling notification
 * triggers to wake Chrome up so that scheduled notifications can be displayed. Thread model: This
 * class is to be run on the UI thread only.
 */
public class NotificationTriggerScheduler {
    private static final NotificationTriggerScheduler INSTANCE = new NotificationTriggerScheduler();

    private static NotificationTriggerScheduler sInstanceForTests;

    protected static void setInstanceForTests(NotificationTriggerScheduler instance) {
        sInstanceForTests = instance;
        ResettersForTesting.register(() -> sInstanceForTests = null);
    }

    @CalledByNative
    public static NotificationTriggerScheduler getInstance() {
        return sInstanceForTests == null ? INSTANCE : sInstanceForTests;
    }

    @VisibleForTesting
    protected NotificationTriggerScheduler() {}

    /** Calls into native code to trigger all pending notifications. */
    public void triggerNotifications() {
        NotificationTriggerSchedulerJni.get().triggerNotifications();
    }

    /**
     * Method to call when Android runs the scheduled task.
     *
     * @param timestamp The timestamp for which this trigger got scheduled.
     * @return true if we should continue waking up native code, otherwise this event got handled
     *     already so no need to continue.
     */
    public boolean checkAndResetTrigger(long timestamp) {
        // TODO(crbug.com/1379251): This class can probably be deleted.
        if (getNextTrigger() != timestamp) return false;
        removeNextTrigger();
        return true;
    }

    private long getNextTrigger() {
        return ChromeSharedPreferences.getInstance()
                .readLong(ChromePreferenceKeys.NOTIFICATIONS_NEXT_TRIGGER, Long.MAX_VALUE);
    }

    private void removeNextTrigger() {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.NOTIFICATIONS_NEXT_TRIGGER);
    }

    @NativeMethods
    interface Natives {
        void triggerNotifications();
    }
}
