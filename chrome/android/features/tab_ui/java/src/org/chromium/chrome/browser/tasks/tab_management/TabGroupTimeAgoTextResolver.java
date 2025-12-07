// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.Resources;
import android.util.Pair;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupTimeAgo.TimestampEvent;
import org.chromium.chrome.tab_ui.R;

import java.time.Clock;
import java.time.temporal.ChronoUnit;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Resolves text to display to the user for how long ago the tab group event occurred. */
@NullMarked
public class TabGroupTimeAgoTextResolver {

    // The order is important, as each pair is checked sequentially. The first to have at least one
    // of the given duration is used.
    private static final List<Pair<ChronoUnit, Integer>> CREATED_CHRONO_UNIT_AND_PLURAL_RES =
            List.of(
                    new Pair<>(ChronoUnit.YEARS, R.plurals.tab_groups_created_ago_years),
                    new Pair<>(ChronoUnit.MONTHS, R.plurals.tab_groups_created_ago_months),
                    new Pair<>(ChronoUnit.WEEKS, R.plurals.tab_groups_created_ago_weeks),
                    new Pair<>(ChronoUnit.DAYS, R.plurals.tab_groups_created_ago_days),
                    new Pair<>(ChronoUnit.HOURS, R.plurals.tab_groups_created_ago_hours),
                    new Pair<>(ChronoUnit.MINUTES, R.plurals.tab_groups_created_ago_minutes));

    private static final List<Pair<ChronoUnit, Integer>> UPDATED_CHRONO_UNIT_AND_PLURAL_RES =
            List.of(
                    new Pair<>(ChronoUnit.YEARS, R.plurals.tab_groups_updated_ago_years),
                    new Pair<>(ChronoUnit.MONTHS, R.plurals.tab_groups_updated_ago_months),
                    new Pair<>(ChronoUnit.WEEKS, R.plurals.tab_groups_updated_ago_weeks),
                    new Pair<>(ChronoUnit.DAYS, R.plurals.tab_groups_updated_ago_days),
                    new Pair<>(ChronoUnit.HOURS, R.plurals.tab_groups_updated_ago_hours),
                    new Pair<>(ChronoUnit.MINUTES, R.plurals.tab_groups_updated_ago_minutes));

    private final Resources mResources;
    private final Clock mClock;

    /**
     * @param resources Used to resolve strings against.
     * @param clock Used to get the current time.
     */
    public TabGroupTimeAgoTextResolver(Resources resources, Clock clock) {
        mResources = resources;
        mClock = clock;
    }

    /**
     * @param eventMs The time of the event for the tab group in milliseconds.
     * @param eventType The type of event the timestamp represents.
     * @return Simple text for how long ago the event occurred for the tab group.
     */
    // duration.getSeconds should be toSeconds after api 31.
    @SuppressWarnings("JavaDurationGetSecondsToToSeconds")
    public String resolveTimeAgoText(long eventMs, @TimestampEvent int eventType) {
        List<Pair<ChronoUnit, Integer>> selectedChronoUnitAndPluralRes =
                eventType == TimestampEvent.CREATED
                        ? CREATED_CHRONO_UNIT_AND_PLURAL_RES
                        : UPDATED_CHRONO_UNIT_AND_PLURAL_RES;
        long nowMillis = mClock.millis();
        int seconds = (int) TimeUnit.MILLISECONDS.toSeconds(nowMillis - eventMs);
        for (Pair<ChronoUnit, Integer> pair : selectedChronoUnitAndPluralRes) {
            int count = (int) (seconds / pair.first.getDuration().getSeconds());
            if (count >= 1) {
                return mResources.getQuantityString(pair.second, count, count);
            }
        }

        int timeAgoNowResId =
                eventType == TimestampEvent.CREATED
                        ? R.string.tab_groups_created_ago_now
                        : R.string.tab_groups_updated_ago_now;
        return mResources.getString(timeAgoNowResId);
    }
}
