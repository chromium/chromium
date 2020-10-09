// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.util.ObjectsCompat;

import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.components.omnibox.SuggestionAnswer;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Set;

/**
 * Container class with information about each omnibox suggestion item.
 */
public class OmniboxSuggestion {
    public static final int INVALID_GROUP = -1;
    public static final int INVALID_TYPE = -1;

    /**
     * Specifies an individual tile for TILE_NAVSUGGEST suggestions.
     */
    public static class NavsuggestTile {
        /**
         * Title of the website the tile points to.
         */
        public final String title;
        /**
         * URL of the website the tile points to.
         */
        public final GURL url;

        public NavsuggestTile(String title, GURL url) {
            this.title = title;
            this.url = url;
        }
    }

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
    private final @NonNull Set<Integer> mSubtypes;
    private final boolean mIsSearchType;
    private final String mDisplayText;
    private final List<MatchClassification> mDisplayTextClassifications;
    private final String mDescription;
    private final List<MatchClassification> mDescriptionClassifications;
    private final SuggestionAnswer mAnswer;
    private final String mFillIntoEdit;
    private final GURL mUrl;
    private final GURL mImageUrl;
    private final String mImageDominantColor;
    private final int mRelevance;
    private final int mTransition;
    private final boolean mIsStarred;
    private final boolean mIsDeletable;
    private final String mPostContentType;
    private final byte[] mPostData;
    private final int mGroupId;
    private final List<QueryTile> mQueryTiles;
    private final byte[] mClipboardImageData;
    private final boolean mHasTabMatch;
    private final @Nullable List<NavsuggestTile> mNavsuggestTiles;

    public OmniboxSuggestion(int nativeType, Set<Integer> subtypes, boolean isSearchType,
            int relevance, int transition, String displayText,
            List<MatchClassification> displayTextClassifications, String description,
            List<MatchClassification> descriptionClassifications, SuggestionAnswer answer,
            String fillIntoEdit, GURL url, GURL imageUrl, String imageDominantColor,
            boolean isStarred, boolean isDeletable, String postContentType, byte[] postData,
            int groupId, List<QueryTile> queryTiles, byte[] clipboardImageData, boolean hasTabMatch,
            List<NavsuggestTile> navsuggestTiles) {
        if (subtypes == null) {
            subtypes = Collections.emptySet();
        }
        mType = nativeType;
        mSubtypes = subtypes;
        mIsSearchType = isSearchType;
        mRelevance = relevance;
        mTransition = transition;
        mDisplayText = displayText;
        mDisplayTextClassifications = displayTextClassifications;
        mDescription = description;
        mDescriptionClassifications = descriptionClassifications;
        mAnswer = answer;
        mFillIntoEdit = TextUtils.isEmpty(fillIntoEdit) ? displayText : fillIntoEdit;
        assert url != null;
        mUrl = url;
        assert imageUrl != null;
        mImageUrl = imageUrl;
        mImageDominantColor = imageDominantColor;
        mIsStarred = isStarred;
        mIsDeletable = isDeletable;
        mPostContentType = postContentType;
        mPostData = postData;
        mGroupId = groupId;
        mQueryTiles = queryTiles;
        mClipboardImageData = clipboardImageData;
        mHasTabMatch = hasTabMatch;
        mNavsuggestTiles = navsuggestTiles;
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

    public GURL getUrl() {
        return mUrl;
    }

    public GURL getImageUrl() {
        return mImageUrl;
    }

    @Nullable
    public String getImageDominantColor() {
        return mImageDominantColor;
    }

    /**
     * @return Whether the suggestion is a search suggestion.
     */
    public boolean isSearchSuggestion() {
        return mIsSearchType;
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

    public String getPostContentType() {
        return mPostContentType;
    }

    public List<QueryTile> getQueryTiles() {
        return mQueryTiles;
    }

    public byte[] getPostData() {
        return mPostData;
    }

    public boolean hasTabMatch() {
        return mHasTabMatch;
    }

    /**
     * @return The image data for the image clipbaord suggestion. This data has already been
     *         validated in C++ and is safe to use in the browser process.
     */
    @Nullable
    public byte[] getClipboardImageData() {
        return mClipboardImageData;
    }

    /**
     * @return The relevance score of this suggestion.
     */
    public int getRelevance() {
        return mRelevance;
    }

    /**
     * @return Set of suggestion subtypes.
     */
    public @NonNull Set<Integer> getSubtypes() {
        return mSubtypes;
    }

    @Override
    public int hashCode() {
        final int displayTextHash = mDisplayText != null ? mDisplayText.hashCode() : 0;
        final int fillIntoEditHash = mFillIntoEdit != null ? mFillIntoEdit.hashCode() : 0;
        int hash = 37 * mType + 2017 * displayTextHash + 1901 * fillIntoEditHash
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
        return mType == suggestion.mType && ObjectsCompat.equals(mSubtypes, suggestion.mSubtypes)
                && TextUtils.equals(mFillIntoEdit, suggestion.mFillIntoEdit)
                && TextUtils.equals(mDisplayText, suggestion.mDisplayText)
                && ObjectsCompat.equals(
                        mDisplayTextClassifications, suggestion.mDisplayTextClassifications)
                && TextUtils.equals(mDescription, suggestion.mDescription)
                && ObjectsCompat.equals(
                        mDescriptionClassifications, suggestion.mDescriptionClassifications)
                && mIsStarred == suggestion.mIsStarred && mIsDeletable == suggestion.mIsDeletable
                && mRelevance == suggestion.mRelevance
                && ObjectsCompat.equals(mAnswer, suggestion.mAnswer)
                && TextUtils.equals(mPostContentType, suggestion.mPostContentType)
                && Arrays.equals(mPostData, suggestion.mPostData) && mGroupId == suggestion.mGroupId
                && ObjectsCompat.equals(mQueryTiles, suggestion.mQueryTiles);
    }

    /**
     * @return ID of the group this suggestion is associated with, or null, if the suggestion is
     *         not associated with any group, or INVALID_GROUP if suggestion is not associated with
     *         any group.
     */
    public int getGroupId() {
        return mGroupId;
    }

    /**
     * @return List of tiles for TILE_NAVSUGGEST suggestion.
     */
    public @Nullable List<NavsuggestTile> getNavsuggestTiles() {
        return mNavsuggestTiles;
    }

    @Override
    public String toString() {
        List<String> pieces = Arrays.asList("mType=" + mType, "mSubtypes=" + mSubtypes.toString(),
                "mIsSearchType=" + mIsSearchType, "mDisplayText=" + mDisplayText,
                "mDescription=" + mDescription, "mFillIntoEdit=" + mFillIntoEdit, "mUrl=" + mUrl,
                "mImageUrl=" + mImageUrl, "mImageDominatColor=" + mImageDominantColor,
                "mRelevance=" + mRelevance, "mTransition=" + mTransition,
                "mIsStarred=" + mIsStarred, "mIsDeletable=" + mIsDeletable,
                "mPostContentType=" + mPostContentType, "mPostData=" + Arrays.toString(mPostData),
                "mGroupId=" + mGroupId,
                "mDisplayTextClassifications=" + mDisplayTextClassifications,
                "mDescriptionClassifications=" + mDescriptionClassifications, "mAnswer=" + mAnswer);
        return pieces.toString();
    }
}
