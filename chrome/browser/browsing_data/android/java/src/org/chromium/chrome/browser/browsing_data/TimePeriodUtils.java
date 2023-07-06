// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browsing_data;

import android.content.Context;

import androidx.annotation.NonNull;

import java.util.ArrayList;
import java.util.List;

/**
 * A utility class to provide functionalities around clear browsing data {@link TimePeriod}.
 */
public class TimePeriodUtils {
    /**
     * An option to be shown in the time period spiner.
     */
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
        options.add(new TimePeriodSpinnerOption(TimePeriod.LAST_15_MINUTES,
                context.getString(R.string.clear_browsing_data_tab_period_15_minutes)));
        options.add(new TimePeriodSpinnerOption(TimePeriod.LAST_HOUR,
                context.getString(R.string.clear_browsing_data_tab_period_hour)));
        options.add(new TimePeriodSpinnerOption(TimePeriod.LAST_DAY,
                context.getString(R.string.clear_browsing_data_tab_period_24_hours)));
        options.add(new TimePeriodSpinnerOption(TimePeriod.LAST_WEEK,
                context.getString(R.string.clear_browsing_data_tab_period_7_days)));
        options.add(new TimePeriodSpinnerOption(TimePeriod.FOUR_WEEKS,
                context.getString(R.string.clear_browsing_data_tab_period_four_weeks)));
        options.add(new TimePeriodSpinnerOption(TimePeriod.ALL_TIME,
                context.getString(R.string.clear_browsing_data_tab_period_everything)));
        return options.toArray(new TimePeriodSpinnerOption[0]);
    }
}
