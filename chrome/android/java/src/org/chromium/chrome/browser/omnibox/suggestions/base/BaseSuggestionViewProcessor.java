// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewDelegate;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A class that handles base properties and model for most suggestions.
 */
public abstract class BaseSuggestionViewProcessor implements SuggestionProcessor {
    private final Context mContext;
    private final SuggestionHost mSuggestionHost;

    /**
     * @param host A handle to the object using the suggestions.
     */
    public BaseSuggestionViewProcessor(Context context, SuggestionHost host) {
        mContext = context;
        mSuggestionHost = host;
    }

    /**
     * @return whether suggestion can be refined and a refine icon should be shown.
     */
    protected boolean canRefine(OmniboxSuggestion suggestion) {
        return true;
    }

    /**
     * Specify SuggestionDrawableState for suggestion decoration.
     *
     * @param decoration SuggestionDrawableState object defining decoration for the suggestion.
     */
    protected void setSuggestionDrawableState(
            PropertyModel model, SuggestionDrawableState decoration) {
        model.set(BaseSuggestionViewProperties.ICON, decoration);
    }

    /**
     * Specify SuggestionDrawableState for action button.
     *
     * @param decoration SuggestionDrawableState object defining decoration for the action button.
     */
    protected void setActionDrawableState(PropertyModel model, SuggestionDrawableState decoration) {
        model.set(BaseSuggestionViewProperties.ACTION_ICON, decoration);
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        SuggestionViewDelegate delegate =
                mSuggestionHost.createSuggestionViewDelegate(suggestion, position);

        model.set(BaseSuggestionViewProperties.SUGGESTION_DELEGATE, delegate);

        if (canRefine(suggestion)) {
            setActionDrawableState(model,
                    SuggestionDrawableState.Builder
                            .forDrawableRes(mContext, R.drawable.btn_suggestion_refine)
                            .setLarge(true)
                            .setAllowTint(true)
                            .build());
        } else {
            setActionDrawableState(model, null);
        }
    }
}
