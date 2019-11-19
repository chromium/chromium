// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Hosts common code for search engine choice metrics reporting. */
public class SearchEngineChoiceMetrics {
    /** Key used to store the default Search Engine Type before choice is presented. */
    @VisibleForTesting
    public static final String PREF_SEARCH_ENGINE_CHOICE_DEFAULT_TYPE_BEFORE =
            "search_engine_choice_default_type_before";

    /**
     * AndroidSearchEngineChoiceEvents defined in tools/metrics/histograms/enums.xml. These values
     * are persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */

    @IntDef({Events.SNACKBAR_SHOWN, Events.PROMPT_FOLLOWED, Events.SEARCH_ENGINE_CHANGED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Events {
        int SNACKBAR_SHOWN = 0;
        int PROMPT_FOLLOWED = 1;
        int SEARCH_ENGINE_CHANGED = 2;
        int MAX = 3;
    }

    /**
     * AndroidSearchEngineChoiceEventsV2 defined in tools/metrics/histograms/enums.xml. These values
     * are persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({EventsV2.CHOICE_REQUEST_RECEIVED, EventsV2.CHOICE_SKIPPED,
            EventsV2.CHOICE_REQUEST_NO_DATA, EventsV2.CHOICE_REQUEST_VALID,
            EventsV2.CHOICE_REQUEST_METADATA_NULL, EventsV2.CHOICE_REQUEST_PARSE_FAILED,
            EventsV2.PREVIOUS_CHOICE_REQUEST_FAILED, EventsV2.CHOICE_REQUEST_SUCCESS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface EventsV2 {
        int CHOICE_REQUEST_RECEIVED = 0;
        int CHOICE_SKIPPED = 1;
        int CHOICE_REQUEST_NO_DATA = 2;
        int CHOICE_REQUEST_VALID = 3;
        int CHOICE_REQUEST_METADATA_NULL = 4;
        int CHOICE_REQUEST_PARSE_FAILED = 5;
        int PREVIOUS_CHOICE_REQUEST_FAILED = 6;
        int CHOICE_REQUEST_SUCCESS = 7;
        int MAX = 8;
    }

    /**
     * Records an event to the search choice histogram. See {@link Events} and histograms.xml for
     * more details.
     * @param event The {@link Events} to be reported.
     */
    public static void recordEvent(@Events int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.SearchEngineChoice.Events", event, Events.MAX);
    }

    /**
     * Records an event to the search choice histogram. See {@link EventsV2} and histograms.xml for
     * more details.
     * @param event The {@link EventsV2} to be reported.
     */
    public static void recordEventV2(@EventsV2 int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.SearchEngineChoice.EventsV2", event, EventsV2.MAX);
    }

    /** Records the search engine type before the user made a choice about which engine to use. */
    public static void recordSearchEngineTypeBeforeChoice() {
        @SearchEngineType
        int currentSearchEngineType = getDefaultSearchEngineType();
        RecordHistogram.recordEnumeratedHistogram(
                "Android.SearchEngineChoice.SearchEngineBeforeChoicePrompt",
                currentSearchEngineType, SearchEngineType.SEARCH_ENGINE_MAX);
        setPreviousSearchEngineType(currentSearchEngineType);
    }

    /**
     * Records the search engine type after the user chooses a different search engine.
     * @return Whether the search engine was changed.
     **/
    public static boolean recordSearchEngineTypeAfterChoice() {
        if (!isSearchEnginePossiblyDifferent()) return false;

        @SearchEngineType
        int previousSearchEngineType = getPreviousSearchEngineType();
        @SearchEngineType
        int currentSearchEngineType = getDefaultSearchEngineType();
        boolean didChangeEngine = previousSearchEngineType != currentSearchEngineType;
        if (didChangeEngine) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.SearchEngineChoice.ChosenSearchEngine", currentSearchEngineType,
                    SearchEngineType.SEARCH_ENGINE_MAX);
        }
        removePreviousSearchEngineType();
        return didChangeEngine;
    }

    /** @return True if the current search engine is possibly different from the previous one. */
    static boolean isSearchEnginePossiblyDifferent() {
        return ContextUtils.getAppSharedPreferences().contains(
                PREF_SEARCH_ENGINE_CHOICE_DEFAULT_TYPE_BEFORE);
    }

    /** Remove the stored choice from prefs. */
    @VisibleForTesting
    static void removePreviousSearchEngineType() {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .remove(PREF_SEARCH_ENGINE_CHOICE_DEFAULT_TYPE_BEFORE)
                .apply();
    }

    /** Retrieves the previously set search engine from Android prefs. */
    @VisibleForTesting
    @SearchEngineType
    static int getPreviousSearchEngineType() {
        return ContextUtils.getAppSharedPreferences().getInt(
                PREF_SEARCH_ENGINE_CHOICE_DEFAULT_TYPE_BEFORE,
                SearchEngineType.SEARCH_ENGINE_UNKNOWN);
    }

    /**
     * Sets the current default search engine as the previously set search engine in Android prefs.
     */
    @VisibleForTesting
    static void setPreviousSearchEngineType(@SearchEngineType int engine) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putInt(PREF_SEARCH_ENGINE_CHOICE_DEFAULT_TYPE_BEFORE, engine)
                .apply();
    }

    /** Translates from the default search engine url to the {@link SearchEngineType} int. */
    @VisibleForTesting
    @SearchEngineType
    static int getDefaultSearchEngineType() {
        TemplateUrlService templateUrlService = TemplateUrlServiceFactory.get();
        TemplateUrl currentSearchEngine = templateUrlService.getDefaultSearchEngineTemplateUrl();
        if (currentSearchEngine == null) return SearchEngineType.SEARCH_ENGINE_UNKNOWN;
        return templateUrlService.getSearchEngineTypeFromTemplateUrl(
                currentSearchEngine.getKeyword());
    }
}
