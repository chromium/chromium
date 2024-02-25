// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/**
 * The {@link NotificationTriggerScheduler} singleton is responsible for scheduling notification
 * triggers to wake Chrome up so that scheduled notifications can be displayed.
 * Thread model: This class is to be run on the UI thread only.
 */
public class NotificationTriggerScheduler {

    /** Clock to use so we can mock time in tests. */
    public static interface Clock {
        public long currentTimeMillis();
    }

    private Clock mClock;

    // Delay by 9 minutes when we need to reschedule so we're not waking up too often but still
    // within a reasonable time to show scheduled notifications. Note that if the reschedule was
    // caused by an upgrade, we'll show all scheduled notifications on the next browser start anyway
    // so this is just a fallback. 9 minutes were chosen as it's also the minimum time between two
    // scheduled alarms via AlarmManager.
    @VisibleForTesting
    protected static final long RESCHEDULE_DELAY_TIME = DateUtils.MINUTE_IN_MILLIS * 9;

    private static class LazyHolder {
        static final NotificationTriggerScheduler INSTANCE =
                new NotificationTriggerScheduler(System::currentTimeMillis);
    }

    private static NotificationTriggerScheduler sInstanceForTests;

    protected static void setInstanceForTests(NotificationTriggerScheduler instance) {
        sInstanceForTests = instance;
        ResettersForTesting.register(() -> sInstanceForTests = null);
    }

    @CalledByNative
    public static NotificationTriggerScheduler getInstance() {
        return sInstanceForTests == null ? LazyHolder.INSTANCE : sInstanceForTests;
    }

    @VisibleForTesting
    protected NotificationTriggerScheduler(Clock clock) {
        mClock = clock;
    }

    /** Calls into native code to trigger all pending notifications. */
    public void triggerNotifications() {
        NotificationTriggerSchedulerJni.get().triggerNotifications();
    }

    /**
     * Method to call when Android runs the scheduled task.
     * @param timestamp The timestamp for which this trigger got scheduled.
     * @return true if we should continue waking up native code, otherwise this event got handled
     *         already so no need to continue.
     */
    public boolean checkAndResetTrigger(long timestamp) {
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

    private void setNextTrigger(long timestamp) {
        ChromeSharedPreferences.getInstance()
                .writeLong(ChromePreferenceKeys.NOTIFICATIONS_NEXT_TRIGGER, timestamp);
    }

    @NativeMethods
    interface Natives {
        void triggerNotifications();
    }
}
