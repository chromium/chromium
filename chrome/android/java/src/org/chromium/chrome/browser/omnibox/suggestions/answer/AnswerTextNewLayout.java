// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.content.Context;
import android.text.style.MetricAffectingSpan;
import android.text.style.TextAppearanceSpan;

import androidx.annotation.StyleRes;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.components.omnibox.AnswerTextType;
import org.chromium.components.omnibox.AnswerType;
import org.chromium.components.omnibox.SuggestionAnswer;

/**
 * AnswerTextNewLayout builds Omnibox styled Answer suggestion texts for revamped answer layouts.
 */
class AnswerTextNewLayout extends AnswerText {
    private static final String TAG = "AnswerTextNewLayout";
    private final boolean mIsAnswer;
    private final @AnswerType int mAnswerType;

    /**
     * Convert SuggestionAnswer to array of elements that directly translate to user-presented
     * content.
     *
     * @param context Current context.
     * @param suggestion Suggestion to be converted.
     * @param query Query that triggered the suggestion.
     * @return array of AnswerText elements to use to construct suggestion item.
     */
    static AnswerText[] from(Context context, OmniboxSuggestion suggestion, String query) {
        AnswerText[] result = new AnswerText[2];

        SuggestionAnswer answer = suggestion.getAnswer();
        if (answer == null) {
            // As an exception, we handle calculation suggestions, too, considering them an Answer,
            // even if these are not one.
            assert suggestion.getType() == OmniboxSuggestionType.CALCULATOR;
            result[0] = new AnswerTextNewLayout(context, query, true);
            result[1] = new AnswerTextNewLayout(context, suggestion.getDisplayText(), false);
        } else if (answer.getType() == AnswerType.DICTIONARY) {
            result[0] =
                    new AnswerTextNewLayout(context, answer.getType(), answer.getFirstLine(), true);
            result[1] = new AnswerTextNewLayout(
                    context, answer.getType(), answer.getSecondLine(), false);
            result[0].mMaxLines = 1;
        } else {
            // Construct the Answer card presenting AiS in Answer > Query order.
            // Note: Despite AiS being presented in reverse order (first answer, then query)
            // we want to ensure that the query is announced first to visually impaired people
            // to avoid confusion.
            result[0] = new AnswerTextNewLayout(
                    context, answer.getType(), answer.getSecondLine(), true);
            result[1] = new AnswerTextNewLayout(
                    context, answer.getType(), answer.getFirstLine(), false);
            result[1].mMaxLines = 1;

            String temp = result[1].mAccessibilityDescription;
            result[1].mAccessibilityDescription = result[0].mAccessibilityDescription;
            result[0].mAccessibilityDescription = temp;
        }

        return result;
    }

    /**
     * Create new instance of AnswerTextNewLayout for answer suggestions.
     *
     * @param context Current context.
     * @param type Answer type, eg. AnswerType.WEATHER.
     * @param line Suggestion line that will be converted to Answer Text.
     * @param isAnswerLine True, if this instance holds answer.
     */
    AnswerTextNewLayout(Context context, @AnswerType int type, SuggestionAnswer.ImageLine line,
            boolean isAnswerLine) {
        super(context);
        mIsAnswer = isAnswerLine;
        mAnswerType = type;
        build(line);
    }

    /**
     * Create new instance of AnswerTextNewLayout for non-answer suggestions.
     * @param context Current context.
     * @param text Suggestion text.
     * @param isAnswerLine True, if this instance holds answer.
     */
    AnswerTextNewLayout(Context context, String text, boolean isAnswerLine) {
        super(context);
        mIsAnswer = isAnswerLine;
        mAnswerType = AnswerType.INVALID;
        appendAndStyleText(text, getAppearanceForText(AnswerTextType.SUGGESTION));
    }

    /**
     * Process (if desired) content of the answer text.
     *
     * @param text Source text.
     * @return Either original or modified text.
     */
    @Override
    protected String processAnswerText(String text) {
        if (mIsAnswer && mAnswerType == AnswerType.CURRENCY) {
            // Modify the content of answer to present only the value after conversion, that is:
            //    1,000 United State Dollar = 1,330.75 Canadian Dollar
            // becomes
            //    1,330.75 Canadian Dollar
            int offset = text.indexOf(" = ");
            if (offset > 0) {
                text = text.substring(offset + 3);
            }
        }
        return text;
    }

    /**
     * Return the TextAppearanceSpan array specifying text decorations for a given field type.
     *
     * @param type The answer type as specified at http://goto.google.com/ais_api.
     * @return TextAppearanceSpan array specifying styles to be used to present text field.
     */
    @Override
    protected MetricAffectingSpan[] getAppearanceForText(@AnswerTextType int type) {
        return mIsAnswer ? getAppearanceForAnswerText(type) : getAppearanceForQueryText(type);
    }

    /**
     * Return text styles for elements in main line holding answer.
     *
     * @param type The answer type as specified at http://goto.google.com/ais_api.
     * @return array of TextAppearanceSpan objects defining style for the text.
     */
    private MetricAffectingSpan[] getAppearanceForAnswerText(@AnswerTextType int type) {
        if (mAnswerType != AnswerType.DICTIONARY && mAnswerType != AnswerType.FINANCE) {
            return new TextAppearanceSpan[] {
                    new TextAppearanceSpan(mContext, R.style.TextAppearance_BlackTitle1)};
        }

        @StyleRes
        int res = 0;
        switch (type) {
            case AnswerTextType.DESCRIPTION_NEGATIVE:
                res = R.style.TextAppearance_OmniboxAnswerDescriptionNegativeSmall;
                break;

            case AnswerTextType.DESCRIPTION_POSITIVE:
                res = R.style.TextAppearance_OmniboxAnswerDescriptionPositiveSmall;
                break;

            case AnswerTextType.SUGGESTION_SECONDARY_TEXT_MEDIUM:
                res = R.style.TextAppearance_BlackDisabledText2;
                break;

            case AnswerTextType.SUGGESTION:
            case AnswerTextType.PERSONALIZED_SUGGESTION:
            case AnswerTextType.ANSWER_TEXT_MEDIUM:
            case AnswerTextType.ANSWER_TEXT_LARGE:
            case AnswerTextType.TOP_ALIGNED:
            case AnswerTextType.SUGGESTION_SECONDARY_TEXT_SMALL:
                res = R.style.TextAppearance_BlackTitle1;
                break;

            default:
                Log.w(TAG, "Unknown answer type: " + type);
                res = R.style.TextAppearance_BlackTitle1;
                break;
        }

        return new TextAppearanceSpan[] {new TextAppearanceSpan(mContext, res)};
    }

    /**
     * Return text styles for elements in second line holding query.
     *
     * @param type The answer type as specified at http://goto.google.com/ais_api.
     * @return array of TextAppearanceSpan objects defining style for the text.
     */
    private MetricAffectingSpan[] getAppearanceForQueryText(@AnswerTextType int type) {
        return new TextAppearanceSpan[] {
                new TextAppearanceSpan(mContext, R.style.TextAppearance_BlackHint2)};
    }
}
