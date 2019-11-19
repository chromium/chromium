// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.support.v4.util.ObjectsCompat;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.components.omnibox.SuggestionAnswer;

import java.util.ArrayList;
import java.util.List;

/**
 * Container class with information about each omnibox suggestion item.
 */
@VisibleForTesting
public class OmniboxSuggestion {
    private static final String KEY_ZERO_SUGGEST_LIST_SIZE = "zero_suggest_list_size";
    private static final String KEY_PREFIX_ZERO_SUGGEST_URL = "zero_suggest_url";
    private static final String KEY_PREFIX_ZERO_SUGGEST_DISPLAY_TEST = "zero_suggest_display_text";
    private static final String KEY_PREFIX_ZERO_SUGGEST_DESCRIPTION = "zero_suggest_description";
    private static final String KEY_PREFIX_ZERO_SUGGEST_NATIVE_TYPE = "zero_suggest_native_type";
    private static final String KEY_PREFIX_ZERO_SUGGEST_IS_SEARCH_TYPE = "zero_suggest_is_search";
    private static final String KEY_PREFIX_ZERO_SUGGEST_ANSWER_TEXT = "zero_suggest_answer_text";
    // Deprecated:
    // private static final String KEY_PREFIX_ZERO_SUGGEST_ANSWER_TYPE = "zero_suggest_answer_type";
    private static final String KEY_PREFIX_ZERO_SUGGEST_IS_DELETABLE = "zero_suggest_is_deletable";
    private static final String KEY_PREFIX_ZERO_SUGGEST_IS_STARRED = "zero_suggest_is_starred";

    /**
     * Specifies the style of portions of the suggestion text.
     * <p>
     * ACMatchClassification (as defined in C++) further describes the fields and usage.
     */
    public static class MatchClassification {
        /**
         * The offset into the text where this classification begins.
         */
        public final int offset;

        /**
         * A bitfield that determines the style of this classification.
         * @see MatchClassificationStyle
         */
        public final int style;

