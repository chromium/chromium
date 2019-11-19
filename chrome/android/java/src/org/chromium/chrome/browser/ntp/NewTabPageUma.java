// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.os.SystemClock;
import android.view.View;
import android.view.ViewTreeObserver;

import androidx.annotation.IntDef;

import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.rappor.RapporServiceBridge;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.UrlUtilitiesJni;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.PageTransition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Records UMA stats for which actions the user takes on the NTP in the
 * "NewTabPage.ActionAndroid2" histogram.
 */
public final class NewTabPageUma {
    private NewTabPageUma() {}

    // Possible actions taken by the user on the NTP. These values are also defined in
    // enums.xml as NewTabPageActionAndroid2.
    // WARNING: these values must stay in sync with enums.xml.

    /** User performed a search using the omnibox. */
    private static final int ACTION_SEARCHED_USING_OMNIBOX = 0;

    /** User navigated to Google search homepage using the omnibox. */
    private static final int ACTION_NAVIGATED_TO_GOOGLE_HOMEPAGE = 1;

    /** User navigated to any other page using the omnibox. */
    private static final int ACTION_NAVIGATED_USING_OMNIBOX = 2;

    /** User opened a most visited tile. */
    public static final int ACTION_OPENED_MOST_VISITED_TILE = 3;

    /** User opened the recent tabs manager. */
    public static final int ACTION_OPENED_RECENT_TABS_MANAGER = 4;

    /** User opened the history manager. */
    public static final int ACTION_OPENED_HISTORY_MANAGER = 5;

    /** User opened the bookmarks manager. */
    public static final int ACTION_OPENED_BOOKMARKS_MANAGER = 6;

    /** User opened the downloads manager. */
    public static final int ACTION_OPENED_DOWNLOADS_MANAGER = 7;

    /** User navigated to the webpage for a snippet shown on the NTP. */
    public static final int ACTION_OPENED_SNIPPET = 8;

    /** User clicked on the "learn more" link in the footer. */
    public static final int ACTION_CLICKED_LEARN_MORE = 9;

    /** User clicked on the "Refresh" button in the "all dismissed" state. */
    public static final int ACTION_CLICKED_ALL_DISMISSED_REFRESH = 10;

    /** User opened an explore sites tile. */
    public static final int ACTION_OPENED_EXPLORE_SITES_TILE = 11;

    /** The number of possible actions. */
    private static final int NUM_ACTIONS = 12;

    /** User navigated to a page using the omnibox. */
    private static final int RAPPOR_ACTION_NAVIGATED_USING_OMNIBOX = 0;

    /** User navigated to a page using one of the suggested tiles. */
    public static final int RAPPOR_ACTION_VISITED_SUGGESTED_TILE = 1;

    /** Regular NTP impression (usually when a new tab is opened). */
    public static final int NTP_IMPRESSION_REGULAR = 0;

    /** Potential NTP impressions (instead of blank page if no tab is open). */
    public static final int NTP_IMPESSION_POTENTIAL_NOTAB = 1;

    /** The number of possible NTP impression types */
    private static final int NUM_NTP_IMPRESSION = 2;

    /** The maximal number of suggestions per section. Keep in sync with kMaxSuggestionsPerCategory
     * in content_suggestions_metrics.cc. */
    private static final int MAX_SUGGESTIONS_PER_SECTION = 20;

