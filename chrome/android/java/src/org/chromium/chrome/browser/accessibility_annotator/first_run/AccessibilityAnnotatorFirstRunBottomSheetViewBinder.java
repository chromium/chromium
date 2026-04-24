// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator.first_run;

import static org.chromium.chrome.browser.accessibility_annotator.first_run.AccessibilityAnnotatorFirstRunBottomSheetProperties.CARD_1_TEXT;
import static org.chromium.chrome.browser.accessibility_annotator.first_run.AccessibilityAnnotatorFirstRunBottomSheetProperties.CARD_2_TEXT;
import static org.chromium.chrome.browser.accessibility_annotator.first_run.AccessibilityAnnotatorFirstRunBottomSheetProperties.DESCRIPTION;
import static org.chromium.chrome.browser.accessibility_annotator.first_run.AccessibilityAnnotatorFirstRunBottomSheetProperties.ICON;
import static org.chromium.chrome.browser.accessibility_annotator.first_run.AccessibilityAnnotatorFirstRunBottomSheetProperties.LEARN_MORE_DESCRIPTION;
import static org.chromium.chrome.browser.accessibility_annotator.first_run.AccessibilityAnnotatorFirstRunBottomSheetProperties.PRIMARY_BUTTON_LABEL;
import static org.chromium.chrome.browser.accessibility_annotator.first_run.AccessibilityAnnotatorFirstRunBottomSheetProperties.SECONDARY_BUTTON_LABEL;
import static org.chromium.chrome.browser.accessibility_annotator.first_run.AccessibilityAnnotatorFirstRunBottomSheetProperties.TITLE;

import android.view.View;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View Binder for the Accessibility Annotator first-run bottom sheet. */
@NullMarked
/*package*/ class AccessibilityAnnotatorFirstRunBottomSheetViewBinder {
    static void bind(
            PropertyModel model,
            AccessibilityAnnotatorFirstRunBottomSheetViewHolder view,
            PropertyKey propertyKey) {
        if (TITLE == propertyKey) {
            setMaybeEmptyText(view.mTitle, model.get(TITLE));
        } else if (DESCRIPTION == propertyKey) {
            setMaybeEmptyText(view.mDescription, model.get(DESCRIPTION));
        } else if (LEARN_MORE_DESCRIPTION == propertyKey) {
            setMaybeEmptyText(view.mLearnMoreDescription, model.get(LEARN_MORE_DESCRIPTION));
        } else if (CARD_1_TEXT == propertyKey) {
            setMaybeEmptyText(view.mCard1Text, model.get(CARD_1_TEXT));
        } else if (CARD_2_TEXT == propertyKey) {
            setMaybeEmptyText(view.mCard2Text, model.get(CARD_2_TEXT));
        } else if (ICON == propertyKey) {
            int iconId = model.get(ICON);
            if (iconId == 0) {
                view.mIcon.setVisibility(View.GONE);
                return;
            }
            view.mIcon.setImageResource(iconId);
            view.mIcon.setVisibility(View.VISIBLE);
        } else if (PRIMARY_BUTTON_LABEL == propertyKey) {
            setMaybeEmptyText(view.mPrimaryButton, model.get(PRIMARY_BUTTON_LABEL));
        } else if (SECONDARY_BUTTON_LABEL == propertyKey) {
            setMaybeEmptyText(view.mSecondaryButton, model.get(SECONDARY_BUTTON_LABEL));
        }
    }

    private static void setMaybeEmptyText(TextView textView, @Nullable CharSequence text) {
        if (text == null || text.length() == 0) {
            textView.setText("");
            textView.setVisibility(View.GONE);
            return;
        }
        textView.setText(text);
        textView.setVisibility(View.VISIBLE);
    }
}
