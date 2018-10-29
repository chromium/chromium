// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.support.annotation.ColorInt;
import android.support.annotation.ColorRes;
import android.text.Spannable;
import android.text.TextUtils;
import android.util.Pair;
import android.view.View;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionViewProperties.SuggestionIcon;
import org.chromium.ui.base.DeviceFormFactor;

class SuggestionViewViewBinder {
    /**
     * @see
     * org.chromium.chrome.browser.modelutil.PropertyModelChangeProcessor.ViewBinder#bind(Object,
     * Object, Object)
     */
    public static void bind(PropertyModel model, SuggestionView view, PropertyKey propertyKey) {
        if (SuggestionViewProperties.USE_DARK_COLORS.equals(propertyKey)) {
            boolean useDarkColors = model.get(SuggestionViewProperties.USE_DARK_COLORS);
            view.updateRefineIconTint(useDarkColors);
            view.updateSuggestionIconTint(useDarkColors);
            view.getTextLine1().setTextColor(
                    getStandardFontColor(view.getContext(), useDarkColors));
        } else if (SuggestionViewProperties.IS_ANSWER.equals(propertyKey)) {
            updateSuggestionLayoutType(view, model);
        } else if (SuggestionViewProperties.HAS_ANSWER_IMAGE.equals(propertyKey)) {
            int visibility =
                    model.get(SuggestionViewProperties.HAS_ANSWER_IMAGE) ? View.VISIBLE : View.GONE;
            view.getAnswerImageView().setVisibility(visibility);
        } else if (SuggestionViewProperties.ANSWER_IMAGE.equals(propertyKey)) {
            view.getAnswerImageView().setImageBitmap(
                    model.get(SuggestionViewProperties.ANSWER_IMAGE));
        } else if (SuggestionViewProperties.REFINABLE.equals(propertyKey)) {
            boolean refinable = model.get(SuggestionViewProperties.REFINABLE);
            view.setRefinable(refinable);
            if (refinable) view.initRefineIcon(model.get(SuggestionViewProperties.USE_DARK_COLORS));
        } else if (SuggestionViewProperties.SUGGESTION_ICON_TYPE.equals(propertyKey)) {
            if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(view.getContext())) return;

            @SuggestionIcon
            int type = model.get(SuggestionViewProperties.SUGGESTION_ICON_TYPE);

            if (type == SuggestionIcon.UNDEFINED) return;

            int drawableId = R.drawable.ic_omnibox_page;
            switch (type) {
                case SuggestionIcon.BOOKMARK:
                    drawableId = R.drawable.btn_star;
                    break;
                case SuggestionIcon.MAGNIFIER:
                    drawableId = R.drawable.ic_suggestion_magnifier;
                    break;
                case SuggestionIcon.HISTORY:
                    drawableId = R.drawable.ic_suggestion_history;
                    break;
                case SuggestionIcon.VOICE:
                    drawableId = R.drawable.btn_mic;
                    break;
                default:
                    break;
            }
            view.setSuggestionIconDrawable(
                    drawableId, model.get(SuggestionViewProperties.USE_DARK_COLORS));
        } else if (SuggestionViewProperties.TEXT_LINE_1_SIZING.equals(propertyKey)) {
            Pair<Integer, Float> sizing = model.get(SuggestionViewProperties.TEXT_LINE_1_SIZING);
            view.getTextLine1().setTextSize(sizing.first, sizing.second);
        } else if (SuggestionViewProperties.TEXT_LINE_1_TEXT.equals(propertyKey)) {
            view.getTextLine1().setText(model.get(SuggestionViewProperties.TEXT_LINE_1_TEXT).text);
        } else if (SuggestionViewProperties.TEXT_LINE_1_ALIGNMENT_CONSTRAINTS.equals(propertyKey)) {
            Pair<Float, Float> constraints =
                    model.get(SuggestionViewProperties.TEXT_LINE_1_ALIGNMENT_CONSTRAINTS);
            view.updateTextAlignmentConstraintWidths(constraints.first, constraints.second);
        } else if (SuggestionViewProperties.TEXT_LINE_2_SIZING.equals(propertyKey)) {
            Pair<Integer, Float> sizing = model.get(SuggestionViewProperties.TEXT_LINE_2_SIZING);
            view.getTextLine2().setTextSize(sizing.first, sizing.second);
        } else if (SuggestionViewProperties.TEXT_LINE_2_MAX_LINES.equals(propertyKey)) {
            updateSuggestionLayoutType(view, model);
            int numberLines = model.get(SuggestionViewProperties.TEXT_LINE_2_MAX_LINES);
            if (numberLines == 1) {
                view.getTextLine2().setEllipsize(null);
                view.getTextLine2().setSingleLine();
            } else {
                view.getTextLine2().setSingleLine(false);
                view.getTextLine2().setEllipsize(TextUtils.TruncateAt.END);
                view.getTextLine2().setMaxLines(numberLines);
            }
        } else if (SuggestionViewProperties.TEXT_LINE_2_TEXT_COLOR.equals(propertyKey)) {
            view.getTextLine2().setTextColor(
                    model.get(SuggestionViewProperties.TEXT_LINE_2_TEXT_COLOR));
        } else if (SuggestionViewProperties.TEXT_LINE_2_TEXT_DIRECTION.equals(propertyKey)) {
            ApiCompatibilityUtils.setTextDirection(view.getTextLine2(),
                    model.get(SuggestionViewProperties.TEXT_LINE_2_TEXT_DIRECTION));
        } else if (SuggestionViewProperties.TEXT_LINE_2_TEXT.equals(propertyKey)) {
            Spannable line2Text = model.get(SuggestionViewProperties.TEXT_LINE_2_TEXT).text;
            if (TextUtils.isEmpty(line2Text)) {
                view.getTextLine2().setVisibility(View.INVISIBLE);
            } else {
                view.getTextLine2().setVisibility(View.VISIBLE);
                view.getTextLine2().setText(line2Text);
            }
        }
    }

    private static void updateSuggestionLayoutType(SuggestionView view, PropertyModel model) {
        boolean isAnswer = model.get(SuggestionViewProperties.IS_ANSWER);
        int numberLines = model.get(SuggestionViewProperties.TEXT_LINE_2_MAX_LINES);
        if (!isAnswer) {
            view.setSuggestionLayoutType(SuggestionView.SuggestionLayoutType.TEXT_SUGGESTION);
        } else {
            view.setSuggestionLayoutType(numberLines > 1
                            ? SuggestionView.SuggestionLayoutType.MULTI_LINE_ANSWER
                            : SuggestionView.SuggestionLayoutType.ANSWER);
        }
    }

    /**
     * Get the appropriate font color to be used for non-URL text in suggestions.
     * @param context The context to load the color.
     * @param useDarkColors Whether dark colors should be used.
     * @return The font color to be used.
     */
    @ColorInt
    public static int getStandardFontColor(Context context, boolean useDarkColors) {
        @ColorRes
        int res = useDarkColors ? R.color.url_emphasis_default_text
                                : R.color.url_emphasis_light_default_text;
        return ApiCompatibilityUtils.getColor(context.getResources(), res);
    }

    /**
     * Get the appropriate font color to be used for URL text in suggestions.
     * @param context The context to load the color.
     * @param useDarkColors Whether dark colors should be used.
     * @return The font color to be used.
     */
    @ColorInt
    public static int getStandardUrlColor(Context context, boolean useDarkColors) {
        @ColorRes
        int res = useDarkColors ? R.color.suggestion_url_dark_modern
                                : R.color.suggestion_url_light_modern;
        return ApiCompatibilityUtils.getColor(context.getResources(), res);
    }
}
