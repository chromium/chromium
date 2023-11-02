// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Helper class to handle conditional tab strip related utilities.
 */
public class ConditionalTabStripUtils {
    public static final int UNDO_DISMISS_SNACKBAR_DURATION = 5000;
    public static final int CONDITIONAL_TAB_STRIP_DISMISS_COUNTER_ABANDONED = -1;
    @VisibleForTesting
    public static final String FEATURE_STATUS =
            ChromePreferenceKeys.CONDITIONAL_TAB_STRIP_FEATURE_STATUS;
    @VisibleForTesting
    public static final String CONTINUOUS_DISMISS_COUNTER =
            ChromePreferenceKeys.CONDITIONAL_TAB_STRIP_CONTINUOUS_DISMISS_COUNTER;
    @VisibleForTesting
    public static final String OPT_OUT_INDICATOR =
            ChromePreferenceKeys.CONDITIONAL_TAB_STRIP_OPT_OUT;
    private static final String LAST_SHOWN_TIMESTAMP =
            ChromePreferenceKeys.CONDITIONAL_TAB_STRIP_LAST_SHOWN_TIMESTAMP;
    @VisibleForTesting
    public static final String UMA_USER_STATUS_RESULT = "TabStrip.UserStatus";
    private static final String CONDITIONAL_TAB_STRIP_SESSION_TIME_MS_PARAM =
            "conditional_tab_strip_session_time_ms";
    private static final String CONDITIONAL_TAB_STRIP_INFOBAR_LIMIT_PARAM =
            "conditional_tab_strip_infobar_limit";
    private static final String CONDITIONAL_TAB_STRIP_INFOBAR_PERIOD_PARAM =
            "conditional_tab_strip_infobar_period";
    public static final IntCachedFieldTrialParameter CONDITIONAL_TAB_STRIP_SESSION_TIME_MS =
            new IntCachedFieldTrialParameter(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID,
                    CONDITIONAL_TAB_STRIP_SESSION_TIME_MS_PARAM, 3600000);
    public static final IntCachedFieldTrialParameter CONDITIONAL_TAB_STRIP_INFOBAR_LIMIT =
            new IntCachedFieldTrialParameter(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID,
                    CONDITIONAL_TAB_STRIP_INFOBAR_LIMIT_PARAM, 6);
    public static final IntCachedFieldTrialParameter CONDITIONAL_TAB_STRIP_INFOBAR_PERIOD =
            new IntCachedFieldTrialParameter(ChromeFeatureList.CONDITIONAL_TAB_STRIP_ANDROID,
                    CONDITIONAL_TAB_STRIP_INFOBAR_PERIOD_PARAM, 3);
    /**
     * A series of possible states of the conditional tab strip in a feature session. A feature
     * session is defined as usage of Chrome without a gap of more than an hour, which may include
     * multiple foregroundings/backgroundings. FeatureStatus.DEFAULT is the initial state for the
     * feature when the previous feature session has expired. The strip will not show in this state
     * until being activated. FeatureStatus.ACTIVATED is the status when conditional tab strip is
     * activated and strip will show in this state. FeatureStatus.FORBIDDEN is the state when user
     * explicitly dismisses the strip, and the strip will never reshow in this state until returning
     * to FeatureStatus.Default after feature session expiration.
     */
    @IntDef({FeatureStatus.FORBIDDEN, FeatureStatus.ACTIVATED, FeatureStatus.DEFAULT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FeatureStatus {
        int FORBIDDEN = 0;
        int ACTIVATED = 1;
        int DEFAULT = 2;
    }

    /**
     * A series of possible statuses of a user in a feature session of conditional tab strip (see
     * feature session definition in {@link #FEATURE_STATUS}). It is recorded as a histogram once
     * per feature session.
     */
    @IntDef({UserStatus.NON_USER, UserStatus.TAB_STRIP_NOT_SHOWN, UserStatus.TAB_STRIP_SHOWN,
            UserStatus.TAB_STRIP_SHOWN_AND_DISMISSED, UserStatus.TAB_STRIP_PERMANENTLY_HIDDEN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UserStatus {
        // User has never seen conditional tab strip in any previous feature sessions.
        int NON_USER = 0;
        // User has seen conditional tab strip in previous sessions before, but not in current
        // feature session.
        int TAB_STRIP_NOT_SHOWN = 1;
        // User has seen conditional tab strip in current feature session.
        int TAB_STRIP_SHOWN = 2;
        // User has seen conditional tab strip in current feature session and dismissed it.
        int TAB_STRIP_SHOWN_AND_DISMISSED = 3;
        // User has opt-outed the conditional tab strip feature.
        int TAB_STRIP_PERMANENTLY_HIDDEN = 4;
        // Update TabStripUserStatus in enums.xml when adding new items.
        int NUM_ENTRIES = 5;
    }

    /**
     * A series of possible reasons that caused conditional tab strip to show.
     */
    @IntDef({ReasonToShow.TAB_SWITCHED, ReasonToShow.NEW_TAB, ReasonToShow.LONG_PRESS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ReasonToShow {
        int TAB_SWITCHED = 0;
        int NEW_TAB = 1;
        int LONG_PRESS = 2;
        // Update TabStripReasonForShow in enums.xml when adding new items.
        int NUM_ENTRIES = 3;
    }

    /**
     * Update whether the current feature session for conditional tab strip is expired based on the
     * last time when Chrome is in background and save the status in SharedPreference for future
     * reference (see feature session definition in {@link #FEATURE_STATUS}).
     *
     * @param lastBackgroundedTimeMillis The last time the application was backgrounded. Set in
     *                                   ChromeTabbedActivity::onStartWithNative
     */
    public static void updateFeatureExpiration(final long lastBackgroundedTimeMillis) {
        long expirationTime =
                lastBackgroundedTimeMillis + CONDITIONAL_TAB_STRIP_SESSION_TIME_MS.getValue();
        if (lastBackgroundedTimeMillis == -1 || System.currentTimeMillis() > expirationTime) {
            recordUserStatus();
            updateContinuousDismissCounter();
            setFeatureStatus(FeatureStatus.DEFAULT);
        }
    }

    private static void recordUserStatus() {
        long lastShownTimeStamp = getLastShownTimeStamp();
        int previousFeatureStatus = getFeatureStatus();
        // TODO(yuezhanggg@): Right now we only use the lastShownTimeStamp as a boolean indicator.
        // We want to update the logic below so that these metrics can be recorded on a time window
        // basis, e.g. 28 days.
        if (getOptOutIndicator()) {
            recordUserStatusEnums(UserStatus.TAB_STRIP_PERMANENTLY_HIDDEN);
        } else if (lastShownTimeStamp == -1) {
            recordUserStatusEnums(UserStatus.NON_USER);
        } else if (previousFeatureStatus == FeatureStatus.DEFAULT) {
            recordUserStatusEnums(UserStatus.TAB_STRIP_NOT_SHOWN);
        } else if (previousFeatureStatus == FeatureStatus.ACTIVATED) {
            recordUserStatusEnums(UserStatus.TAB_STRIP_SHOWN);
        } else if (previousFeatureStatus == FeatureStatus.FORBIDDEN) {
            recordUserStatusEnums(UserStatus.TAB_STRIP_SHOWN_AND_DISMISSED);
        }
    }

    private static void updateContinuousDismissCounter() {
        int count = getContinuousDismissCount();
        // If the counter is set to -1, the counter is no longer needed and we should never update
        // it.
        if (count == CONDITIONAL_TAB_STRIP_DISMISS_COUNTER_ABANDONED) return;
        if (getFeatureStatus() == FeatureStatus.ACTIVATED) {
            // If the strip is activated in the previous session, we reset the counter to 0.
            setContinuousDismissCount(0);
        } else if (getFeatureStatus() == FeatureStatus.FORBIDDEN) {
            // We no longer keep the counter when the number of sessions in which user has
            // continuously dismissed the strip has reached the limit. Otherwise we update the
            // counter.
            count += 1;
            setContinuousDismissCount(count >= CONDITIONAL_TAB_STRIP_INFOBAR_LIMIT.getValue()
                            ? CONDITIONAL_TAB_STRIP_DISMISS_COUNTER_ABANDONED
                            : count);
        }
    }

    private static void recordUserStatusEnums(@UserStatus int userStatus) {
        RecordHistogram.recordEnumeratedHistogram(
                UMA_USER_STATUS_RESULT, userStatus, UserStatus.NUM_ENTRIES);
    }

    /**
     * Update the status of the conditional tab strip feature in SharedPreference.
     *
     * @param featureStatus the target {@link FeatureStatus} to set.
     */
    public static void setFeatureStatus(@FeatureStatus int featureStatus) {
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        sharedPreferencesManager.writeInt(FEATURE_STATUS, featureStatus);
    }

    /**
     * Update the timestamp of last time that conditional tab strip shows in SharedPreference with
     * current time.
     */
    public static void updateLastShownTimeStamp() {
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        sharedPreferencesManager.writeLong(LAST_SHOWN_TIMESTAMP, System.currentTimeMillis());
    }

    /**
     * Get the status of the conditional tab strip feature from SharedPreference.
     *
     * @return {@link FeatureStatus} that indicates the saved status of conditional tab strip
     *         feature.
     */
    public static @FeatureStatus int getFeatureStatus() {
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        return sharedPreferencesManager.readInt(FEATURE_STATUS, FeatureStatus.DEFAULT);
    }

    @VisibleForTesting
    public static long getLastShownTimeStamp() {
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        return sharedPreferencesManager.readLong(LAST_SHOWN_TIMESTAMP, -1);
    }

    /**
     * Get the count that records how many sessions in which user has continuously dismissed the
     * conditional tab strip from SharedPreference.
     */
    @VisibleForTesting
    public static int getContinuousDismissCount() {
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        return sharedPreferencesManager.readInt(CONTINUOUS_DISMISS_COUNTER, 0);
    }

    /**
     * Set the count that records how many sessions in which user has continuously dismissed the
     * conditional tab strip in SharedPreference.
     *
     * @param count the updated count to save into SharedPreference.
     */
    public static void setContinuousDismissCount(int count) {
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        sharedPreferencesManager.writeInt(CONTINUOUS_DISMISS_COUNTER, count);
    }

    /**
     * Get the boolean indicator that indicates whether this user has opt-outed the conditional tab
     * strip feature from SharedPreference.
     */
    public static boolean getOptOutIndicator() {
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        return sharedPreferencesManager.readBoolean(OPT_OUT_INDICATOR, false);
    }

    /**
     * Set the boolean indicator that indicates whether this user has opt-outed the conditional tab
     * strip feature in SharedPreference.
     *
     * @param isOptOut the boolean indicator to save into SharedPreference.
     */
    public static void setOptOutIndicator(boolean isOptOut) {
        SharedPreferencesManager sharedPreferencesManager = SharedPreferencesManager.getInstance();
        sharedPreferencesManager.writeBoolean(OPT_OUT_INDICATOR, isOptOut);
    }

    /**
     * Get a boolean indicator that indicates whether the undo snackbar will show for a dismissal.
     * If not, the opt-out infobar should show.
     *
     * @return whether the snackbar should show for this dismissal.
     */
    public static boolean shouldShowSnackbarForDismissal() {
        int count = ConditionalTabStripUtils.getContinuousDismissCount();
        return count == CONDITIONAL_TAB_STRIP_DISMISS_COUNTER_ABANDONED
                || (count + 1) % CONDITIONAL_TAB_STRIP_INFOBAR_PERIOD.getValue() != 0;
    }
}
