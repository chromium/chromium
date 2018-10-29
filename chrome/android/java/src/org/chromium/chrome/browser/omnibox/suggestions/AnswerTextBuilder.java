// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.res.Resources;
import android.graphics.Paint;
import android.text.Html;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.TextPaint;
import android.text.style.AbsoluteSizeSpan;
import android.text.style.ForegroundColorSpan;
import android.text.style.MetricAffectingSpan;
import android.util.Log;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;

import java.util.List;

/**
 * Helper class that builds Spannables to represent the styled text in answers from Answers in
 * Suggest.
 */
class AnswerTextBuilder {
    private static final String TAG = "AnswerTextBuilder";

    // Types, sizes and colors specified at http://goto.google.com/ais_api.
    // Deprecated: ANSWERS_ANSWER_TEXT_TYPE = 1;
    // Deprecated: ANSWERS_HEADLINE_TEXT_TYPE = 2;
    private static final int ANSWERS_TOP_ALIGNED_TEXT_TYPE = 3;
    // Deprecated: ANSWERS_DESCRIPTION_TEXT_TYPE = 4;
    private static final int ANSWERS_DESCRIPTION_TEXT_NEGATIVE_TYPE = 5;
    private static final int ANSWERS_DESCRIPTION_TEXT_POSITIVE_TYPE = 6;
    // Deprecated: ANSWERS_MORE_INFO_TEXT_TYPE = 7;
    private static final int ANSWERS_SUGGESTION_TEXT_TYPE = 8;
    // Deprecated: ANSWERS_SUGGESTION_TEXT_POSITIVE_TYPE = 9;
    // Deprecated: ANSWERS_SUGGESTION_TEXT_NEGATIVE_TYPE = 10;
    // Deprecated: ANSWERS_SUGGESTION_LINK_COLOR_TYPE = 11;
    // Deprecated: ANSWERS_STATUS_TEXT_TYPE = 12;
    private static final int ANSWERS_PERSONALIZED_SUGGESTION_TEXT_TYPE = 13;
    // Deprecated: ANSWERS_IMMERSIVE_DESCRIPTION_TEXT = 14;
    // Deprecated: ANSWERS_DATE_TEXT = 15;
    // Deprecated: ANSWERS_PREVIEW_TEXT = 16;
    private static final int ANSWERS_ANSWER_TEXT_MEDIUM_TYPE = 17;
    private static final int ANSWERS_ANSWER_TEXT_LARGE_TYPE = 18;
    private static final int ANSWERS_SECONDARY_TEXT_SMALL_TYPE = 19;
    private static final int ANSWERS_SECONDARY_TEXT_MEDIUM_TYPE = 20;

    private static final int ANSWERS_TOP_ALIGNED_TEXT_SIZE_SP = 12;
    private static final int ANSWERS_DESCRIPTION_TEXT_NEGATIVE_SIZE_SP = 16;
    private static final int ANSWERS_DESCRIPTION_TEXT_POSITIVE_SIZE_SP = 16;
    private static final int ANSWERS_SUGGESTION_TEXT_SIZE_SP = 16;
    private static final int ANSWERS_PERSONALIZED_SUGGESTION_TEXT_SIZE_SP = 15;
    private static final int ANSWERS_ANSWER_TEXT_MEDIUM_SIZE_SP = 20;
    private static final int ANSWERS_ANSWER_TEXT_LARGE_SIZE_SP = 24;
    private static final int ANSWERS_SECONDARY_TEXT_SMALL_SIZE_SP = 12;
    private static final int ANSWERS_SECONDARY_TEXT_MEDIUM_SIZE_SP = 14;

