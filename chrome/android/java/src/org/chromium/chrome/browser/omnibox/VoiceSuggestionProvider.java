// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.os.Bundle;
import android.speech.RecognizerIntent;

import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion.MatchClassification;
import org.chromium.chrome.browser.search_engines.TemplateUrlService;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A search provider that processes and stores voice recognition results and makes them available
 * in the omnibox results. The voice results are added to the end of the omnibox results.
 */
public class VoiceSuggestionProvider {
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
    protected VoiceSuggestionProvider(float confidenceThresholdShow,
            float confidenceThresholdHideAlts) {
        mConfidenceThresholdShow = confidenceThresholdShow;
        mConfidenceThresholdHideAlts = confidenceThresholdHideAlts;
    }

    /**
     * Clears the current voice search results.
     */
    public void clearVoiceSearchResults() {
        mResults.clear();
    }

    /**
     * @return The current voice search results.  This could be {@code null} if no results are
     *         currently present.
     */
    public List<VoiceResult> getResults() {
        return Collections.unmodifiableList(mResults);
    }

    /**
     * Takes and processes the results from a recognition action. It parses the confidence and
     * string values and stores the processed results here so they are made available to the
     * {@link AutocompleteController} and show up in the omnibox results. This method does not
     * reorder the voice results that come back from the recognizer.
     * @param extras The {@link Bundle} that contains the recognition results from a
     *               {@link RecognizerIntent#ACTION_RECOGNIZE_SPEECH} action.
     */
    public void setVoiceResultsFromIntentBundle(Bundle extras) {
        clearVoiceSearchResults();

        if (extras == null) return;

        ArrayList<String> strings = extras.getStringArrayList(
                RecognizerIntent.EXTRA_RESULTS);
        float[] confidences = extras.getFloatArray(
                RecognizerIntent.EXTRA_CONFIDENCE_SCORES);

        if (strings == null || confidences == null) return;

        assert (strings.size() == confidences.length);
        if (strings.size() != confidences.length) return;

        for (int i = 0; i < strings.size(); ++i) {
            // Remove any spaces in the voice search match when determining whether it
            // appears to be a URL. This is to prevent cases like (
            // "tech crunch.com" and "www. engadget .com" from not appearing like URLs)
            // from not navigating to the URL.
            // If the string appears to be a URL, then use it instead of the string returned from
            // the voice engine.
            String culledString = strings.get(i).replaceAll(" ", "");
            String url = AutocompleteController.nativeQualifyPartialURLQuery(culledString);
            mResults.add(new VoiceResult(
                    url == null ? strings.get(i) : culledString, confidences[i]));
        }
    }

    /**
     * Adds the currently stored voice recognition results to the current list of
     * {@link OmniboxSuggestion}s passed in.  Returns the new list to the caller.
     * @param suggestions The current list of {@link OmniboxSuggestion}s.
     * @param maxVoiceResults The maximum number of voice results that should be added.
     * @return A new list of {@link OmniboxSuggestion}s, which can include voice results.
     */
    public List<OmniboxSuggestion> addVoiceSuggestions(
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
                    && newSuggestions.size() < suggestionLength + maxVoiceResults; ++i) {
                addVoiceResultToOmniboxSuggestions(newSuggestions, mResults.get(i),
                        mConfidenceThresholdShow);
            }
        }

        return newSuggestions;
    }

    private void addVoiceResultToOmniboxSuggestions(List<OmniboxSuggestion> suggestions,
            VoiceResult result, float confidenceThreshold) {
        if (doesVoiceResultHaveMatch(suggestions, result)) return;
        if (result.getConfidence() < confidenceThreshold && result.getConfidence() > 0) return;
        String voiceUrl = TemplateUrlService.getInstance().getUrlForVoiceSearchQuery(
                result.getMatch());
        List<MatchClassification> classifications = new ArrayList<>();
        classifications.add(new MatchClassification(0, MatchClassificationStyle.NONE));
        suggestions.add(new OmniboxSuggestion(
                OmniboxSuggestionType.VOICE_SUGGEST,
                true,
                0,
                1,
                result.getMatch(),
                classifications,
                null,
                classifications,
                null,
                null,
                null,
                voiceUrl,
                false,
                false));
    }

    private boolean doesVoiceResultHaveMatch(List<OmniboxSuggestion> suggestions,
            VoiceResult result) {
        for (OmniboxSuggestion suggestion : suggestions) {
            if (suggestion.getDisplayText().equals(result.getMatch())) return true;
        }

        return false;
    }

    /**
     * A storage class that holds voice recognition string matches and confidence scores.
     */
    public static class VoiceResult {
        private final String mMatch;
        private final float mConfidence;

        /**
         * Creates an instance of a VoiceResult.
         * @param match The text match from the voice recognition.
         * @param confidence The confidence value of the recognition that should go from 0.0 to 1.0.
         */
        public VoiceResult(String match, float confidence) {
            mMatch = match;
            mConfidence = confidence;
        }

        /**
         * @return The text match from the voice recognition.
         */
        public String getMatch() {
            return mMatch;
        }

        /**
         * @return The confidence value of the recognition that should go from 0.0 to 1.0.
         */
        public float getConfidence() {
            return mConfidence;
        }
    }
}
