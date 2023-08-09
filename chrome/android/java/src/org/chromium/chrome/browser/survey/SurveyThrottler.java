// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.survey;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.survey.ChromeSurveyController.FilteringResult;

import java.util.Calendar;
import java.util.Random;

public class SurveyThrottler {
    private final int mMaxNumber;

    /**
     * @param maxNumber The max number that used to control the rate limit.
     */
    SurveyThrottler(int maxNumber) {
        mMaxNumber = maxNumber;
    }

    /**
     * Rolls a random number to see if the user was eligible for the survey. The user will skip the
     * roll if:
     *  1. User is a first time user
     *  2. User as performed the roll today
     *  3. Max number is not setup correctly
     *
     * @return Whether the user is eligible (i.e. the random number rolled was 0).
     */
    boolean isRandomlySelectedForSurvey() {
        if (FirstRunStatus.isFirstRunTriggered()) {
            recordSurveyFilteringResult(FilteringResult.FIRST_TIME_USER);
            return false;
        }

        SharedPreferencesManager preferences = SharedPreferencesManager.getInstance();
        int lastDate = preferences.readInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, -1);
        int today = getDayOfYear();
        if (lastDate == today) {
            recordSurveyFilteringResult(FilteringResult.USER_ALREADY_SAMPLED_TODAY);
            return false;
        }

        if (mMaxNumber <= 0) {
            recordSurveyFilteringResult(FilteringResult.MAX_NUMBER_MISSING);
            return false;
        }

        preferences.writeInt(ChromePreferenceKeys.SURVEY_DATE_LAST_ROLLED, today);
        if (getRandomNumberUpTo(mMaxNumber) == 0) {
            recordSurveyFilteringResult(FilteringResult.USER_SELECTED_FOR_SURVEY);
            return true;
        } else {
            recordSurveyFilteringResult(FilteringResult.ROLLED_NON_ZERO_NUMBER);
            return false;
        }
    }

    /**
     * @param max The max threshold for the random number generator.
     * @return A random number from 0 (inclusive) to the max number (exclusive).
     */
    @VisibleForTesting
    int getRandomNumberUpTo(int max) {
        return new Random().nextInt(max);
    }

    /** @return The day of the year for today. */
    @VisibleForTesting
    int getDayOfYear() {
        ThreadUtils.assertOnBackgroundThread();
        return Calendar.getInstance().get(Calendar.DAY_OF_YEAR);
    }

    static void recordSurveyFilteringResult(@FilteringResult int value) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Survey.SurveyFilteringResults", value, FilteringResult.NUM_ENTRIES);
    }
}