    /**
     * Builds a Spannable containing all of the styled text in the supplied ImageLine.
     *
     * @param line All text fields within this line will be added to the returned Spannable.
     *             types.
     * @param metrics Font metrics which will be used to properly size and layout images and top-
     *                aligned text.
     * @param density Screen density which will be used to properly size and layout images and top-
     *                aligned text.
     */
    static Spannable buildSpannable(
            SuggestionAnswer.ImageLine line, Paint.FontMetrics metrics, float density) {
        SpannableStringBuilder builder = new SpannableStringBuilder();

        // Determine the height of the largest text element in the line.  This
        // will be used to top-align text and scale images.
        int maxTextHeightSp = getMaxTextHeightSp(line);

        List<SuggestionAnswer.TextField> textFields = line.getTextFields();
        for (int i = 0; i < textFields.size(); i++) {
            appendAndStyleText(builder, textFields.get(i), maxTextHeightSp, metrics, density);
        }
        if (line.hasAdditionalText()) {
            builder.append("  ");
            SuggestionAnswer.TextField additionalText = line.getAdditionalText();
            appendAndStyleText(builder, additionalText, maxTextHeightSp, metrics, density);
        }
        if (line.hasStatusText()) {
            builder.append("  ");
            SuggestionAnswer.TextField statusText = line.getStatusText();
            appendAndStyleText(builder, statusText, maxTextHeightSp, metrics, density);
        }

        return builder;
    }

    /**
     * Determine the height of the largest text field in the entire line.
     *
     * @param line An ImageLine containing the text fields.
     * @return The height in SP.
     */
    static int getMaxTextHeightSp(SuggestionAnswer.ImageLine line) {
        int maxHeightSp = 0;

        List<SuggestionAnswer.TextField> textFields = line.getTextFields();
        for (int i = 0; i < textFields.size(); i++) {
            int height = getAnswerTextSizeSp(textFields.get(i).getType());
            if (height > maxHeightSp) {
                maxHeightSp = height;
            }
        }
        if (line.hasAdditionalText()) {
            int height = getAnswerTextSizeSp(line.getAdditionalText().getType());
            if (height > maxHeightSp) {
                maxHeightSp = height;
            }
        }
        if (line.hasStatusText()) {
            int height = getAnswerTextSizeSp(line.getStatusText().getType());
            if (height > maxHeightSp) {
                maxHeightSp = height;
            }
        }

        return maxHeightSp;
    }

