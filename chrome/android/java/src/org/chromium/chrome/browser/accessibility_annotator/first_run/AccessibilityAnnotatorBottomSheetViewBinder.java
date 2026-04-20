// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_annotator.first_run;

import static org.chromium.chrome.browser.accessibility_annotator.first_run.AccessibilityAnnotatorBottomSheetProperties.DESCRIPTION;
import static org.chromium.chrome.browser.accessibility_annotator.first_run.AccessibilityAnnotatorBottomSheetProperties.ICON;
import static org.chromium.chrome.browser.accessibility_annotator.first_run.AccessibilityAnnotatorBottomSheetProperties.PRIMARY_BUTTON_LABEL;
import static org.chromium.chrome.browser.accessibility_annotator.first_run.AccessibilityAnnotatorBottomSheetProperties.SECONDARY_BUTTON_LABEL;
import static org.chromium.chrome.browser.accessibility_annotator.first_run.AccessibilityAnnotatorBottomSheetProperties.TITLE;

import android.view.View;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View Binder for the Accessibility Annotator bottom sheet. */
@NullMarked
/*package*/ class AccessibilityAnnotatorBottomSheetViewBinder {
    static void bind(
            PropertyModel model,
            AccessibilityAnnotatorBottomSheetViewHolder view,
            PropertyKey propertyKey) {
        if (TITLE == propertyKey) {
            setMaybeEmptyText(view.mTitle, model.get(TITLE));
        } else if (DESCRIPTION == propertyKey) {
            CharSequence description = model.get(DESCRIPTION);
            if (description == null || description.length() == 0) {
                view.mDescription.setVisibility(View.GONE);
            } else {
                view.mDescription.setText(description);
                view.mDescription.setVisibility(View.VISIBLE);
            }
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

    private static void setMaybeEmptyText(TextView textView, @Nullable String text) {
        if (text == null || text.isEmpty()) {
            textView.setText("");
            textView.setVisibility(View.GONE);
            return;
        }
        textView.setText(text);
        textView.setVisibility(View.VISIBLE);
    }
}
