// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.basic;

import android.content.Context;
import android.graphics.Bitmap;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.StyleSpan;
import android.util.Pair;
import android.util.TypedValue;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.favicon.LargeIconBridge;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties.SuggestionIcon;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties.SuggestionTextContainer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** A class that handles model and view creation for the basic omnibox suggestions. */
public class BasicSuggestionProcessor implements SuggestionProcessor {
    private final Context mContext;
    private final SuggestionHost mSuggestionHost;
    private final UrlBarEditingTextStateProvider mUrlBarEditingTextProvider;
    private LargeIconBridge mLargeIconBridge;
    private boolean mEnableSuggestionFavicons;
    private final int mDesiredFaviconWidthPx;

    /**
     * @param context An Android context.
     * @param suggestionHost A handle to the object using the suggestions.
     * @param editingTextProvider A means of accessing the text in the omnibox.
     */
    public BasicSuggestionProcessor(Context context, SuggestionHost suggestionHost,
            UrlBarEditingTextStateProvider editingTextProvider) {
        mContext = context;
        mDesiredFaviconWidthPx = mContext.getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_favicon_size);
        mSuggestionHost = suggestionHost;
        mUrlBarEditingTextProvider = editingTextProvider;
    }

    @Override
    public boolean doesProcessSuggestion(OmniboxSuggestion suggestion) {
        return true;
    }

    @Override
    public int getViewTypeId() {
        return OmniboxSuggestionUiType.DEFAULT;
    }

    @Override
    public PropertyModel createModelForSuggestion(OmniboxSuggestion suggestion) {
        return new PropertyModel(SuggestionViewProperties.ALL_KEYS);
    }

    @Override
    public void populateModel(OmniboxSuggestion suggestion, PropertyModel model, int position) {
        model.set(SuggestionViewProperties.DELEGATE,
                mSuggestionHost.createSuggestionViewDelegate(suggestion, position));

        setStateForSuggestion(model, suggestion);
    }

    @Override
    public void onUrlFocusChange(boolean hasFocus) {}

    @Override
    public void recordSuggestionPresented(OmniboxSuggestion suggestion, PropertyModel model) {
        RecordHistogram.recordEnumeratedHistogram("Omnibox.IconOrFaviconShown",
                model.get(SuggestionViewProperties.SUGGESTION_ICON_TYPE),
                SuggestionIcon.TOTAL_COUNT);
    }

    @Override
    public void recordSuggestionUsed(OmniboxSuggestion suggestion, PropertyModel model) {
        RecordHistogram.recordEnumeratedHistogram("Omnibox.SuggestionUsed.IconOrFaviconType",
                model.get(SuggestionViewProperties.SUGGESTION_ICON_TYPE),
                SuggestionIcon.TOTAL_COUNT);
    }

    /**
     * Signals that native initialization has completed.
     */
    @Override
    public void onNativeInitialized() {
        // Experiment: controls presence of certain answer icon types.
        mEnableSuggestionFavicons =
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_SHOW_SUGGESTION_FAVICONS);
    }

    /**
     * Updates the profile used for extracting website favicons.
     * @param profile The profile to be used.
     */
    public void setProfile(Profile profile) {
        if (mEnableSuggestionFavicons) {
            mLargeIconBridge = new LargeIconBridge(profile);
        }
    }

    /**
     * Returns suggestion icon to be presented for specified omnibox suggestion.
     *
     * This method returns the stock icon type to be attached to the Suggestion.
     * Note that the stock icons do not include Favicon - Favicon is only declared
     * when we know we have a valid and large enough site favicon to present.
     */
    @VisibleForTesting
    public @SuggestionIcon int getSuggestionIconType(OmniboxSuggestion suggestion) {
        if (suggestion.isUrlSuggestion()) {
            if (suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_TEXT
                    || suggestion.getType() == OmniboxSuggestionType.CLIPBOARD_IMAGE) {
                return SuggestionIcon.MAGNIFIER;
            } else if (suggestion.isStarred()) {
                return SuggestionIcon.BOOKMARK;
            } else {
                return SuggestionIcon.GLOBE;
            }
        } else /* Search suggestion */ {
            switch (suggestion.getType()) {
                case OmniboxSuggestionType.VOICE_SUGGEST:
                    return SuggestionIcon.VOICE;

                case OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED:
                case OmniboxSuggestionType.SEARCH_HISTORY:
                    return SuggestionIcon.HISTORY;

                default:
                    return SuggestionIcon.MAGNIFIER;
            }
        }
    }

    private void setStateForSuggestion(PropertyModel model, OmniboxSuggestion suggestion) {
        int suggestionType = suggestion.getType();
        Spannable textLine1;

        Spannable textLine2;
        int textLine2Color = 0;
        int textLine2Direction = View.TEXT_DIRECTION_INHERIT;

        if (suggestion.isUrlSuggestion()) {
            boolean urlHighlighted = false;
            if (!TextUtils.isEmpty(suggestion.getUrl())) {
                Spannable str = SpannableString.valueOf(suggestion.getDisplayText());
                urlHighlighted = applyHighlightToMatchRegions(
                        str, suggestion.getDisplayTextClassifications());
                textLine2 = str;
                textLine2Color = ApiCompatibilityUtils.getColor(mContext.getResources(),
                        model.get(SuggestionCommonProperties.USE_DARK_COLORS)
                                ? R.color.suggestion_url_dark_modern
                                : R.color.suggestion_url_light_modern);

                if (suggestionType == OmniboxSuggestionType.CLIPBOARD_TEXT) {
                    textLine2Direction = View.TEXT_DIRECTION_INHERIT;
                } else {
                    textLine2Direction = View.TEXT_DIRECTION_LTR;
                }
            } else {
                textLine2 = null;
            }
            textLine1 = getSuggestedQuery(suggestion, true, !urlHighlighted);
        } else {
            textLine1 = getSuggestedQuery(suggestion, false, true);
            if ((suggestionType == OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY)
                    || (suggestionType == OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE)) {
                textLine2 = SpannableString.valueOf(suggestion.getDescription());
                textLine2Color = ApiCompatibilityUtils.getColor(mContext.getResources(),
                        model.get(SuggestionCommonProperties.USE_DARK_COLORS)
                                ? R.color.default_text_color_dark
                                : R.color.default_text_color_light);
                textLine2Direction = View.TEXT_DIRECTION_INHERIT;
            } else {
                textLine2 = null;
            }
        }

        model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE, getSuggestionIconType(suggestion));
        model.set(SuggestionViewProperties.SUGGESTION_ICON_BITMAP, null);

        model.set(
                SuggestionViewProperties.TEXT_LINE_1_TEXT, new SuggestionTextContainer(textLine1));
        model.set(SuggestionViewProperties.TEXT_LINE_1_SIZING,
                Pair.create(TypedValue.COMPLEX_UNIT_PX,
                        (int) mContext.getResources().getDimension(
                                org.chromium.chrome.R.dimen
                                        .omnibox_suggestion_first_line_text_size)));

        model.set(
                SuggestionViewProperties.TEXT_LINE_2_TEXT, new SuggestionTextContainer(textLine2));
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT_COLOR, textLine2Color);
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT_DIRECTION, textLine2Direction);
        model.set(SuggestionViewProperties.TEXT_LINE_2_SIZING,
                Pair.create(TypedValue.COMPLEX_UNIT_PX,
                        (int) mContext.getResources().getDimension(
                                org.chromium.chrome.R.dimen
                                        .omnibox_suggestion_second_line_text_size)));
        model.set(SuggestionViewProperties.TEXT_LINE_1_MAX_LINES, 1);
        model.set(SuggestionViewProperties.TEXT_LINE_2_MAX_LINES, 1);

        // Include site favicon if we are presenting URL and have favicon available.
        // TODO(gangwu): Create a saparate processor for clipboard suggestions.
        if (mLargeIconBridge != null && suggestion.getUrl() != null
                && suggestion.getType() != OmniboxSuggestionType.CLIPBOARD_TEXT) {
            mLargeIconBridge.getLargeIconForUrl(suggestion.getUrl(), mDesiredFaviconWidthPx,
                    (Bitmap icon, int fallbackColor, boolean isFallbackColorDefault,
                            int iconType) -> {
                        if (icon != null) {
                            model.set(SuggestionViewProperties.SUGGESTION_ICON_BITMAP, icon);
                            model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE,
                                    SuggestionIcon.FAVICON);
                        }
                    });
        }

        boolean isRefinable =
                !(mUrlBarEditingTextProvider.getTextWithoutAutocomplete().trim().equalsIgnoreCase(
                          suggestion.getDisplayText())
                        || suggestionType == OmniboxSuggestionType.CLIPBOARD_TEXT
                        || suggestionType == OmniboxSuggestionType.CLIPBOARD_URL
                        || suggestionType == OmniboxSuggestionType.CLIPBOARD_IMAGE);
        model.set(SuggestionViewProperties.REFINABLE, isRefinable);
    }

    /**
     * Get the first line for a text based omnibox suggestion.
     * @param suggestion The item containing the suggestion data.
     * @param showDescriptionIfPresent Whether to show the description text of the suggestion if
     *                                 the item contains valid data.
     * @param shouldHighlight Whether the query should be highlighted.
     * @return The first line of text.
     */
    private Spannable getSuggestedQuery(OmniboxSuggestion suggestion,
            boolean showDescriptionIfPresent, boolean shouldHighlight) {
        String userQuery = mUrlBarEditingTextProvider.getTextWithoutAutocomplete();
        String suggestedQuery = null;
        List<OmniboxSuggestion.MatchClassification> classifications;
        if (showDescriptionIfPresent && !TextUtils.isEmpty(suggestion.getUrl())
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

        if (suggestion.getType() == OmniboxSuggestionType.SEARCH_SUGGEST_TAIL) {
            String fillIntoEdit = suggestion.getFillIntoEdit();
            final String ellipsisPrefix = "\u2026 ";
            suggestedQuery = ellipsisPrefix + suggestedQuery;
            // Offset the match classifications by the length of the ellipsis prefix to ensure
            // the highlighting remains correct.
            for (int i = 0; i < classifications.size(); i++) {
                classifications.set(i,
                        new OmniboxSuggestion.MatchClassification(
                                classifications.get(i).offset + ellipsisPrefix.length(),
                                classifications.get(i).style));
            }
            classifications.add(
                    0, new OmniboxSuggestion.MatchClassification(0, MatchClassificationStyle.NONE));
        }

        Spannable str = SpannableString.valueOf(suggestedQuery);
        if (shouldHighlight) applyHighlightToMatchRegions(str, classifications);
        return str;
    }

    private static boolean applyHighlightToMatchRegions(
            Spannable str, List<OmniboxSuggestion.MatchClassification> classifications) {
        boolean hasMatch = false;
        for (int i = 0; i < classifications.size(); i++) {
            OmniboxSuggestion.MatchClassification classification = classifications.get(i);
            if ((classification.style & MatchClassificationStyle.MATCH)
                    == MatchClassificationStyle.MATCH) {
                int matchStartIndex = classification.offset;
                int matchEndIndex;
                if (i == classifications.size() - 1) {
                    matchEndIndex = str.length();
                } else {
                    matchEndIndex = classifications.get(i + 1).offset;
                }
                matchStartIndex = Math.min(matchStartIndex, str.length());
                matchEndIndex = Math.min(matchEndIndex, str.length());

                hasMatch = true;
                // Bold the part of the URL that matches the user query.
                str.setSpan(new StyleSpan(android.graphics.Typeface.BOLD), matchStartIndex,
                        matchEndIndex, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
            }
        }
        return hasMatch;
    }
}