    /**
     * Append the styled text in textField to the supplied builder.
     *
     * @param builder The builder to append the text to.
     * @param textField The text field (with text and type) to append.
     * @param maxTextHeightSp The height in SP of the largest text field in the entire line. Used to
     *                        top-align text when specified.
     * @param metrics Font metrics which will be used to properly size and layout images and top-
     *                aligned text.
     * @param density Screen density which will be used to properly size and layout images and top-
     *                aligned text.
     */
    @SuppressWarnings("deprecation") // Update usage of Html.fromHtml when API min is 24
    private static void appendAndStyleText(SpannableStringBuilder builder,
            SuggestionAnswer.TextField textField, int maxTextHeightSp, Paint.FontMetrics metrics,
            float density) {
        String text = textField.getText();
        int type = textField.getType();

        // Unescape HTML entities (e.g. "&quot;", "&gt;").
        text = Html.fromHtml(text).toString();

        // Append as HTML (answer responses contain simple markup).
        int start = builder.length();
        builder.append(Html.fromHtml(text));
        int end = builder.length();

        // Apply styles according to the type.
        AbsoluteSizeSpan sizeSpan = new AbsoluteSizeSpan(getAnswerTextSizeSp(type), true);
        builder.setSpan(sizeSpan, start, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        ForegroundColorSpan colorSpan = new ForegroundColorSpan(getAnswerTextColor(type));
        builder.setSpan(colorSpan, start, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        if (type == ANSWERS_TOP_ALIGNED_TEXT_TYPE) {
            TopAlignedSpan topAlignedSpan = new TopAlignedSpan(
                    ANSWERS_TOP_ALIGNED_TEXT_SIZE_SP, maxTextHeightSp, metrics, density);
            builder.setSpan(topAlignedSpan, start, end, Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);
        }
    }

    /**
     * Return the SP text height for the specified answer text type.
     *
     * @param type The answer type as specified at http://goto.google.com/ais_api.
     */
    private static int getAnswerTextSizeSp(int type) {
        switch (type) {
            case ANSWERS_TOP_ALIGNED_TEXT_TYPE:
                return ANSWERS_TOP_ALIGNED_TEXT_SIZE_SP;
            case ANSWERS_DESCRIPTION_TEXT_NEGATIVE_TYPE:
                return ANSWERS_DESCRIPTION_TEXT_NEGATIVE_SIZE_SP;
            case ANSWERS_DESCRIPTION_TEXT_POSITIVE_TYPE:
                return ANSWERS_DESCRIPTION_TEXT_POSITIVE_SIZE_SP;
            case ANSWERS_SUGGESTION_TEXT_TYPE:
                return ANSWERS_SUGGESTION_TEXT_SIZE_SP;
            case ANSWERS_PERSONALIZED_SUGGESTION_TEXT_TYPE:
                return ANSWERS_PERSONALIZED_SUGGESTION_TEXT_SIZE_SP;
            case ANSWERS_ANSWER_TEXT_MEDIUM_TYPE:
                return ANSWERS_ANSWER_TEXT_MEDIUM_SIZE_SP;
            case ANSWERS_ANSWER_TEXT_LARGE_TYPE:
                return ANSWERS_ANSWER_TEXT_LARGE_SIZE_SP;
            case ANSWERS_SECONDARY_TEXT_SMALL_TYPE:
                return ANSWERS_SECONDARY_TEXT_SMALL_SIZE_SP;
            case ANSWERS_SECONDARY_TEXT_MEDIUM_TYPE:
                return ANSWERS_SECONDARY_TEXT_MEDIUM_SIZE_SP;
            default:
                Log.w(TAG, "Unknown answer type: " + type);
                return ANSWERS_SUGGESTION_TEXT_SIZE_SP;
        }
    }

    /**
     * Return the color code for the specified answer text type.
     *
     * @param type The answer type as specified at http://goto.google.com/ais_api.
     */
    private static int getAnswerTextColor(int type) {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        switch (type) {
            case ANSWERS_DESCRIPTION_TEXT_NEGATIVE_TYPE:
                return ApiCompatibilityUtils.getColor(
                        resources, R.color.answers_description_text_negative);

            case ANSWERS_DESCRIPTION_TEXT_POSITIVE_TYPE:
                return ApiCompatibilityUtils.getColor(
                        resources, R.color.answers_description_text_positive);

            case ANSWERS_SUGGESTION_TEXT_TYPE:
                return ApiCompatibilityUtils.getColor(resources, R.color.url_emphasis_default_text);

            case ANSWERS_PERSONALIZED_SUGGESTION_TEXT_TYPE:
                return ApiCompatibilityUtils.getColor(resources, R.color.url_emphasis_default_text);

            case ANSWERS_TOP_ALIGNED_TEXT_TYPE:
            case ANSWERS_ANSWER_TEXT_MEDIUM_TYPE:
            case ANSWERS_ANSWER_TEXT_LARGE_TYPE:
            case ANSWERS_SECONDARY_TEXT_SMALL_TYPE:
            case ANSWERS_SECONDARY_TEXT_MEDIUM_TYPE:
                return ApiCompatibilityUtils.getColor(resources, R.color.answers_answer_text);

            default:
                Log.w(TAG, "Unknown answer type: " + type);
                return ApiCompatibilityUtils.getColor(resources, R.color.url_emphasis_default_text);
        }
    }

    /**
     * Aligns the top of the spanned text with the top of some other specified text height. This is
     * done by calculating the ascent of both text heights and shifting the baseline of the spanned
     * text by the difference.  As a result, "top aligned" means the top of the ascents are
     * aligned, which looks as expected in most cases (some glyphs in some fonts are drawn above
     * the top of the ascent).
     */
    private static class TopAlignedSpan extends MetricAffectingSpan {
        private int mBaselineShift;

        /**
         * Constructor for TopAlignedSpan.
         *
         * @param textHeightSp The total height in SP of the text covered by this span.
         * @param maxTextHeightSp The total height in SP of the text we wish to top-align with.
         * @param metrics The font metrics used to determine what proportion of the font height is
         *                the ascent.
         * @param density The display density.
         */
        public TopAlignedSpan(
                int textHeightSp, int maxTextHeightSp, Paint.FontMetrics metrics, float density) {
            float ascentProportion = metrics.ascent / (metrics.top - metrics.bottom);

            int textAscentPx = (int) (textHeightSp * ascentProportion * density);
            int maxTextAscentPx = (int) (maxTextHeightSp * ascentProportion * density);

            this.mBaselineShift = -(maxTextAscentPx - textAscentPx); // Up is -y.
        }

        @Override
        public void updateDrawState(TextPaint tp) {
            tp.baselineShift += mBaselineShift;
        }

        @Override
        public void updateMeasureState(TextPaint tp) {
            tp.baselineShift += mBaselineShift;
        }
    }
}
