// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Paint;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewCompat;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.style.StyleSpan;
import android.util.Pair;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor;
import org.chromium.chrome.browser.omnibox.MatchClassificationStyle;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion.MatchClassification;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionViewProperties.SuggestionIcon;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionViewProperties.SuggestionTextContainer;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Adapter for providing data and views to the omnibox results list.
 */
@VisibleForTesting
public class OmniboxResultsAdapter extends BaseAdapter {
    private final List<OmniboxResultItem> mSuggestionItems;
    private final Context mContext;
    private final AnswersImageFetcher mImageFetcher;

    private ToolbarDataProvider mDataProvider;
    private OmniboxSuggestionDelegate mSuggestionDelegate;
    private boolean mUseDarkColors = true;
    private Set<String> mPendingAnswerRequestUrls = new HashSet<>();
    private int mLayoutDirection;

    public OmniboxResultsAdapter(Context context, List<OmniboxResultItem> suggestionItems,
            AnswersImageFetcher answersImageFetcher) {
        mContext = context;
        mSuggestionItems = suggestionItems;
        mImageFetcher = answersImageFetcher;
    }

    public void notifySuggestionsChanged() {
        notifyDataSetChanged();
    }

    @Override
    public int getCount() {
        return mSuggestionItems.size();
    }

    @Override
    public Object getItem(int position) {
        return mSuggestionItems.get(position);
    }

    @Override
    public long getItemId(int position) {
        return position;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        SuggestionView suggestionView;
        if (convertView instanceof SuggestionView) {
            suggestionView = (SuggestionView) convertView;
        } else {
            suggestionView = new SuggestionView(mContext);
        }
        OmniboxResultItem item = mSuggestionItems.get(position);
        // Always attempt to update the answer icon before updating the rest of the properties.  If
        // the answer image is already cached, the response will be synchronous and thus we have
        // the ability to push the answer image at the same time with all of the other state.
        //
        // Also, notifyDataSetChanged takes no effect if called within an update, so moving this
        // after pushing the state results in cached icons not showing up.
        maybeFetchAnswerIcon(item);
        updateView(suggestionView, item);

        // TODO(tedchoc): Remove the init function and push params to the model.
        suggestionView.init(item, mSuggestionDelegate, position);
        ViewCompat.setLayoutDirection(suggestionView, mLayoutDirection);

        return suggestionView;
    }

