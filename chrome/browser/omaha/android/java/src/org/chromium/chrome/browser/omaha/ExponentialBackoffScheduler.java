// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;
import android.content.SharedPreferences;

import org.chromium.base.ContextUtils;
import org.chromium.base.TimeUtils;

import java.util.Random;

import javax.annotation.concurrent.NotThreadSafe;

/**
 * Manages a timer that implements exponential backoff for failed attempts.
 *
 * <p>The first timer will fire after BASE_MILLISECONDS. On a failure, the timer is changed to
 * (randomInteger[0, 2^failures) + 1) * BASE_MILLISECONDS. MAX_MILLISECONDS is used to ensure that
 * you aren't waiting years for a timer to fire.
 *
 * <p>The state is stored in shared preferences to ensure that they are kept after the device
 * sleeps. Because multiple ExponentialBackoffSchedulers can be used by different components, the
 * owning class must set the preference name.
 *
 * <p>Timestamps are recorded in RTC to avoid situations where the phone is rebooted, messing up any
 * timestamps generated using elapsedRealtime().
 *
 * <p>This class is not thread-safe because any two different classes could be accessing the same
 * SharedPreferences.
 */
@NotThreadSafe
public class ExponentialBackoffScheduler {
    // TODO(crbug.com/40150188): remove and use OmahaBase.TAG when OmahaBase.java is modularized.
    // Used in various org.chromium.chrome.browser.omaha files.
    static final String TAG = "omaha";

    private static final String PREFERENCE_DELAY = "delay";
    private static final String PREFERENCE_FAILED_ATTEMPTS = "backoffFailedAttempts";

    private static Random sRandom = new Random();

    private static final int MAX_EXPONENT = 10;

    private final long mBaseMilliseconds;
    private final long mMaxMilliseconds;
    private final String mPreferencePackage;

    /**
     * Creates a new scheduler.
     * @param packageName The name under which to store its state in SharedPreferences.
     * @param baseMilliseconds Used to calculate random backoff times.
     * @param maxMilliseconds The absolute maximum delay allowed.
     */
    public ExponentialBackoffScheduler(
            String packageName, long baseMilliseconds, long maxMilliseconds) {
        mPreferencePackage = packageName;
        mBaseMilliseconds = baseMilliseconds;
        mMaxMilliseconds = maxMilliseconds;
    }

    /** Calculates when the next event should occur, including delays due to failures. */
    public long calculateNextTimestamp() {
        return generateRandomDelay() + getCurrentTime();
    }

    public int getNumFailedAttempts() {
        SharedPreferences preferences = getSharedPreferences();
        return preferences.getInt(PREFERENCE_FAILED_ATTEMPTS, 0);
    }

    public void increaseFailedAttempts() {
        SharedPreferences preferences = getSharedPreferences();
        int numFailedAttempts = getNumFailedAttempts() + 1;
        preferences.edit().putInt(PREFERENCE_FAILED_ATTEMPTS, numFailedAttempts).apply();
    }

    public void resetFailedAttempts() {
        SharedPreferences preferences = getSharedPreferences();
        preferences.edit().putInt(PREFERENCE_FAILED_ATTEMPTS, 0).apply();
    }

    /** Returns a timestamp representing now, according to the backoff scheduler. */
    public long getCurrentTime() {
        return TimeUtils.currentTimeMillis();
    }

    /**
     * Returns the delay used to generate the last alarm.  If no previous alarm was generated,
     * return the base delay.
     */
    public long getGeneratedDelay() {
        SharedPreferences preferences = getSharedPreferences();
        return preferences.getLong(PREFERENCE_DELAY, mBaseMilliseconds);
    }

    /**
     * Determines the amount of time to wait for the current delay, then saves it.
     * @return the number of milliseconds to wait.
     */
    private long generateRandomDelay() {
        long delay;
        int numFailedAttempts = getNumFailedAttempts();
        if (numFailedAttempts == 0) {
            delay = Math.min(mBaseMilliseconds, mMaxMilliseconds);
        } else {
            int backoffCoefficient = computeConstrainedBackoffCoefficient(numFailedAttempts);
            delay = Math.min(backoffCoefficient * mBaseMilliseconds, mMaxMilliseconds);
        }

        // Save the delay for validation checks.
        SharedPreferences preferences = getSharedPreferences();
        preferences.edit().putLong(PREFERENCE_DELAY, delay).apply();
        return delay;
    }

    /**
     * Calculates a random coefficient based on the number of cumulative failed attempts.
     * @param numFailedAttempts Number of cumulative failed attempts
     * @return A random number between 1 and 2^N, where N is the smallest value of MAX_EXPONENT and
     *         numFailedAttempts
     */
    private int computeConstrainedBackoffCoefficient(int numFailedAttempts) {
        int n = Math.min(MAX_EXPONENT, numFailedAttempts);
        int twoToThePowerOfN = 1 << n;
        return sRandom.nextInt(twoToThePowerOfN) + 1;
    }

    private SharedPreferences getSharedPreferences() {
        SharedPreferences preferences =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(mPreferencePackage, Context.MODE_PRIVATE);
        return preferences;
    }
}
