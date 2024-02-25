// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Centralizes UMA data collection for Related Searches. All calls must be made from the UI thread.
 */
public class RelatedSearchesUma {
    // Constants for user permissions histogram.
    @IntDef({
        Permissions.SEND_NOTHING,
        Permissions.SEND_URL,
        Permissions.SEND_CONTENT,
        Permissions.SEND_URL_AND_CONTENT,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface Permissions {
        int SEND_NOTHING = 0;
        int SEND_URL = 1;
        int SEND_CONTENT = 2;
        int SEND_URL_AND_CONTENT = 3;
        int NUM_ENTRIES = 4;
    }

    // Constants with ScrollAndClickStatus in enums.xml.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        ScrollAndClickStatus.NO_SCROLL_NO_CLICK,
        ScrollAndClickStatus.NO_SCROLL_CLICKED,
        ScrollAndClickStatus.SCROLLED_NO_CLICK,
        ScrollAndClickStatus.SCROLLED_CLICKED,
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface ScrollAndClickStatus {
        int NO_SCROLL_NO_CLICK = 0;
        int NO_SCROLL_CLICKED = 1;
        int SCROLLED_NO_CLICK = 2;
        int SCROLLED_CLICKED = 3;
        int NUM_ENTRIES = 4;
    }

    /**
     * Logs a histogram indicating which privacy permissions are available that Related Searches
     * cares about. This ignores any language constraint.
     * <p>This can be called multiple times for each user from any part of the code that's freqently
     * executed.
     * @param canSendUrl Whether this user has allowed sending page URL info to Google.
     * @param canSendContent Whether the user can send page content to Google (has accepted the
     *        Contextual Search opt-in).
     */
    static void logRelatedSearchesPermissionsForAllUsers(
            boolean canSendUrl, boolean canSendContent) {
        @Permissions int permissionsEnum;
        if (canSendUrl) {
            permissionsEnum =
                    canSendContent ? Permissions.SEND_URL_AND_CONTENT : Permissions.SEND_URL;
        } else {
            permissionsEnum = canSendContent ? Permissions.SEND_CONTENT : Permissions.SEND_NOTHING;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Search.RelatedSearches.AllUserPermissions",
                permissionsEnum,
                Permissions.NUM_ENTRIES);
    }

    /**
     * Logs a histogram indicating that a user is qualified for the Related Searches experiment
     * regardless of whether that feature is enabled. This uses a boolean histogram but always
     * logs true in order to get a raw bucket count (without using a user action, as suggested
     * in the User Action Guidelines doc).
     * <p>We use this to gauge whether each group has a balanced number of qualified users.
     * Can be logged multiple times since we'll just look at the user-count of this histogram.
     * This should be called any time a gesture is detected that could trigger a Related Search
     * if the feature were enabled.
     */
    static void logRelatedSearchesQualifiedUsers() {
        RecordHistogram.recordBooleanHistogram("Search.RelatedSearches.QualifiedUsers", true);
    }

    /**
     * Logs that a Related Searches suggestion was selected by the user and records its position.
     * A position of 0 indicates that the query is the default selection search. This may not be a
     * possible position in some implementations. All indices from 1 on are true Related Searches
     * suggestions.
     * @param position The zero-based position of the suggestion in the UI, or the one-based
     *    position of the suggestion in the list of those returned by the server in cases where the
     *    UI does not show the default selection search in position 0.
     */
    public static void logSelectedSuggestionIndex(int position) {
        RecordHistogram.recordCount1MHistogram(
                "Search.RelatedSearches.SelectedSuggestionIndex", position);
    }

    /**
     * Logs that a Chip was selected by the user in a carousel and records its position.
     * The indexes indicate the physical position of the chip in the carousel, not the
     * logical association with any suggestion (since the first position is variable).
     * @param position The 0-based position in the carousel.
     */
    public static void logSelectedCarouselIndex(int position) {
        RecordHistogram.recordCount1MHistogram(
                "Search.RelatedSearches.SelectedCarouselIndex", position);
    }

    /**
     * Logs the CTR for a Related Searches user interaction. Call this function with either
     * {@code false} or {@code true} when the UI is closed depending on whether the user chose any
     * suggestion.
     * @param clicked Whether the user clicked any suggestion or not after they were presented.
     */
    public static void logCtr(boolean clicked) {
        RecordHistogram.recordBooleanHistogram("Search.RelatedSearches.CTR", clicked);
    }

    /**
     * Logs the number of suggestions that were selected in a bottom-bar search session.
     * @param numberOfSuggestionsClicked A count of all the clicks on any suggestion in the
     * UI, including the default selection search (when shown in within the suggestions UI).
     */
    public static void logNumberOfSuggestionsClicked(int numberOfSuggestionsClicked) {
        if (numberOfSuggestionsClicked > 0) {
            RecordHistogram.recordCount1MHistogram(
                    "Search.RelatedSearches.NumberOfSuggestionsClicked2",
                    numberOfSuggestionsClicked);
        }
    }

    /**
     * Logs that the last visible item position in a carousel when a carousel shows.
     * @param position The last visible item position in the carousel.
     */
    public static void logCarouselLastVisibleItemPosition(int position) {
        RecordHistogram.recordCount1MHistogram(
                "Search.RelatedSearches.CarouselLastVisibleItemPosition", position);
    }

    /**
     * Logs weather the users scrolled the carousel or not.
     * @param scrolled Whether the user scrolled the carousel after chips were presented.
     */
    public static void logCarouselScrolled(boolean scrolled) {
        RecordHistogram.recordBooleanHistogram("Search.RelatedSearches.CarouselScrolled", scrolled);
    }

    /**
     * Logs weather the users scrolled and clicked the carousel.
     * @param scrolled Whether the user scrolled the carousel after chips were presented.
     * @param clicked Whether the user clicked any suggestion or not after they were presented.
     */
    public static void logCarouselScrollAndClickStatus(boolean scrolled, boolean clicked) {
        @ScrollAndClickStatus int scrollAndClickStatus;
        if (scrolled) {
            scrollAndClickStatus =
                    clicked
                            ? ScrollAndClickStatus.SCROLLED_CLICKED
                            : ScrollAndClickStatus.SCROLLED_NO_CLICK;
        } else {
            scrollAndClickStatus =
                    clicked
                            ? ScrollAndClickStatus.NO_SCROLL_CLICKED
                            : ScrollAndClickStatus.NO_SCROLL_NO_CLICK;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Search.RelatedSearches.CarouselScrollAndClick",
                scrollAndClickStatus,
                Permissions.NUM_ENTRIES);
    }
}