    /**
     * Possible results when updating content suggestions list in the UI. Keep in sync with the
     * ContentSuggestionsUIUpdateResult2 enum in enums.xml. Do not remove or change existing
     * values other than NUM_UI_UPDATE_RESULTS.
     */
    @IntDef({ContentSuggestionsUIUpdateResult.SUCCESS_APPENDED,
            ContentSuggestionsUIUpdateResult.SUCCESS_REPLACED,
            ContentSuggestionsUIUpdateResult.FAIL_ALL_SEEN,
            ContentSuggestionsUIUpdateResult.FAIL_DISABLED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContentSuggestionsUIUpdateResult {
        /**
         * The content suggestions are successfully appended (because they are set for the first
         * time or explicitly marked to be appended).
         */
        int SUCCESS_APPENDED = 0;

        /**
         * Update successful, suggestions were replaced (some of them possibly seen, the exact
         * number reported in a separate histogram).
         */
        int SUCCESS_REPLACED = 1;

        /** Update failed, all previous content suggestions have been seen (and kept). */
        int FAIL_ALL_SEEN = 2;

        /** Update failed, because it is disabled by a variation parameter. */
        int FAIL_DISABLED = 3;

        int NUM_ENTRIES = 4;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused. This maps directly to
    // the ContentSuggestionsDisplayStatus enum defined in tools/metrics/enums.xml.
    @IntDef({ContentSuggestionsDisplayStatus.VISIBLE, ContentSuggestionsDisplayStatus.COLLAPSED,
            ContentSuggestionsDisplayStatus.DISABLED_BY_POLICY})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ContentSuggestionsDisplayStatus {
        int VISIBLE = 0;
        int COLLAPSED = 1;
        int DISABLED_BY_POLICY = 2;
        int NUM_ENTRIES = 3;
    }

    /** The NTP was loaded in a cold startup. */
    private static final int LOAD_TYPE_COLD_START = 0;

    /** The NTP was loaded in a warm startup. */
    private static final int LOAD_TYPE_WARM_START = 1;

    /**
     * The NTP was loaded at some other time after activity creation and the user interacted with
     * the activity in the meantime.
     */
    private static final int LOAD_TYPE_OTHER = 2;

    /** The number of load types. */
    private static final int LOAD_TYPE_COUNT = 3;

    /**
     * Records an action taken by the user on the NTP.
     * @param action One of the ACTION_* values defined in this class.
     */
    public static void recordAction(int action) {
        assert action >= 0;
        assert action < NUM_ACTIONS;
        RecordHistogram.recordEnumeratedHistogram("NewTabPage.ActionAndroid2", action, NUM_ACTIONS);
    }

    /**
     * Record that the user has navigated away from the NTP using the omnibox.
     * @param destinationUrl The URL to which the user navigated.
     * @param transitionType The transition type of the navigation, from PageTransition.java.
     */
    public static void recordOmniboxNavigation(String destinationUrl, int transitionType) {
        if ((transitionType & PageTransition.CORE_MASK) == PageTransition.GENERATED) {
            recordAction(ACTION_SEARCHED_USING_OMNIBOX);
        } else {
            if (UrlUtilitiesJni.get().isGoogleHomePageUrl(destinationUrl)) {
                recordAction(ACTION_NAVIGATED_TO_GOOGLE_HOMEPAGE);
            } else {
                recordAction(ACTION_NAVIGATED_USING_OMNIBOX);
            }
            recordExplicitUserNavigation(destinationUrl, RAPPOR_ACTION_NAVIGATED_USING_OMNIBOX);
        }
    }

    /**
     * Record the eTLD+1 for a website explicitly visited by the user, using Rappor.
     */
    public static void recordExplicitUserNavigation(String destinationUrl, int rapporMetric) {
        switch (rapporMetric) {
            case RAPPOR_ACTION_NAVIGATED_USING_OMNIBOX:
                RapporServiceBridge.sampleDomainAndRegistryFromURL(
                        "NTP.ExplicitUserAction.PageNavigation.OmniboxNonSearch", destinationUrl);
                return;
            case RAPPOR_ACTION_VISITED_SUGGESTED_TILE:
                RapporServiceBridge.sampleDomainAndRegistryFromURL(
                        "NTP.ExplicitUserAction.PageNavigation.NTPTileClick", destinationUrl);
                return;
            default:
                return;
        }
    }

    /**
     * Records how content suggestions have been updated in the UI.
     * @param result result key, one of {@link ContentSuggestionsUIUpdateResult}'s values.
     */
    public static void recordUIUpdateResult(
            @ContentSuggestionsUIUpdateResult int result) {
        RecordHistogram.recordEnumeratedHistogram("NewTabPage.ContentSuggestions.UIUpdateResult2",
                result, ContentSuggestionsUIUpdateResult.NUM_ENTRIES);
    }

    /**
     * Record how many content suggestions have been seen by the user in the UI section before the
     * section was successfully updated.
     * @param numberOfSuggestionsSeen The number of content suggestions seen so far in the section.
     */
    public static void recordNumberOfSuggestionsSeenBeforeUIUpdateSuccess(
            int numberOfSuggestionsSeen) {
        assert numberOfSuggestionsSeen >= 0;
        RecordHistogram.recordCount100Histogram(
                "NewTabPage.ContentSuggestions.UIUpdateSuccessNumberOfSuggestionsSeen",
                numberOfSuggestionsSeen);
    }

    /**
     * Record a NTP impression (even potential ones to make informed product decisions). If the
     * impression type is {@link NewTabPageUma#NTP_IMPRESSION_REGULAR}, also records a user action.
     * @param impressionType Type of the impression from NewTabPageUma.java
     */
    public static void recordNTPImpression(int impressionType) {
        assert impressionType >= 0;
        assert impressionType < NUM_NTP_IMPRESSION;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.NTP.Impression", impressionType, NUM_NTP_IMPRESSION);
    }

    /**
     * Records how often new tabs with a NewTabPage are created. This helps to determine how often
     * users navigate back to already opened NTPs.
     * @param tabModelSelector Model selector controlling the creation of new tabs.
     */
    public static void monitorNTPCreation(TabModelSelector tabModelSelector) {
        tabModelSelector.addObserver(new TabCreationRecorder());
    }

    /**
     * Records the type of load for the NTP, such as cold or warm start.
     */
    public static void recordLoadType(ChromeActivity activity) {
        if (activity.getLastUserInteractionTime() > 0) {
            RecordHistogram.recordEnumeratedHistogram(
                    "NewTabPage.LoadType", LOAD_TYPE_OTHER, LOAD_TYPE_COUNT);
            return;
        }

        if (activity.hadWarmStart()) {
            RecordHistogram.recordEnumeratedHistogram(
                    "NewTabPage.LoadType", LOAD_TYPE_WARM_START, LOAD_TYPE_COUNT);
            return;
        }

        RecordHistogram.recordEnumeratedHistogram(
                "NewTabPage.LoadType", LOAD_TYPE_COLD_START, LOAD_TYPE_COUNT);
    }

    /**
     * Records the network status of the user.
     */
    public static void recordIsUserOnline() {
        RecordHistogram.recordBooleanHistogram(
                "NewTabPage.MobileIsUserOnline", NetworkChangeNotifier.isOnline());
    }

    /**
     * Records the time duration that the NTP was visible.
     * @param lastShownTimeNs A long as returned by System#nanoTime() - this should have been
     *                        called at the moment the new tab page is shown.
     */
    public static void recordTimeSpentOnNtp(long lastShownTimeNs) {
        RecordHistogram.recordMediumTimesHistogram("NewTabPage.TimeSpent",
                (System.nanoTime() - lastShownTimeNs) / TimeUtils.NANOSECONDS_PER_MILLISECOND);
    }
    /**
     * Records how much time elapsed from start until the search box became available to the user.
     */
    public static void recordSearchAvailableLoadTime(ChromeActivity activity) {
        // Log the time it took for the search box to be displayed at startup, based on the
        // timestamp on the intent for the activity. If the user has interacted with the
        // activity already, it's not a startup, and the timestamp on the activity would not be
        // relevant either.
        if (activity.getLastUserInteractionTime() != 0) return;
        long timeFromIntent = SystemClock.elapsedRealtime()
                - IntentHandler.getTimestampFromIntent(activity.getIntent());
        if (activity.hadWarmStart()) {
            RecordHistogram.recordMediumTimesHistogram(
                    "NewTabPage.SearchAvailableLoadTime2.WarmStart", timeFromIntent);
        } else {
            RecordHistogram.recordMediumTimesHistogram(
                    "NewTabPage.SearchAvailableLoadTime2.ColdStart", timeFromIntent);
        }
    }

    /**
     * Records number of prefetched article suggestions, which were available when content
     * suggestions surface was opened and there was no network connection.
     */
    public static void recordPrefetchedArticleSuggestionsCount(int count) {
        RecordHistogram.recordEnumeratedHistogram(
                "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible."
                        + "Articles.Prefetched.Offline2",
                count, MAX_SUGGESTIONS_PER_SECTION);
    }

    /**
     * Records position of a prefetched article suggestion, which was seen by the user on the
     * suggestions surface when there was no network connection.
     */
    public static void recordPrefetchedArticleSuggestionImpressionPosition(int positionInSection) {
        RecordHistogram.recordEnumeratedHistogram("NewTabPage.ContentSuggestions.Shown.Articles."
                        + "Prefetched.Offline2",
                positionInSection, MAX_SUGGESTIONS_PER_SECTION);
    }

    /**
     * Records Content Suggestions Display Status when NTPs opened.
     */
    public static void recordContentSuggestionsDisplayStatus() {
        @ContentSuggestionsDisplayStatus
        int status = ContentSuggestionsDisplayStatus.VISIBLE;
        if (!PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_SECTION_ENABLED)) {
            // Disabled by policy.
            status = ContentSuggestionsDisplayStatus.DISABLED_BY_POLICY;
        } else if (!PrefServiceBridge.getInstance().getBoolean(Pref.NTP_ARTICLES_LIST_VISIBLE)) {
            // Articles are collapsed.
            status = ContentSuggestionsDisplayStatus.COLLAPSED;
        }

        RecordHistogram.recordEnumeratedHistogram("ContentSuggestions.Feed.DisplayStatusOnOpen",
                status, ContentSuggestionsDisplayStatus.NUM_ENTRIES);
    }

    /**
     * Records the number of new NTPs opened in a new tab. Use through
     * {@link NewTabPageUma#monitorNTPCreation(TabModelSelector)}.
     */
    private static class TabCreationRecorder extends EmptyTabModelSelectorObserver {
        @Override
        public void onNewTabCreated(Tab tab) {
            if (!NewTabPage.isNTPUrl(tab.getUrl())) return;
            RecordUserAction.record("MobileNTPOpenedInNewTab");
        }
    }

    /**
     * Setups up an onPreDraw listener for the given view to emit a metric exactly once. The view
     * should be guaranteed to be shown on the page/screen on every load, otherwise the metric
     * may not be emitted, or worse not emitted promptly.
     * @param view The UI element to track.
     * @param constructedTimeNs The timestamp at which the new tab page's construction started.
     */
    public static void trackTimeToFirstDraw(View view, long constructedTimeNs) {
        // Use preDraw instead of draw because api level 25 and earlier doesn't seem to call the
        // onDraw listener. Also, the onDraw version cannot be removed inside of the
        // notification, which complicates this.
        view.getViewTreeObserver().addOnPreDrawListener(new ViewTreeObserver.OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                long timeToFirstDrawMs = (System.nanoTime() - constructedTimeNs)
                        / TimeUtils.NANOSECONDS_PER_MILLISECOND;
                RecordHistogram.recordTimesHistogram(
                        "NewTabPage.TimeToFirstDraw2", timeToFirstDrawMs);
                view.getViewTreeObserver().removeOnPreDrawListener(this);
                return true;
            }
        });
    }
}
