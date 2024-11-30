// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.res.Resources;
import android.text.TextUtils;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Utilities for the tab resumption module. */
public class TabResumptionModuleUtils {
    static final int DISPLAY_TEXT_MAX_LINES_DEFAULT = 3;
    static final int DISPLAY_TEXT_MAX_LINES_WITH_REASON = 2;

    /** Callback to handle click on suggestion tiles. */
    public interface SuggestionClickCallback {
        void onSuggestionClicked(SuggestionEntry entry);
    }

    /** Overrides getCurrentTime() return value, for testing. */
    public interface FakeGetCurrentTimeMs {
        long get();
    }

    /**
     * A set of the host URLs of default native apps on Android. This set should be keep consistent
     * with kDefaultAppBlocklist in {@link
     * components/visited_url_ranking/internal/url_visit_util.h}.
     */
    static final Set<String> sDefaultAppBlocklist =
            Set.of(
                    "assistant.google.com",
                    "calendar.google.com",
                    "docs.google.com",
                    "drive.google.com",
                    "mail.google.com",
                    "music.youtube.com",
                    "m.youtube.com",
                    "photos.google.com",
                    "www.youtube.com");

    private static FakeGetCurrentTimeMs sFakeGetCurrentTimeMs;

    /**
     * Overrides the getCurrentTimeMs() results. Using a function instead of static value for
     * flexibility, which is useful for simulating an advancing clock. Passing null disables this.
     */
    static void setFakeCurrentTimeMsForTesting(FakeGetCurrentTimeMs fakeGetCurrentTimeMs) {
        sFakeGetCurrentTimeMs = fakeGetCurrentTimeMs;
    }

    /** Returns the current time in ms since the epock. Mockable. */
    static long getCurrentTimeMs() {
        return (sFakeGetCurrentTimeMs == null)
                ? System.currentTimeMillis()
                : sFakeGetCurrentTimeMs.get();
    }

    /**
     * Extracts the registered, organization-identifying host and all its registry information, but
     * no subdomains, from a given URL. In particular, removes the "www." prefix.
     */
    static String getDomainUrl(GURL url) {
        String domainUrl = UrlUtilities.getDomainAndRegistry(url.getSpec(), false);
        return TextUtils.isEmpty(domainUrl) ? url.getHost() : domainUrl;
    }

    /**
     * Returns whether to exclude the given URL. It returns true if the host of the URL matches one
     * of the default native apps.
     *
     * @param url The URL of the suggestion.
     */
    static boolean shouldExcludeUrl(GURL url) {
        return ChromeFeatureList.sTabResumptionModuleAndroidUseDefaultAppFilter.getValue()
                && sDefaultAppBlocklist.contains(url.getHost());
    }

    /**
     * Computes the string representation of how recent an event was, given the time delta.
     *
     * @param res Resources for string resource retrieval.
     * @param timeDelta Time delta in milliseconds.
     */
    static String getRecencyString(Resources res, long timeDeltaMs) {
        if (timeDeltaMs < 0L) timeDeltaMs = 0L;

        long daysElapsed = TimeUnit.MILLISECONDS.toDays(timeDeltaMs);
        if (daysElapsed > 0L) {
            return res.getQuantityString(R.plurals.n_days_ago, (int) daysElapsed, daysElapsed);
        }

        long hoursElapsed = TimeUnit.MILLISECONDS.toHours(timeDeltaMs);
        if (hoursElapsed > 0L) {
            return res.getQuantityString(
                    R.plurals.n_hours_ago_narrow, (int) hoursElapsed, hoursElapsed);
        }

        // Bound recency to 1 min.
        long minutesElapsed = Math.max(1L, TimeUnit.MILLISECONDS.toMinutes(timeDeltaMs));
        return res.getQuantityString(
                R.plurals.n_minutes_ago_narrow, (int) minutesElapsed, minutesElapsed);
    }

    /**
     * Returns whether the suggestions to show are finalized, i.e., don't need to match local Tabs.
     */
    static boolean areSuggestionsFinalized(SuggestionBundle bundle) {
        if (bundle == null || bundle.entries == null || bundle.entries.isEmpty()) return true;

        if (bundle.entries.size() == 1) {
            return !bundle.entries.get(0).getNeedMatchLocalTab();
        } else {
            return !bundle.entries.get(0).getNeedMatchLocalTab()
                    && !bundle.entries.get(1).getNeedMatchLocalTab();
        }
    }
}
