// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.res.Resources;
import android.text.TextUtils;

import org.chromium.base.cached_flags.BooleanCachedFieldTrialParameter;
import org.chromium.base.cached_flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/** Utilities for the tab resumption module. */
public class TabResumptionModuleUtils {
    private static final int DEFAULT_MAX_TILES_NUMBER = 2;

    /** Callback to handle click on suggestion tiles. */
    public interface SuggestionClickCallbacks {
        // Called to open a URL.
        void onSuggestionClickByUrl(GURL gurl);

        // Called to switch to an existing Tab.
        void onSuggestionClickByTabId(int tabId);
    }

    private static final String TAB_RESUMPTION_V2_PARAM = "enable_v2";
    public static final BooleanCachedFieldTrialParameter TAB_RESUMPTION_V2 =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID,
                    TAB_RESUMPTION_V2_PARAM,
                    false);

    private static final String TAB_RESUMPTION_MAX_TILES_NUMBER_PARAM = "max_tiles_number";
    public static final IntCachedFieldTrialParameter TAB_RESUMPTION_MAX_TILES_NUMBER =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID,
                    TAB_RESUMPTION_MAX_TILES_NUMBER_PARAM,
                    DEFAULT_MAX_TILES_NUMBER);

    private static final String TAB_RESUMPTION_USE_SALIENT_IMAGE_PARAM = "use_salient_image";
    public static final BooleanCachedFieldTrialParameter TAB_RESUMPTION_USE_SALIENT_IMAGE =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID,
                    TAB_RESUMPTION_USE_SALIENT_IMAGE_PARAM,
                    false);

    private static final String TAB_RESUMPTION_SHOW_SEE_MORE_PARAM = "show_see_more";
    public static final BooleanCachedFieldTrialParameter TAB_RESUMPTION_SHOW_SEE_MORE =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID,
                    TAB_RESUMPTION_SHOW_SEE_MORE_PARAM,
                    false);

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
     * Extracts the registered, organization-identifying host and all its registry information, but
     * no subdomains, from a given URL. In particular, removes the "www." prefix.
     */
    static String getDomainUrl(GURL url) {
        String domainUrl = UrlUtilities.getDomainAndRegistry(url.getSpec(), false);
        return TextUtils.isEmpty(domainUrl) ? url.getHost() : domainUrl;
    }
}