        public MatchClassification(int offset, int style) {
            this.offset = offset;
            this.style = style;
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof MatchClassification)) return false;
            MatchClassification other = (MatchClassification) obj;
            return offset == other.offset && style == other.style;
        }
    }

    private final int mType;
    private final boolean mIsSearchType;
    private final String mDisplayText;
    private final List<MatchClassification> mDisplayTextClassifications;
    private final String mDescription;
    private final List<MatchClassification> mDescriptionClassifications;
    private final SuggestionAnswer mAnswer;
    private final String mFillIntoEdit;
    private final String mUrl;
    private final String mImageUrl;
    private final String mImageDominantColor;
    private final int mRelevance;
    private final int mTransition;
    private final boolean mIsStarred;
    private final boolean mIsDeletable;

    public OmniboxSuggestion(int nativeType, boolean isSearchType, int relevance, int transition,
            String displayText, List<MatchClassification> displayTextClassifications,
            String description, List<MatchClassification> descriptionClassifications,
            SuggestionAnswer answer, String fillIntoEdit, String url, String imageUrl,
            String imageDominantColor, boolean isStarred, boolean isDeletable) {
        mType = nativeType;
        mIsSearchType = isSearchType;
        mRelevance = relevance;
        mTransition = transition;
        mDisplayText = displayText;
        mDisplayTextClassifications = displayTextClassifications;
        mDescription = description;
        mDescriptionClassifications = descriptionClassifications;
        mAnswer = answer;
        mFillIntoEdit = TextUtils.isEmpty(fillIntoEdit) ? displayText : fillIntoEdit;
        mUrl = url;
        mImageUrl = imageUrl;
        mImageDominantColor = imageDominantColor;
        mIsStarred = isStarred;
        mIsDeletable = isDeletable;
    }

    public int getType() {
        return mType;
    }

    public int getTransition() {
        return mTransition;
    }

    public String getDisplayText() {
        return mDisplayText;
    }

    public List<MatchClassification> getDisplayTextClassifications() {
        return mDisplayTextClassifications;
    }

    public String getDescription() {
        return mDescription;
    }

    public List<MatchClassification> getDescriptionClassifications() {
        return mDescriptionClassifications;
    }

    public SuggestionAnswer getAnswer() {
        return mAnswer;
    }

    public boolean hasAnswer() {
        return mAnswer != null;
    }

    public String getFillIntoEdit() {
        return mFillIntoEdit;
    }

    public String getUrl() {
        return mUrl;
    }

    @Nullable
    public String getImageUrl() {
        return mImageUrl;
    }

    @Nullable
    public String getImageDominantColor() {
        return mImageDominantColor;
    }

    /**
     * @return Whether the suggestion is a URL.
     */
    public boolean isUrlSuggestion() {
        return !mIsSearchType;
    }

    /**
     * @return Whether this suggestion represents a starred/bookmarked URL.
     */
    public boolean isStarred() {
        return mIsStarred;
    }

    public boolean isDeletable() {
        return mIsDeletable;
    }

    /**
     * @return The relevance score of this suggestion.
     */
    public int getRelevance() {
        return mRelevance;
    }

    @Override
    public String toString() {
        return mType + " relevance=" + mRelevance + " \"" + mDisplayText + "\" -> " + mUrl;
    }

    @Override
    public int hashCode() {
        int hash = 37 * mType + mDisplayText.hashCode() + mFillIntoEdit.hashCode()
                + (mIsStarred ? 1 : 0) + (mIsDeletable ? 1 : 0);
        if (mAnswer != null) hash = hash + mAnswer.hashCode();
        return hash;
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof OmniboxSuggestion)) {
            return false;
        }

        OmniboxSuggestion suggestion = (OmniboxSuggestion) obj;
        return mType == suggestion.mType
                && TextUtils.equals(mFillIntoEdit, suggestion.mFillIntoEdit)
                && TextUtils.equals(mDisplayText, suggestion.mDisplayText)
                && ObjectsCompat.equals(
                        mDisplayTextClassifications, suggestion.mDisplayTextClassifications)
                && TextUtils.equals(mDescription, suggestion.mDescription)
                && ObjectsCompat.equals(
                        mDescriptionClassifications, suggestion.mDescriptionClassifications)
                && mIsStarred == suggestion.mIsStarred && mIsDeletable == suggestion.mIsDeletable
                && ObjectsCompat.equals(mAnswer, suggestion.mAnswer);
    }

    /**
     * Cache the given suggestion list in shared preferences.
     * @param suggestions Suggestions to be cached.
     */
    public static void cacheOmniboxSuggestionListForZeroSuggest(
            List<OmniboxSuggestion> suggestions) {
        Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.putInt(KEY_ZERO_SUGGEST_LIST_SIZE, suggestions.size()).apply();
        for (int i = 0; i < suggestions.size(); i++) {
            OmniboxSuggestion suggestion = suggestions.get(i);
            if (suggestion.mAnswer != null) continue;

            editor.putString(KEY_PREFIX_ZERO_SUGGEST_URL + i, suggestion.getUrl())
                    .putString(
                            KEY_PREFIX_ZERO_SUGGEST_DISPLAY_TEST + i, suggestion.getDisplayText())
                    .putString(KEY_PREFIX_ZERO_SUGGEST_DESCRIPTION + i, suggestion.getDescription())
                    .putInt(KEY_PREFIX_ZERO_SUGGEST_NATIVE_TYPE + i, suggestion.getType())
                    .putBoolean(KEY_PREFIX_ZERO_SUGGEST_IS_SEARCH_TYPE + i,
                            !suggestion.isUrlSuggestion())
                    .putBoolean(KEY_PREFIX_ZERO_SUGGEST_IS_DELETABLE + i, suggestion.mIsDeletable)
                    .putBoolean(KEY_PREFIX_ZERO_SUGGEST_IS_STARRED + i, suggestion.mIsStarred)
                    .apply();
        }
    }

    /**
     * @return The zero suggest result if they have been cached before.
     */
    public static List<OmniboxSuggestion> getCachedOmniboxSuggestionsForZeroSuggest() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        int size = prefs.getInt(KEY_ZERO_SUGGEST_LIST_SIZE, -1);
        List<OmniboxSuggestion> suggestions = null;
        if (size > 1) {
            suggestions = new ArrayList<>(size);
            List<MatchClassification> classifications = new ArrayList<>();
            classifications.add(new MatchClassification(0, MatchClassificationStyle.NONE));
            for (int i = 0; i < size; i++) {
                // TODO(tedchoc): Answers in suggest were previously cached, but that could lead to
                //                stale or misleading answers for cases like weather.  Ignore any
                //                previously cached answers for several releases while any previous
                //                results are cycled through.
                String answerText = prefs.getString(KEY_PREFIX_ZERO_SUGGEST_ANSWER_TEXT + i, "");
                if (!TextUtils.isEmpty(answerText)) continue;

                String url = prefs.getString(KEY_PREFIX_ZERO_SUGGEST_URL + i, "");
                String displayText = prefs.getString(KEY_PREFIX_ZERO_SUGGEST_DISPLAY_TEST + i, "");
                String description = prefs.getString(KEY_PREFIX_ZERO_SUGGEST_DESCRIPTION + i, "");
                int nativeType = prefs.getInt(KEY_PREFIX_ZERO_SUGGEST_NATIVE_TYPE + i, -1);
                boolean isSearchType =
                        prefs.getBoolean(KEY_PREFIX_ZERO_SUGGEST_IS_SEARCH_TYPE, true);
                boolean isStarred = prefs.getBoolean(KEY_PREFIX_ZERO_SUGGEST_IS_STARRED + i, false);
                boolean isDeletable =
                        prefs.getBoolean(KEY_PREFIX_ZERO_SUGGEST_IS_DELETABLE + i, false);
                OmniboxSuggestion suggestion = new OmniboxSuggestion(nativeType, !isSearchType, 0,
                        0, displayText, classifications, description, classifications, null, "",
                        url, null, null, isStarred, isDeletable);
                suggestions.add(suggestion);
            }
        }
        return suggestions;
    }
}
