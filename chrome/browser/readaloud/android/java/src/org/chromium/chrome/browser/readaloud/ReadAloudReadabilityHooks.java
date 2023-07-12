// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

/**
 * Interface providing access to ReadAloud page readability checking.
 * Page can only be played if it's readable, which is true if (among others) there's enough text to
 * be played as audio.
 */
public interface ReadAloudReadabilityHooks {
    /** Interface to receive result for readability check. */
    interface ReadabilityCallback {
        /**
         * Called if isPageReadable() succeeds.
         *
         * @param url url of the page to check
         * @param isReadable          if page can be played
         * @param timepointsSupported whether timepoints are supported. Timepoints are
         *                            used forword-by-word highlighting.
         */
        void onSuccess(String url, boolean isReadable, boolean timepointsSupported);
        /** Called if isPageReadable() fails. */
        void onFailure(String url, Throwable t);
    }

    /** Returns true if ReadAloud feature is available. */
    boolean isEnabled();

    /**
     * Checks whether a given page is readable.
     * @param url url of a page to check
     * @param callback callback to get result
     */
    void isPageReadable(String url, ReadabilityCallback callback);
}
