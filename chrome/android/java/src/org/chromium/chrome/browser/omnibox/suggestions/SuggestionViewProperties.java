// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.graphics.Bitmap;
import android.support.annotation.IntDef;
import android.text.Spannable;
import android.text.TextUtils;
import android.text.style.UpdateAppearance;
import android.util.Pair;

import org.chromium.chrome.browser.modelutil.PropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.chrome.browser.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The properties associated with rendering the suggestion view.
 */
class SuggestionViewProperties {
    @IntDef({SuggestionIcon.UNDEFINED, SuggestionIcon.BOOKMARK, SuggestionIcon.HISTORY,
            SuggestionIcon.GLOBE, SuggestionIcon.MAGNIFIER, SuggestionIcon.VOICE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SuggestionIcon {
        int UNDEFINED = -1;
        int BOOKMARK = 0;
        int HISTORY = 1;
        int GLOBE = 2;
        int MAGNIFIER = 3;
        int VOICE = 4;
    }

    /**
     * Container for suggestion text that prevents updates when the text/spans has not changed.
     */
    static class SuggestionTextContainer {
        public final Spannable text;

        public SuggestionTextContainer(Spannable text) {
            this.text = text;
        }

        @Override
        public String toString() {
            return "SuggestionTextContainer: " + (text == null ? "null" : text.toString());
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof SuggestionTextContainer)) return false;
            SuggestionTextContainer other = (SuggestionTextContainer) obj;
            if (!TextUtils.equals(text, other.text)) return false;
            if (text == null) return true;

            UpdateAppearance[] thisSpans = text.getSpans(0, text.length(), UpdateAppearance.class);
            UpdateAppearance[] otherSpans =
                    other.text.getSpans(0, other.text.length(), UpdateAppearance.class);
            if (thisSpans.length != otherSpans.length) return false;
            for (int i = 0; i < thisSpans.length; i++) {
                UpdateAppearance thisSpan = thisSpans[i];
                UpdateAppearance otherSpan = otherSpans[i];
                if (!thisSpan.getClass().equals(otherSpan.getClass())) return false;

                if (text.getSpanStart(thisSpan) != other.text.getSpanStart(otherSpan)
                        || text.getSpanEnd(thisSpan) != other.text.getSpanEnd(otherSpan)
                        || text.getSpanFlags(thisSpan) != other.text.getSpanFlags(otherSpan)) {
                    return false;
                }

                // TODO(tedchoc): This is a dangerous assumption.  We should actually update all
                //                span types we use in suggestion text to implement .equals and
                //                ensure the internal styles (e.g. color used in a foreground span)
                //                is actually the same.  This "seems" safe for now, but not
                //                particularly robust.
                //
                //                Once that happens, share this logic with
                //                UrlBarMediator#isNewTextEquivalentToExistingText.
            }
            return true;
        }
    }

    /** Whether dark colors should be applied to text, icons */
    public static final WritableBooleanPropertyKey USE_DARK_COLORS =
            new WritableBooleanPropertyKey();

    /** Whether the suggestion is for an answer. */
    public static final WritableBooleanPropertyKey IS_ANSWER = new WritableBooleanPropertyKey();
    /** Whether an answer image will be shown. */
    public static final WritableBooleanPropertyKey HAS_ANSWER_IMAGE =
            new WritableBooleanPropertyKey();
    /** The answer image to be shown. */
    public static final WritableObjectPropertyKey<Bitmap> ANSWER_IMAGE =
            new WritableObjectPropertyKey<>();

    /** Whether the suggestion supports refinement. */
    public static final WritableBooleanPropertyKey REFINABLE = new WritableBooleanPropertyKey();

    /** The suggestion icon type shown. */
    public static final WritableIntPropertyKey SUGGESTION_ICON_TYPE = new WritableIntPropertyKey();

    /**
     * The sizing information for the first line of text.
     *
     * The first item is the unit of size (e.g. TypedValue.COMPLEX_UNIT_PX), and the second item
     * is the size itself.
     */
    public static final WritableObjectPropertyKey<Pair<Integer, Float>> TEXT_LINE_1_SIZING =
            new WritableObjectPropertyKey<>();
    /** The actual text content for the first line of text. */
    public static final WritableObjectPropertyKey<SuggestionTextContainer> TEXT_LINE_1_TEXT =
            new WritableObjectPropertyKey<>();
    /** The alignment constraints for positioning the first line of text. */
    public static final WritableObjectPropertyKey<Pair<Float, Float>>
            TEXT_LINE_1_ALIGNMENT_CONSTRAINTS = new WritableObjectPropertyKey<>();

    /**
     * The sizing information for the second line of text.
     *
     * The first item is the unit of size (e.g. TypedValue.COMPLEX_UNIT_PX), and the second item
     * is the size itself.
     */
    public static final WritableObjectPropertyKey<Pair<Integer, Float>> TEXT_LINE_2_SIZING =
            new WritableObjectPropertyKey<>();
    /** The maximum number of lines to be shown for the second line of text. */
    public static final WritableIntPropertyKey TEXT_LINE_2_MAX_LINES = new WritableIntPropertyKey();
    /** The color to be applied to the second line of text. */
    public static final WritableIntPropertyKey TEXT_LINE_2_TEXT_COLOR =
            new WritableIntPropertyKey();
    /** The direction the text should be laid out for the second line of text. */
    public static final WritableIntPropertyKey TEXT_LINE_2_TEXT_DIRECTION =
            new WritableIntPropertyKey();
    /** The actual text content for the second line of text. */
    public static final WritableObjectPropertyKey<SuggestionTextContainer> TEXT_LINE_2_TEXT =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {USE_DARK_COLORS, IS_ANSWER, HAS_ANSWER_IMAGE, ANSWER_IMAGE,
                    REFINABLE, SUGGESTION_ICON_TYPE, TEXT_LINE_1_SIZING, TEXT_LINE_1_TEXT,
                    TEXT_LINE_1_ALIGNMENT_CONSTRAINTS, TEXT_LINE_2_SIZING, TEXT_LINE_2_MAX_LINES,
                    TEXT_LINE_2_TEXT_COLOR, TEXT_LINE_2_TEXT_DIRECTION, TEXT_LINE_2_TEXT};
}