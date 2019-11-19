// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * A processor of omnibox suggestions. Implementers are provided the opportunity to analyze a
 * suggestion and create a custom model.
 */
public interface SuggestionProcessor {
    /**
     * @param suggestion The suggestion to process.
     * @return Whether this suggestion processor handles this type of suggestion.
     */
    boolean doesProcessSuggestion(OmniboxSuggestion suggestion);

    /**
     * @return The type of view the models created by this processor represent.
     */
    int getViewTypeId();

    /**
     * @see org.chromium.chrome.browser.omnibox.UrlFocusChangeListener#onUrlFocusChange(boolean)
     */
    void onUrlFocusChange(boolean hasFocus);

    /**
     * Signals that native initialization has completed.
     */
    void onNativeInitialized();

    /**
     * Create a model for a suggestion at the specified position.
     * @param suggestion The suggestion to create the model for.
     * @return A model for the suggestion.
     */
    PropertyModel createModelForSuggestion(OmniboxSuggestion suggestion);

    /**
     * Populate a model for the given suggestion.
     * @param suggestion The suggestion to populate the model for.
     * @param model The model to populate.
     * @param position The position of the suggestion in the list.
     */
    void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position);

    /**
     * Record histograms for presented suggestion.
     * Purpose of this function is bookkeeping of presented suggestions at the time user finishes
     * interacting with omnibox (whether navigating somewhere, turning off screen, leaving omnibox
     * or closing the app).
     * This call is invoked only on Processor responsible for managing specific omnibox suggestion
     * type.
     */
    void recordSuggestionPresented(OmniboxSuggestion suggestion, PropertyModel model);

    /**
     * Record histograms for used suggestion.
     * Invoked whenever user uses particular suggestion to navigate somewhere.
     * Only the processor responsible for managing specific suggestion receives this call.
     */
    void recordSuggestionUsed(OmniboxSuggestion suggestion, PropertyModel model);
}
