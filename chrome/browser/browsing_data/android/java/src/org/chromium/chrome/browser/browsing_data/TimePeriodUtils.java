// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import android.content.Context;

import androidx.annotation.NonNull;

import java.util.ArrayList;
import java.util.List;

/** A utility class to provide functionalities around clear browsing data {@link TimePeriod}. */
public class TimePeriodUtils {
    /** An option to be shown in the time period spiner. */
    public static class TimePeriodSpinnerOption {
        private @TimePeriod int mTimePeriod;
        private String mTitle;

        /**
         * Constructs this time period spinner option.
         * @param timePeriod The time period.
         * @param title The text that will be used to represent this item in the spinner.
         */
        public TimePeriodSpinnerOption(@TimePeriod int timePeriod, String title) {
            mTimePeriod = timePeriod;
            mTitle = title;
        }

        /**
         * @return The time period.
         */
        public @TimePeriod int getTimePeriod() {
            return mTimePeriod;
        }

        @Override
        public String toString() {
            return mTitle;
        }
    }

    /**
     * Returns the Array of time periods. Options are displayed in the same order as they appear
     * in the array.
     */
    public static TimePeriodSpinnerOption[] getTimePeriodSpinnerOptions(@NonNull Context context) {
        List<TimePeriodSpinnerOption> options = new ArrayList<>();
        options.add(
                new TimePeriodSpinnerOption(
                        TimePeriod.LAST_15_MINUTES,
                        getTimePeriodString(context, TimePeriod.LAST_15_MINUTES)));
        options.add(
                new TimePeriodSpinnerOption(
                        TimePeriod.LAST_HOUR, getTimePeriodString(context, TimePeriod.LAST_HOUR)));
        options.add(
                new TimePeriodSpinnerOption(
                        TimePeriod.LAST_DAY, getTimePeriodString(context, TimePeriod.LAST_DAY)));
        options.add(
                new TimePeriodSpinnerOption(
                        TimePeriod.LAST_WEEK, getTimePeriodString(context, TimePeriod.LAST_WEEK)));
        options.add(
                new TimePeriodSpinnerOption(
                        TimePeriod.FOUR_WEEKS,
                        getTimePeriodString(context, TimePeriod.FOUR_WEEKS)));
        options.add(
                new TimePeriodSpinnerOption(
                        TimePeriod.ALL_TIME, getTimePeriodString(context, TimePeriod.ALL_TIME)));
        return options.toArray(new TimePeriodSpinnerOption[0]);
    }

    /** Returns the string associated with the time period. */
    public static String getTimePeriodString(@NonNull Context context, @TimePeriod int timePeriod) {
        switch (timePeriod) {
            case TimePeriod.LAST_15_MINUTES:
                return context.getString(R.string.clear_browsing_data_tab_period_15_minutes);
            case TimePeriod.LAST_HOUR:
                return context.getString(R.string.clear_browsing_data_tab_period_hour);
            case TimePeriod.LAST_DAY:
                return context.getString(R.string.clear_browsing_data_tab_period_24_hours);
            case TimePeriod.LAST_WEEK:
                return context.getString(R.string.clear_browsing_data_tab_period_7_days);
            case TimePeriod.FOUR_WEEKS:
                return context.getString(R.string.clear_browsing_data_tab_period_four_weeks);
            case TimePeriod.ALL_TIME:
                return context.getString(R.string.clear_browsing_data_tab_period_everything);
            default:
                throw new IllegalStateException("Unexpected value: " + timePeriod);
        }
    }
}
