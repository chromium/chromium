// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler.VoiceResult;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion.MatchClassification;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A search provider that processes and stores voice recognition results and makes them available
 * in the omnibox results. The voice results are added to the end of the omnibox results.
 */
class VoiceSuggestionProvider {
    private static final float CONFIDENCE_THRESHOLD_SHOW = 0.3f;
    private static final float CONFIDENCE_THRESHOLD_HIDE_ALTS = 0.8f;

    private final List<VoiceResult> mResults = new ArrayList<VoiceResult>();

    private final float mConfidenceThresholdShow;
    private final float mConfidenceThresholdHideAlts;

    /**
     * Creates an instance of a {@link VoiceSuggestionProvider} class.
     */
    public VoiceSuggestionProvider() {
        mConfidenceThresholdShow = CONFIDENCE_THRESHOLD_SHOW;
        mConfidenceThresholdHideAlts = CONFIDENCE_THRESHOLD_HIDE_ALTS;
    }

    /**
     * Creates an instance of a {@link VoiceSuggestionProvider} class.
     * @param confidenceThresholdShow The minimum confidence a result can have to be shown (does not
     *                                include the first result).  Confidence values are between
     *                                0.0 and 1.0.
     * @param confidenceThresholdHideAlts The maximum confidence the first result can have before it
     *                                    will no longer show alternates.  Confidence values are
     *                                    between 0.0 and 1.0.
     */
    @VisibleForTesting
    protected VoiceSuggestionProvider(
            float confidenceThresholdShow, float confidenceThresholdHideAlts) {
        mConfidenceThresholdShow = confidenceThresholdShow;
        mConfidenceThresholdHideAlts = confidenceThresholdHideAlts;
    }

    /**
     * Clears the current voice search results.
     */
    void clearVoiceSearchResults() {
        mResults.clear();
    }

    /**
     * @return The current voice search results.  This could be {@code null} if no results are
     *         currently present.
     */
    @VisibleForTesting
    List<VoiceResult> getResults() {
        return Collections.unmodifiableList(mResults);
    }

    /**
     * Sets the voice result options to be displayed in the autocomplete suggestion list.
     * @param results The list that contains the recognition results from a voice action.
     */
    void setVoiceResults(@Nullable List<VoiceResult> results) {
        clearVoiceSearchResults();
        if (results == null || results.size() == 0) return;
        mResults.addAll(results);
    }

    /**
     * Adds the currently stored voice recognition results to the current list of
     * {@link OmniboxSuggestion}s passed in.  Returns the new list to the caller.
     * @param suggestions The current list of {@link OmniboxSuggestion}s.
     * @param maxVoiceResults The maximum number of voice results that should be added.
     * @return A new list of {@link OmniboxSuggestion}s, which can include voice results.
     */
    List<OmniboxSuggestion> addVoiceSuggestions(
            List<OmniboxSuggestion> suggestions, int maxVoiceResults) {
        if (mResults.size() == 0 || maxVoiceResults == 0) return suggestions;

        List<OmniboxSuggestion> newSuggestions = new ArrayList<OmniboxSuggestion>();
        if (suggestions != null && suggestions.size() > 0) {
            newSuggestions.addAll(suggestions);
        }

        VoiceResult firstResult = mResults.get(0);
        addVoiceResultToOmniboxSuggestions(newSuggestions, firstResult, 0);

        final int suggestionLength = suggestions != null ? suggestions.size() : 0;
        if (firstResult.getConfidence() < mConfidenceThresholdHideAlts) {
            for (int i = 1; i < mResults.size()
                    && newSuggestions.size() < suggestionLength + maxVoiceResults;
                    ++i) {
                addVoiceResultToOmniboxSuggestions(
                        newSuggestions, mResults.get(i), mConfidenceThresholdShow);
            }
        }

        return newSuggestions;
    }

    private void addVoiceResultToOmniboxSuggestions(
            List<OmniboxSuggestion> suggestions, VoiceResult result, float confidenceThreshold) {
        if (doesVoiceResultHaveMatch(suggestions, result)) return;
        if (result.getConfidence() < confidenceThreshold && result.getConfidence() > 0) return;
        String voiceUrl =
                TemplateUrlServiceFactory.get().getUrlForVoiceSearchQuery(result.getMatch());
        List<MatchClassification> classifications = new ArrayList<>();
        classifications.add(new MatchClassification(0, MatchClassificationStyle.NONE));
        suggestions.add(new OmniboxSuggestion(OmniboxSuggestionType.VOICE_SUGGEST, true, 0, 1,
                result.getMatch(), classifications, null, classifications, null, null, voiceUrl,
                null, null, false, false));
    }

    private boolean doesVoiceResultHaveMatch(
            List<OmniboxSuggestion> suggestions, VoiceResult result) {
        for (OmniboxSuggestion suggestion : suggestions) {
            if (suggestion.getDisplayText().equals(result.getMatch())) return true;
        }

        return false;
    }
}