    private PropertyModel getModel(SuggestionView view) {
        PropertyModel model = (PropertyModel) view.getTag(R.id.view_model);
        if (model == null) {
            model = new PropertyModel(SuggestionViewProperties.ALL_KEYS);
            PropertyModelChangeProcessor.create(model, view, SuggestionViewViewBinder::bind);
            model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE, SuggestionIcon.UNDEFINED);
            view.setTag(R.id.view_model, model);
        }
        return model;
    }

    private void updateView(SuggestionView view, OmniboxResultItem item) {
        PropertyModel model = getModel(view);
        Paint textLine1Paint = view.getTextLine1().getPaint();
        Paint textLine2Paint = view.getTextLine2().getPaint();
        model.set(SuggestionViewProperties.USE_DARK_COLORS, mUseDarkColors);
        model.set(SuggestionViewProperties.TEXT_LINE_1_ALIGNMENT_CONSTRAINTS, Pair.create(0f, 0f));

        OmniboxSuggestion suggestion = item.getSuggestion();
        // Suggestions with attached answers are rendered with rich results regardless of which
        // suggestion type they are.
        if (suggestion.hasAnswer()) {
            setStateForAnswerSuggestion(model, suggestion.getAnswer(), item.getAnswerImage(),
                    textLine1Paint, textLine2Paint);
        } else {
            setStateForTextSuggestion(model, item, textLine1Paint);
        }
    }

    private boolean applyHighlightToMatchRegions(
            Spannable str, List<MatchClassification> classifications) {
        boolean hasMatch = false;
        for (int i = 0; i < classifications.size(); i++) {
            MatchClassification classification = classifications.get(i);
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

    /**
     * Get the first line for a text based omnibox suggestion.
     * @param suggestionItem The item containing the suggestion data.
     * @param showDescriptionIfPresent Whether to show the description text of the suggestion if
     *                                 the item contains valid data.
     * @param shouldHighlight Whether the query should be highlighted.
     * @param textLine1Paint The paint used for the first text line.
     * @return The first line of text.
     */
    private Spannable getSuggestedQuery(PropertyModel model, OmniboxResultItem suggestionItem,
            boolean showDescriptionIfPresent, boolean shouldHighlight, Paint textLine1Paint) {
        String userQuery = suggestionItem.getMatchedQuery();
        String suggestedQuery = null;
        List<MatchClassification> classifications;
        OmniboxSuggestion suggestion = suggestionItem.getSuggestion();
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
            classifications = new ArrayList<MatchClassification>();
            classifications.add(new MatchClassification(0, MatchClassificationStyle.NONE));
        }

        if (suggestion.getType() == OmniboxSuggestionType.SEARCH_SUGGEST_TAIL) {
            String fillIntoEdit = suggestion.getFillIntoEdit();
            // Data sanity checks.
            if (fillIntoEdit.startsWith(userQuery) && fillIntoEdit.endsWith(suggestedQuery)
                    && fillIntoEdit.length() < userQuery.length() + suggestedQuery.length()) {
                final String ellipsisPrefix = "\u2026 ";
                suggestedQuery = ellipsisPrefix + suggestedQuery;

                // Offset the match classifications by the length of the ellipsis prefix to ensure
                // the highlighting remains correct.
                for (int i = 0; i < classifications.size(); i++) {
                    classifications.set(i,
                            new MatchClassification(
                                    classifications.get(i).offset + ellipsisPrefix.length(),
                                    classifications.get(i).style));
                }
                classifications.add(0, new MatchClassification(0, MatchClassificationStyle.NONE));

                if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mContext)) {
                    float requiredWidth =
                            textLine1Paint.measureText(fillIntoEdit, 0, fillIntoEdit.length());
                    float matchContentsWidth =
                            textLine1Paint.measureText(suggestedQuery, 0, suggestedQuery.length());

                    model.set(SuggestionViewProperties.TEXT_LINE_1_ALIGNMENT_CONSTRAINTS,
                            Pair.create(requiredWidth, matchContentsWidth));
                    // Update the max text widths values in SuggestionList. These will be passed to
                    // the contents view on layout.
                    mSuggestionDelegate.onTextWidthsUpdated(requiredWidth, matchContentsWidth);
                }
            }
        }

        Spannable str = SpannableString.valueOf(suggestedQuery);
        if (shouldHighlight) applyHighlightToMatchRegions(str, classifications);
        return str;
    }

    private void setStateForTextSuggestion(
            PropertyModel model, OmniboxResultItem suggestionItem, Paint textLine1Paint) {
        OmniboxSuggestion suggestion = suggestionItem.getSuggestion();
        int suggestionType = suggestion.getType();
        @SuggestionIcon
        int suggestionIcon;
        Spannable textLine1;

        Spannable textLine2;
        int textLine2Color = 0;
        int textLine2Direction = View.TEXT_DIRECTION_INHERIT;
        if (suggestion.isUrlSuggestion()) {
            suggestionIcon = SuggestionIcon.GLOBE;
            if (suggestion.isStarred()) {
                suggestionIcon = SuggestionIcon.BOOKMARK;
            } else if (suggestionType == OmniboxSuggestionType.HISTORY_URL) {
                suggestionIcon = SuggestionIcon.HISTORY;
            }
            boolean urlHighlighted = false;
            if (!TextUtils.isEmpty(suggestion.getUrl())) {
                Spannable str = SpannableString.valueOf(suggestion.getDisplayText());
                urlHighlighted = applyHighlightToMatchRegions(
                        str, suggestion.getDisplayTextClassifications());
                textLine2 = str;
                textLine2Color = SuggestionViewViewBinder.getStandardUrlColor(
                        mContext, model.get(SuggestionViewProperties.USE_DARK_COLORS));
                textLine2Direction = View.TEXT_DIRECTION_LTR;
            } else {
                textLine2 = null;
            }
            textLine1 =
                    getSuggestedQuery(model, suggestionItem, true, !urlHighlighted, textLine1Paint);
        } else {
            suggestionIcon = SuggestionIcon.MAGNIFIER;
            if (suggestionType == OmniboxSuggestionType.VOICE_SUGGEST) {
                suggestionIcon = SuggestionIcon.VOICE;
            } else if ((suggestionType == OmniboxSuggestionType.SEARCH_SUGGEST_PERSONALIZED)
                    || (suggestionType == OmniboxSuggestionType.SEARCH_HISTORY)) {
                // Show history icon for suggestions based on user queries.
                suggestionIcon = SuggestionIcon.HISTORY;
            }
            textLine1 = getSuggestedQuery(model, suggestionItem, false, true, textLine1Paint);
            if ((suggestionType == OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY)
                    || (suggestionType == OmniboxSuggestionType.SEARCH_SUGGEST_PROFILE)) {
                textLine2 = SpannableString.valueOf(suggestion.getDescription());
                textLine2Color = SuggestionViewViewBinder.getStandardFontColor(
                        mContext, model.get(SuggestionViewProperties.USE_DARK_COLORS));
                textLine2Direction = View.TEXT_DIRECTION_INHERIT;
            } else {
                textLine2 = null;
            }
        }

        model.set(SuggestionViewProperties.IS_ANSWER, false);
        model.set(SuggestionViewProperties.HAS_ANSWER_IMAGE, false);
        model.set(SuggestionViewProperties.ANSWER_IMAGE, null);

        model.set(
                SuggestionViewProperties.TEXT_LINE_1_TEXT, new SuggestionTextContainer(textLine1));
        model.set(SuggestionViewProperties.TEXT_LINE_1_SIZING,
                Pair.create(TypedValue.COMPLEX_UNIT_PX,
                        mContext.getResources().getDimension(
                                R.dimen.omnibox_suggestion_first_line_text_size)));

        model.set(
                SuggestionViewProperties.TEXT_LINE_2_TEXT, new SuggestionTextContainer(textLine2));
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT_COLOR, textLine2Color);
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT_DIRECTION, textLine2Direction);
        model.set(SuggestionViewProperties.TEXT_LINE_2_SIZING,
                Pair.create(TypedValue.COMPLEX_UNIT_PX,
                        mContext.getResources().getDimension(
                                R.dimen.omnibox_suggestion_second_line_text_size)));
        model.set(SuggestionViewProperties.TEXT_LINE_2_MAX_LINES, 1);

        boolean sameAsTyped = suggestionItem.getMatchedQuery().trim().equalsIgnoreCase(
                suggestion.getDisplayText());
        model.set(SuggestionViewProperties.REFINABLE, !sameAsTyped);

        model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE, suggestionIcon);
    }

    private static int parseNumAnswerLines(List<SuggestionAnswer.TextField> textFields) {
        for (int i = 0; i < textFields.size(); i++) {
            if (textFields.get(i).hasNumLines()) {
                return Math.min(3, textFields.get(i).getNumLines());
            }
        }
        return -1;
    }

    /**
     * Sets both lines of the Omnibox suggestion based on an Answers in Suggest result.
     */
    private void setStateForAnswerSuggestion(PropertyModel model, SuggestionAnswer answer,
            Bitmap answerImage, Paint textLine1Paint, Paint textLine2Paint) {
        float density = mContext.getResources().getDisplayMetrics().density;
        SuggestionAnswer.ImageLine firstLine = answer.getFirstLine();
        SuggestionAnswer.ImageLine secondLine = answer.getSecondLine();
        int numAnswerLines = parseNumAnswerLines(secondLine.getTextFields());
        if (numAnswerLines == -1) numAnswerLines = 1;
        model.set(SuggestionViewProperties.IS_ANSWER, true);

        model.set(SuggestionViewProperties.TEXT_LINE_1_SIZING,
                Pair.create(TypedValue.COMPLEX_UNIT_SP,
                        (float) AnswerTextBuilder.getMaxTextHeightSp(firstLine)));
        model.set(SuggestionViewProperties.TEXT_LINE_1_TEXT,
                new SuggestionTextContainer(AnswerTextBuilder.buildSpannable(
                        firstLine, textLine1Paint.getFontMetrics(), density)));

        model.set(SuggestionViewProperties.TEXT_LINE_2_SIZING,
                Pair.create(TypedValue.COMPLEX_UNIT_SP,
                        (float) AnswerTextBuilder.getMaxTextHeightSp(secondLine)));
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT,
                new SuggestionTextContainer(AnswerTextBuilder.buildSpannable(
                        secondLine, textLine2Paint.getFontMetrics(), density)));
        model.set(SuggestionViewProperties.TEXT_LINE_2_MAX_LINES, numAnswerLines);
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT_COLOR,
                SuggestionViewViewBinder.getStandardFontColor(
                        mContext, model.get(SuggestionViewProperties.USE_DARK_COLORS)));
        model.set(SuggestionViewProperties.TEXT_LINE_2_TEXT_DIRECTION, View.TEXT_DIRECTION_INHERIT);

        model.set(SuggestionViewProperties.HAS_ANSWER_IMAGE, secondLine.hasImage());
        model.set(SuggestionViewProperties.ANSWER_IMAGE, answerImage);

        model.set(SuggestionViewProperties.REFINABLE, true);
        model.set(SuggestionViewProperties.SUGGESTION_ICON_TYPE, SuggestionIcon.MAGNIFIER);
    }

    private void maybeFetchAnswerIcon(OmniboxResultItem item) {
        ThreadUtils.assertOnUiThread();

        // Attempting to fetch answer data before we have a profile to request it for.
        if (mDataProvider == null) return;

        // Do not refetch an answer image if it already exists.
        if (item.getAnswerImage() != null) return;
        OmniboxSuggestion suggestion = item.getSuggestion();
        final String url = getAnswerImageRequestUrl(suggestion);
        if (url == null) return;

        // Do not make duplicate answer image requests for the same URL (to avoid generating
        // duplicate bitmaps for the same image).
        if (mPendingAnswerRequestUrls.contains(url)) return;

        mPendingAnswerRequestUrls.add(url);
        mImageFetcher.requestAnswersImage(
                mDataProvider.getProfile(), url, new AnswersImageFetcher.AnswersImageObserver() {
                    @Override
                    public void onAnswersImageChanged(Bitmap bitmap) {
                        ThreadUtils.assertOnUiThread();

                        onAnswerImageReceived(url, bitmap);
                        boolean retVal = mPendingAnswerRequestUrls.remove(url);
                        assert retVal : "Pending answer URL should exist";
                    }
                });
    }

    private String getAnswerImageRequestUrl(OmniboxSuggestion suggestion) {
        if (!suggestion.hasAnswer()) return null;
        return suggestion.getAnswer().getSecondLine().getImage();
    }

    private void onAnswerImageReceived(String url, Bitmap bitmap) {
        boolean didUpdateImage = false;
        for (int i = 0; i < mSuggestionItems.size(); i++) {
            String answerUrl = getAnswerImageRequestUrl(mSuggestionItems.get(i).getSuggestion());
            if (TextUtils.equals(answerUrl, url)) {
                mSuggestionItems.get(i).setAnswerImage(bitmap);
                didUpdateImage = true;
            }
        }
        if (didUpdateImage) notifyDataSetChanged();
    }

    /**
     * Sets the data provider for the toolbar.
     */
    public void setToolbarDataProvider(ToolbarDataProvider provider) {
        mDataProvider = provider;
    }

    /**
     * Set the selection delegate for suggestion entries in the adapter.
     *
     * @param delegate The delegate for suggestion selections.
     */
    public void setSuggestionDelegate(OmniboxSuggestionDelegate delegate) {
        mSuggestionDelegate = delegate;
    }

    /**
     * Sets the layout direction to be used for any new suggestion views.
     * @see View#setLayoutDirection(int)
     */
    public void setLayoutDirection(int layoutDirection) {
        mLayoutDirection = layoutDirection;
    }

    /**
     * @return The selection delegate for suggestion entries in the adapter.
     */
    @VisibleForTesting
    public OmniboxSuggestionDelegate getSuggestionDelegate() {
        return mSuggestionDelegate;
    }

    /**
     * Specifies the visual state to be used by the suggestions.
     * @param useDarkColors Whether dark colors should be used for fonts and icons.
     */
    public void setUseDarkColors(boolean useDarkColors) {
        mUseDarkColors = useDarkColors;
    }

    /**
     * Handler for actions that happen on suggestion view.
     */
    @VisibleForTesting
    public static interface OmniboxSuggestionDelegate {
        /**
         * Triggered when the user selects one of the omnibox suggestions to navigate to.
         * @param suggestion The OmniboxSuggestion which was selected.
         * @param position Position of the suggestion in the drop down view.
         */
        public void onSelection(OmniboxSuggestion suggestion, int position);

        /**
         * Triggered when the user selects to refine one of the omnibox suggestions.
         * @param suggestion The suggestion selected.
         */
        public void onRefineSuggestion(OmniboxSuggestion suggestion);

        /**
         * Triggered when the user long presses the omnibox suggestion.
         * @param suggestion The suggestion selected.
         * @param position The position of the suggestion.
         */
        public void onLongPress(OmniboxSuggestion suggestion, int position);

        /**
         * Triggered when the user navigates to one of the suggestions without clicking on it.
         * @param suggestion The suggestion that was selected.
         */
        public void onSetUrlToSuggestion(OmniboxSuggestion suggestion);

        /**
         * Triggered when the user touches the suggestion view.
         */
        public void onGestureDown();

        /**
         * Triggered when the user touch on the suggestion view finishes.
         * @param ev the event for the ACTION_UP.
         */
        public void onGestureUp(long timetamp);

        /**
         * Triggered when text width information is updated.
         * These values should be used to calculate max text widths.
         * @param requiredWidth a new required width.
         * @param matchContentsWidth a new match contents width.
         */
        public void onTextWidthsUpdated(float requiredWidth, float matchContentsWidth);

        /**
         * @return max required width for the suggestion.
         */
        public float getMaxRequiredWidth();

        /**
         * @return max match contents width for the suggestion.
         */
        public float getMaxMatchContentsWidth();
    }

    /**
     * Simple wrapper around the omnibox suggestions provided in the backend and the query that
     * matched it.
     */
    @VisibleForTesting
    public static class OmniboxResultItem {
        private final OmniboxSuggestion mSuggestion;
        private final String mMatchedQuery;
        private Bitmap mAnswerImage;

        public OmniboxResultItem(OmniboxSuggestion suggestion, String matchedQuery) {
            mSuggestion = suggestion;
            mMatchedQuery = matchedQuery;
        }

        /**
         * @return The omnibox suggestion for this item.
         */
        public OmniboxSuggestion getSuggestion() {
            return mSuggestion;
        }

        /**
         * @return The user query that triggered this suggestion to be shown.
         */
        public String getMatchedQuery() {
            return mMatchedQuery;
        }

        /**
         * @return The image associated with the answer for this suggestion (if applicable).
         */
        @Nullable
        public Bitmap getAnswerImage() {
            return mAnswerImage;
        }

        private void setAnswerImage(Bitmap bitmap) {
            mAnswerImage = bitmap;
        }

        @Override
        public boolean equals(Object o) {
            if (!(o instanceof OmniboxResultItem)) {
                return false;
            }

            OmniboxResultItem item = (OmniboxResultItem) o;
            return mMatchedQuery.equals(item.mMatchedQuery) && mSuggestion.equals(item.mSuggestion);
        }

        @Override
        public int hashCode() {
            return 53 * mMatchedQuery.hashCode() ^ mSuggestion.hashCode();
        }
    }
}
