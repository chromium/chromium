// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import android.content.Context;
import android.text.Html;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.MetricAffectingSpan;
import android.text.style.TextAppearanceSpan;

import org.chromium.components.omnibox.AnswerTextType;
import org.chromium.components.omnibox.SuggestionAnswer;

import java.util.List;

/**
 * AnswerText specifies details to be presented in a single line of omnibox suggestion.
 */
abstract class AnswerText {
    final Context mContext;
    /** Density of current display. */
    final float mDensity;
    /** Content of the line of text in omnibox suggestion. */
    SpannableStringBuilder mText;
    /**
     * Accessibility description - used to announce the details of the answer.
     * This carries text to be read out loud to the user when talkback mode is enabled.
     * Content of the Accessibility Description may be different from the content of
     * presented string:
     * - visually we want to highlight the answer part of AiS suggestion,
     * - audibly we want to make sure the AiS suggestion is clear to understand.
     * This frequently means we are presenting answers in different order than we're announcing
     * them.
     */
    String mAccessibilityDescription;

    /**
     * Height of the mText.
     * Each AnswerText can be a combination of multiple text styles (both sizes and colors).
     * This height holds either
     * - running maximum (during build phase)
     * - final maximum (after build phase)
     * height of the text contained in mText.
     * Note: since all our AnswerTexts always use the largest text as the very first span of the
     * whole content, it is safe to assume that this field contains the height of the largest text
     * element in all cases, except when computing styles for the first span (= during the very
     * first call to getAppearanceForText()).
     */
    int mHeightSp;
    /** Whether content can wrap around to present more details. */
    int mMaxLines;

    /**
     * Create new instance of AnswerText.
     *
     * @param context Current context.
     */
    AnswerText(Context context) {
        mContext = context;
        mDensity = context.getResources().getDisplayMetrics().density;
        mText = new SpannableStringBuilder();
        mMaxLines = 1;
    }

    /**
     * Builds a Spannable containing all of the styled text in the supplied ImageLine.
     *
     * @param line All text fields within this line will be used to build the resulting content.
     * @param delegate Callback converting AnswerTextType to an array of TextAppearanceSpan objects.
     */
    protected void build(SuggestionAnswer.ImageLine line) {
        // This method also computes height of the entire text span.
        // Ensure we're not rebuilding or appending once AnswerText has been constructed.
        assert mHeightSp == 0;

        List<SuggestionAnswer.TextField> textFields = line.getTextFields();
        for (int i = 0; i < textFields.size(); i++) {
            appendAndStyleText(
                    textFields.get(i).getText(), getAppearanceForText(textFields.get(i).getType()));
            if (textFields.get(i).hasNumLines()) {
                mMaxLines = Math.max(mMaxLines, Math.min(3, textFields.get(i).getNumLines()));
            }
        }

        if (line.hasAdditionalText()) {
            mText.append("  ");
            appendAndStyleText(line.getAdditionalText().getText(),
                    getAppearanceForText(line.getAdditionalText().getType()));
        }
        if (line.hasStatusText()) {
            mText.append("  ");
            appendAndStyleText(line.getStatusText().getText(),
                    getAppearanceForText(line.getStatusText().getType()));
        }

        mAccessibilityDescription = mText.toString();
    }

    /**
     * Append the styled text in textField to the supplied builder.
     *
     * @param text Text to be appended.
     * @param styles Styles to be applied to appended text.
     */
    @SuppressWarnings("deprecation") // Update usage of Html.fromHtml when API min is 24
    protected void appendAndStyleText(String text, MetricAffectingSpan[] styles) {
        // Unescape HTML entities (e.g. "&quot;", "&gt;").
        text = Html.fromHtml(text).toString();
        text = processAnswerText(text);

        // Determine the maximum height of the TextAppearanceSpans that are applied for this field.
        for (MetricAffectingSpan style : styles) {
            if (!(style instanceof TextAppearanceSpan)) continue;
            TextAppearanceSpan textStyle = (TextAppearanceSpan) style;
            int textHeightSp = (int) (textStyle.getTextSize() / mDensity);
            if (mHeightSp < textHeightSp) mHeightSp = textHeightSp;
        }

        // Append as HTML (answer responses contain simple markup).
        int start = mText.length();
        mText.append(text);
        int end = mText.length();

        for (MetricAffectingSpan style : styles) {
            mText.setSpan(style, start, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
    }

    /**
     * Return the TextAppearanceSpan array specifying text decorations for a given field type.
     *
     * @param type The answer type as specified at http://goto.google.com/ais_api.
     * @return TextAppearanceSpan array specifying styles to be used to present text field.
     */
    protected abstract MetricAffectingSpan[] getAppearanceForText(@AnswerTextType int type);

    /**
     * Process (if desired) content of the answer text.
     *
     * @param text Source text.
     * @return Either original or modified text.
     */
    protected String processAnswerText(String text) {
        return text;
    }
}
