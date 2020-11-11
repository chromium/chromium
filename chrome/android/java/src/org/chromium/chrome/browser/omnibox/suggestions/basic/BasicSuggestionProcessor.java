// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.content.Context;
import android.text.TextUtils;

import androidx.annotation.DrawableRes;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties.SuggestionIcon;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** A class that handles model and view creation for the basic omnibox suggestions. */
public class BasicSuggestionProcessor extends BaseSuggestionViewProcessor {
    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private final Supplier<LargeIconBridge> mIconBridgeSupplier;
    private final int mDesiredFaviconWidthPx;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     * @param editingTextProvider A means of accessing the text in the omnibox.
     */
    public BasicSuggestionProcessor(Context context, SuggestionHost suggestionHost,
            UrlBarEditingTextStateProvider editingTextProvider,
            Supplier<LargeIconBridge> iconBridgeSupplier) {
        super(context, suggestionHost);

        mDesiredFaviconWidthPx = getContext().getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);
        mUrlBarEditingTextProvider = editingTextProvider;
        mIconBridgeSupplier = iconBridgeSupplier;
    }

    @Override
    public boolean doesProcessSuggestion(OmniboxSuggestion suggestion, int position) {
        return true;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.DEFAULT;
    }

    @Override
    public PropertyModel createModel() {
        return new PropertyModel(SuggestionViewProperties.ALL_KEYS);
    }

    /**
     * Returns suggestion icon to be presented for specified omnibox suggestion.
     *
     * This method returns the stock icon type to be attached to the Suggestion.
     * Note that the stock icons do not include Favicon - Favicon is only declared
     * when we know we have a valid and large enough site favicon to present.
     */
    private @SuggestionIcon int getSuggestionIconType(OmniboxSuggestion suggestion) {
        if (suggestion.isSearchSuggestion()) {
            switch (suggestion.getType()) {
                case OmniboxSuggestionType.VOICE_SUGGEST:
                    return SuggestionIcon.VOICE;

                case OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED:
                case OmniboxSuggestionType.SEARCH_HISTORY:
                    return SuggestionIcon.HISTORY;

                default:
                    if (suggestion.getSubtypes().contains(/* SUBTYPE_TRENDS = */ 143)) {
                        return SuggestionIcon.TRENDS;
                    }
                    return SuggestionIcon.MAGNIFIER;
            }
        } else {
            if (suggestion.isStarred()) {
                return SuggestionIcon.BOOKMARK;
            } else {
                return SuggestionIcon.GLOBE;
            }
        }
    }

    private void updateSuggestionIcon(OmniboxSuggestion suggestion, PropertyModel model) {
        @SuggestionIcon
        int type = getSuggestionIconType(suggestion);
        @DrawableRes
        int icon = R.drawable.ic_suggestion_magnifier;

        switch (type) {
            case SuggestionIcon.BOOKMARK:
                icon = R.drawable.btn_star;
                break;

            case SuggestionIcon.HISTORY:
                icon = R.drawable.ic_history_googblue_24dp;
                break;

            case SuggestionIcon.GLOBE:
                icon = R.drawable.ic_globe_24dp;
                break;

            case SuggestionIcon.MAGNIFIER:
                icon = R.drawable.ic_suggestion_magnifier;
                break;

            case SuggestionIcon.VOICE:
                icon = R.drawable.btn_mic;
                break;

            case SuggestionIcon.TRENDS:
                icon = R.drawable.trending_up_black_24dp;
                break;

            default:
                // All other cases are invalid.
                assert false : "Suggestion type " + type + " is not valid.";
        }

        model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE, type);
        setSuggestionDrawableState(model,
                SuggestionDrawableState.Builder.forDrawableRes(getContext(), icon)
                        .setAllowTint(true)
                        .build());
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        super.populateModel(suggestion, model, position);
        final @OmniboxSuggestionType int suggestionType = suggestion.getType();
        final boolean isSearchSuggestion = suggestion.isSearchSuggestion();
        SuggestionSpannable textLine2 = null;
        boolean urlHighlighted = false;

        if (!isSearchSuggestion) {
            if (!suggestion.getUrl().isEmpty()) {
                SuggestionSpannable str = new SuggestionSpannable(suggestion.getDisplayText());
                urlHighlighted = applyHighlightToMatchRegions(
                        str, suggestion.getDisplayTextClassifications());
                textLine2 = str;
            }
        } else if (suggestionType == OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE) {
            textLine2 = new SuggestionSpannable(suggestion.getDescription());
        }

        final SuggestionSpannable textLine1 =
                getSuggestedQuery(suggestion, !isSearchSuggestion, !urlHighlighted);

        updateSuggestionIcon(suggestion, model);
        model.set(SuggestionViewProperties.IS_SEARCH_SUGGESTION, isSearchSuggestion);
        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT, textLine1);
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT, textLine2);
        fetchSuggestionFavicon(model, suggestion.getUrl(), mIconBridgeSupplier.get(), () -> {
            model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE, SuggestionIcon.FAVICON);
        });
        model.set(SuggestionViewProperties.ALLOW_WRAP_AROUND, isSearchSuggestion);

        if (!mUrlBarEditingTextProvider.getTextWithoutAutocomplete().trim().equalsIgnoreCase(
                    suggestion.getDisplayText())) {
            setTabSwitchOrRefineAction(model, suggestion, position);
        }
    }

    /**
     * Get the first line for a text based omnibox suggestion.
     * @param suggestion The item containing the suggestion data.
     * @param showDescriptionIfPresent Whether to show the description text of the suggestion if
     *                                 the item contains valid data.
     * @param shouldHighlight Whether the query should be highlighted.
     * @return The first line of text.
     */
    private SuggestionSpannable getSuggestedQuery(OmniboxSuggestion suggestion,
            boolean showDescriptionIfPresent, boolean shouldHighlight) {
        String suggestedQuery = null;
        List<OmniboxSuggestion.MatchClassification> classifications;
        if (showDescriptionIfPresent && !suggestion.getUrl().isEmpty()
                && !TextUtils.isEmpty(suggestion.getDescription())) {
            suggestedQuery = suggestion.getDescription();
            classifications = suggestion.getDescriptionClassifications();
        } else {
            suggestedQuery = suggestion.getDisplayText();
            classifications = suggestion.getDisplayTextClassifications();
        }
        if (suggestedQuery == null) {
            assert false : "Invalid suggestion sent with no displayable text";
            suggestedQuery = "";
            classifications = new ArrayList<OmniboxSuggestion.MatchClassification>();
            classifications.add(
                    new OmniboxSuggestion.MatchClassification(0, MatchClassificationStyle.NONE));
        }

        SuggestionSpannable str = new SuggestionSpannable(suggestedQuery);
        if (shouldHighlight) applyHighlightToMatchRegions(str, classifications);
        return str;
    }
}
