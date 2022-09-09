// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.annotation.Nullable;

/**
 * Provides an interface for detecting and forcing translation on a Contextual Search Request.
 * When translation is forced, the request has additional parameters that force a one-box for the
 * supplied source and destination languages.
 * Methods support detecting underlying translation signals and forcing translation on a request
 * from a given source language, or forcing translation on a request using an auto-detection for
 * to determine whether the source and destination are different.
 */
public interface ContextualSearchTranslation {
    /**
     * Force translation from the given language for the given search request.
     * Also log whenever conditions are right to translate.
     * @param searchRequest The search request to force translation upon.
     * @param sourceLanguage The language to translate from, or an empty string if not known.
     * @param isTapSelection Whether the selection was established with a Tap gesture.
     */
    void forceTranslateIfNeeded(
            ContextualSearchRequest searchRequest, String sourceLanguage, boolean isTapSelection);

    /**
     * Force auto-detect translation for the current search request.  The language to translate from
     * will be auto-detected, and some overtriggering is likely but not harmful (because a translate
     * onebox is suppressed when the from/to languages match.
     * @param searchRequest The search request to force translation upon.
     */
    void forceAutoDetectTranslateUnlessDisabled(ContextualSearchRequest searchRequest);

    /** @return Whether the given {@code sourceLanguage} needs translation for the current user. */
    boolean needsTranslation(@Nullable String sourceLanguage);

    /**
     * @return The best target language based on what the Translate Service knows about the user.
     */
    String getTranslateServiceTargetLanguage();

    /**
     * @return The ordered list of languages that the user can read fluently.
     */
    String getTranslateServiceFluentLanguages();
}
