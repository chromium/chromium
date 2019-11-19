// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications;

import android.text.format.DateUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * The {@link NotificationTriggerScheduler} singleton is responsible for scheduling notification
 * triggers to wake Chrome up so that scheduled notifications can be displayed.
 * Thread model: This class is to be run on the UI thread only.
 */
public class NotificationTriggerScheduler {
    private static final String KEY_NEXT_TRIGGER = "notification_trigger_scheduler.next_trigger";

    /** Clock to use so we can mock time in tests. */
    public static interface Clock { public long currentTimeMillis(); }

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

    @VisibleForTesting
    protected static void setInstanceForTests(NotificationTriggerScheduler instance) {
        sInstanceForTests = instance;
    }

    @CalledByNative
    public static NotificationTriggerScheduler getInstance() {
        return sInstanceForTests == null ? LazyHolder.INSTANCE : sInstanceForTests;
    }

    @VisibleForTesting
    protected NotificationTriggerScheduler(Clock clock) {
        mClock = clock;
    }

    /**
     * Schedules a one-off background task to wake the browser up and call into native code to
     * display pending notifications. If there is already a trigger scheduled earlier, this is a
     * nop. Otherwise the existing trigger is overwritten.
     * @param timestamp The timestamp of the next trigger.
     */
    @CalledByNative
    @VisibleForTesting
    protected void schedule(long timestamp) {
        // Check if there is already a trigger scheduled earlier. Also check for the case where
        // Android did not execute our task and reschedule.
        long now = mClock.currentTimeMillis();
        long nextTrigger = getNextTrigger();

        if (timestamp < nextTrigger) {
            // New timestamp is earlier than existing one -> schedule new task.
            setNextTrigger(timestamp);
            nextTrigger = timestamp;
        } else if (nextTrigger >= now) {
            // Existing timestamp is earlier than new one and still in future -> do nothing.
            return;
        } // else: Existing timestamp is earlier than new one and overdue -> schedule task again.

        long delay = Math.max(nextTrigger - now, 0);
        NotificationTriggerBackgroundTask.schedule(nextTrigger, delay);
    }

    /**
     * Method for rescheduling a background task to wake up Chrome for processing notification
     * trigger events in the event of an OS upgrade or Google Play Services upgrade.
     */
    public void reschedule() {
        schedule(mClock.currentTimeMillis() + RESCHEDULE_DELAY_TIME);
    }

    /**
     * Calls into native code to trigger all pending notifications.
     */
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
        return ContextUtils.getAppSharedPreferences().getLong(KEY_NEXT_TRIGGER, Long.MAX_VALUE);
    }

    private void removeNextTrigger() {
        ContextUtils.getAppSharedPreferences().edit().remove(KEY_NEXT_TRIGGER).apply();
    }

    private void setNextTrigger(long timestamp) {
        ContextUtils.getAppSharedPreferences().edit().putLong(KEY_NEXT_TRIGGER, timestamp).apply();
    }

    @NativeMethods
    interface Natives {
        void triggerNotifications();
    }
}
