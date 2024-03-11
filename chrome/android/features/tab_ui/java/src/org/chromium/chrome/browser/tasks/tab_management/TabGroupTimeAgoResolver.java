// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.Resources;
import android.util.Pair;

import org.chromium.chrome.tab_ui.R;

import java.time.Clock;
import java.time.temporal.ChronoUnit;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Resolves text to display to the user for how long ago the tab group was created. */
public class TabGroupTimeAgoResolver {
    // The order is important, as each pair is checked sequentially. The first to have at least one
    // of the given duration is used.
    private static final List<Pair<ChronoUnit, Integer>> CHRONO_UNIT_AND_PLURAL_RES =
            Arrays.asList(
                    new Pair<>(ChronoUnit.YEARS, R.plurals.tab_groups_created_ago_years),
                    new Pair<>(ChronoUnit.MONTHS, R.plurals.tab_groups_created_ago_months),
                    new Pair<>(ChronoUnit.WEEKS, R.plurals.tab_groups_created_ago_weeks),
                    new Pair<>(ChronoUnit.DAYS, R.plurals.tab_groups_created_ago_days),
                    new Pair<>(ChronoUnit.HOURS, R.plurals.tab_groups_created_ago_hours),
                    new Pair<>(ChronoUnit.MINUTES, R.plurals.tab_groups_created_ago_minutes));

    private final Resources mResources;
    private final Clock mClock;

    /**
     * @param resources Used to resolve strings against.
     * @param clock Used to get the current time.
     */
    public TabGroupTimeAgoResolver(Resources resources, Clock clock) {
        mResources = resources;
        mClock = clock;
    }

    /**
     * @param creationMillis The creation time of the tab group.
     * @return Simple text for how long ago the tab group was created.
     */
    public String resolveTimeAgoText(long creationMillis) {
        long nowMillis = mClock.millis();
        int seconds = (int) TimeUnit.MILLISECONDS.toSeconds(nowMillis - creationMillis);
        for (Pair<ChronoUnit, Integer> pair : CHRONO_UNIT_AND_PLURAL_RES) {
            int count = (int) (seconds / pair.first.getDuration().getSeconds());
            if (count >= 1) {
                return mResources.getQuantityString(pair.second, count, count);
            }
        }

        return mResources.getString(R.string.tab_groups_created_ago_now);
    }
}
