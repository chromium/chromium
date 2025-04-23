// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.readaloud.PlaybackArgs.PlaybackMode;

import java.util.HashSet;
import java.util.Map;

/**
 * Interface providing access to ReadAloud page readability checking. Page can only be played if
 * it's readable, which is true if (among others) there's enough text to be played as audio.
 */
@NullMarked
public interface ReadAloudReadabilityHooks {
    /** Result of a readability check for a specific mode. */
    public static class ReadabilityResult {
        public final boolean readable;
        public final boolean supportsHighlighting;

        public ReadabilityResult(boolean readable, boolean supportsHighlighting) {
            this.readable = readable;
            this.supportsHighlighting = supportsHighlighting;
        }
    }

    /** Interface to receive result for readability check. */
    interface ReadabilityCallback {
        /**
         * Called if isPageReadable() succeeds.
         *
         * @param tab tab of a page to check
         * @param url url of the page to check
         * @param isReadable if page can be played
         * @param timepointsSupported whether timepoints are supported. Timepoints are used
         *     forword-by-word highlighting.
         */
        default void onSuccess(
                Tab tab, String url, boolean isReadable, boolean timepointsSupported) {}

        /**
         * Called if isPageReadable() succeeds. (To be removed once the overloaded function is
         * overridden in clank)
         *
         * @param url url of the page to check
         * @param isReadable if page can be played
         * @param timepointsSupported whether timepoints are supported. Timepoints are used
         *     forword-by-word highlighting.
         */
        default void onSuccess(String url, boolean isReadable, boolean timepointsSupported) {}

        /** Called if isPageReadable() fails. */
        void onFailure(String url, Throwable t);
    }

    interface ReadabilityPerModeCallback {
        /**
         * Called if isPageReadable() succeeds.
         *
         * <p>This mode contains per-mode readability information and backward compatibility is
         * maintained through the CLASSIC mode.
         */
        default void onSuccess(
                String url, Map<PlaybackMode, ReadabilityResult> readabilityPerMode) {}

        /** Indicates a failure in readability check. */
        default void onFailure(String url, Throwable t) {}
    }

    /** Returns true if ReadAloud feature is available. */
    boolean isEnabled();

    /**
     * Checks whether a given page is readable. (To be removed once the overloaded function is
     * overridden in clank)
     *
     * @param url url of a page to check
     * @param callback callback to get result
     */
    void isPageReadable(String url, ReadabilityCallback callback);

    default void isPageReadable(String url, ReadabilityPerModeCallback callback) {}

    /**
     * Checks whether a given page is readable.
     *
     * @param tab tab of a page to check
     * @param url url of a page to check
     * @param callback callback to get result
     */
    default void isPageReadable(Tab tab, String url, ReadabilityCallback callback) {}

    /**
     * Checks whether a given page is readable.
     *
     * @param tab tab of a page to check
     * @param url url of a page to check
     * @param translateLanguage Target language for translation, or null if the page is not
     *     translated.
     * @param callback callback to get result
     */
    default void isPageReadable(
            Tab tab,
            String url,
            @Nullable String translateLanguage,
            ReadabilityCallback callback) {}

    /**
     * Get the languages that are compatible with the voices.
     *
     * @return a hashset of compatible languages with the voices.
     */
    default HashSet<String> getCompatibleLanguages() {
        return new HashSet<String>();
    }
}
