// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.collection.ArraySet;

import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.components.omnibox.SuggestionAnswer;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Utility class for all omnibox suggestions related tests that aids constructing of Omnibox
 * Suggestions.
 */
public class OmniboxSuggestionBuilderForTest {
    // Fields below directly represent fields used in OmniboxSuggestion.java.
    private @OmniboxSuggestionType int mType;
    private Set<Integer> mSubtypes;
    private boolean mIsSearchType;
    private String mDisplayText;
    private List<OmniboxSuggestion.MatchClassification> mDisplayTextClassifications;
    private String mDescription;
    private List<OmniboxSuggestion.MatchClassification> mDescriptionClassifications;
    private SuggestionAnswer mAnswer;
    private String mFillIntoEdit;
    private GURL mUrl;
    private GURL mImageUrl;
    private String mImageDominantColor;
    private int mRelevance;
    private int mTransition;
    private boolean mIsStarred;
    private boolean mIsDeletable;
    private String mPostContentType;
    private byte[] mPostData;
    private int mGroupId;
    private List<QueryTile> mQueryTiles;
    private byte[] mClipboardImageData;
    private boolean mHasTabMatch;
    private List<OmniboxSuggestion.NavsuggestTile> mNavsuggestTiles;

    /**
     * Create a suggestion builder for a search suggestion.
     *
     * @return Omnibox suggestion builder that can be further refined by the user.
     */
    public static OmniboxSuggestionBuilderForTest searchWithType(@OmniboxSuggestionType int type) {
        return new OmniboxSuggestionBuilderForTest(type)
                .setIsSearch(true)
                .setDisplayText("Dummy Suggestion")
                .setDescription("Dummy Description")
                .setUrl(new GURL("http://dummy-website.com/test"));
    }

    public OmniboxSuggestionBuilderForTest(@OmniboxSuggestionType int type) {
        mType = type;
        mSubtypes = new ArraySet<>();
        mDisplayTextClassifications = new ArrayList<>();
        mDescriptionClassifications = new ArrayList<>();
        mUrl = GURL.emptyGURL();
        mImageUrl = GURL.emptyGURL();
        mGroupId = OmniboxSuggestion.INVALID_GROUP;

        mDisplayTextClassifications.add(
                new OmniboxSuggestion.MatchClassification(0, MatchClassificationStyle.NONE));
        mDescriptionClassifications.add(
                new OmniboxSuggestion.MatchClassification(0, MatchClassificationStyle.NONE));
    }

    public OmniboxSuggestionBuilderForTest() {
        this(OmniboxSuggestion.INVALID_TYPE);
    }

    /**
     * Construct OmniboxSuggestion from user set parameters.
     * Default/fallback values for not explicitly initialized fields are supplied by the builder.
     *
     * @return New OmniboxSuggestion.
     */
    public OmniboxSuggestion build() {
        return new OmniboxSuggestion(mType, mSubtypes, mIsSearchType, mRelevance, mTransition,
                mDisplayText, mDisplayTextClassifications, mDescription,
                mDescriptionClassifications, mAnswer, mFillIntoEdit, mUrl, mImageUrl,
                mImageDominantColor, mIsStarred, mIsDeletable, mPostContentType, mPostData,
                mGroupId, mQueryTiles, mClipboardImageData, mHasTabMatch, mNavsuggestTiles);
    }

    /**
     * @param text Display text to be used with the built suggestion.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setDisplayText(String text) {
        mDisplayText = text;
        return this;
    }

    /**
     * @param text Description text to be used with the built suggestion.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setDescription(String text) {
        mDescription = text;
        return this;
    }

    /**
     * @param id Group Id for newly built suggestion.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setGroupId(int id) {
        mGroupId = id;
        return this;
    }

    /**
     * @param type Post content type to set for this suggestion.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setPostContentType(String type) {
        mPostContentType = type;
        return this;
    }

    /**
     * @param data Post data to set for this suggestion.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setPostData(byte[] data) {
        mPostData = data;
        return this;
    }

    /**
     * @param url URL for the built suggestion.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setUrl(GURL url) {
        mUrl = url;
        return this;
    }

    /**
     * @param url Image URL for the built suggestion.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setImageUrl(GURL url) {
        mImageUrl = url;
        return this;
    }

    /**
     * @param color Image dominant color to set for built suggestion.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setImageDominantColor(String color) {
        mImageDominantColor = color;
        return this;
    }

    /**
     * @param isSearch Whether built suggestion is a search suggestion.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setIsSearch(boolean isSearch) {
        mIsSearchType = isSearch;
        return this;
    }

    /**
     * @param isStarred Whether built suggestion is bookmarked.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setIsStarred(boolean isStarred) {
        mIsStarred = isStarred;
        return this;
    }

    /**
     * @param answer The answer in the Omnibox suggestion.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setAnswer(SuggestionAnswer answer) {
        mAnswer = answer;
        return this;
    }

    /**
     * @param clipboardImageData Image data to set for this suggestion.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setClipboardImageData(byte[] clipboardImageData) {
        mClipboardImageData = clipboardImageData;
        return this;
    }

    /**
     * @param hasTabMatch Whether built suggestion has tab match.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setHasTabMatch(boolean hasTabMatch) {
        mHasTabMatch = hasTabMatch;
        return this;
    }

    /**
     * @param relevance Relevance score for newly constructed suggestion.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setRelevance(int relevance) {
        mRelevance = relevance;
        return this;
    }

    /**
     * @param type Suggestion type.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest setType(@OmniboxSuggestionType int type) {
        mType = type;
        return this;
    }

    /**
     * @param subtype Suggestion subtype.
     * @return Omnibox suggestion builder.
     */
    public OmniboxSuggestionBuilderForTest addSubtype(int subtype) {
        mSubtypes.add(subtype);
        return this;
    }
}
