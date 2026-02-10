// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.common;

import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NonEditableTextProperties.CLICK_RUNNABLE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NonEditableTextProperties.CONTENT_DESCRIPTION;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NonEditableTextProperties.ICON;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.IMPORTANT_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.NOTICE_TEXT;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.SHOW_BACKGROUND;

import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link EditorComponentsProperties} changes in a {@link PropertyModel}
 * to the suitable method in the corresponding views.
 */
@NullMarked
public class EditorComponentsViewBinder {
    public static void bindNonEditableTextView(PropertyModel model, View view, PropertyKey key) {
        if (key == EditorComponentsProperties.NonEditableTextProperties.PRIMARY_TEXT) {
            TextView textView = view.findViewById(R.id.primary_text);
            textView.setText(
                    model.get(EditorComponentsProperties.NonEditableTextProperties.PRIMARY_TEXT));
        } else if (key == EditorComponentsProperties.NonEditableTextProperties.SECONDARY_TEXT) {
            TextView secondaryTextView = view.findViewById(R.id.secondary_text);
            String secondaryText =
                    model.get(EditorComponentsProperties.NonEditableTextProperties.SECONDARY_TEXT);
            if (secondaryText != null && !secondaryText.isEmpty()) {
                secondaryTextView.setText(secondaryText);
                secondaryTextView.setVisibility(View.VISIBLE);
            } else {
                secondaryTextView.setVisibility(View.GONE);
            }
        } else if (key == ICON) {
            ImageView iconView = view.findViewById(R.id.icon);
            iconView.setImageResource(model.get(ICON));
            iconView.setVisibility(View.VISIBLE);
        } else if (key == CLICK_RUNNABLE) {
            view.setOnClickListener(v -> model.get(CLICK_RUNNABLE).run());
        } else if (key == CONTENT_DESCRIPTION) {
            view.setContentDescription(model.get(CONTENT_DESCRIPTION));
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    public static void bindNoticeTextView(PropertyModel model, TextView view, PropertyKey key) {
        if (key == NOTICE_TEXT) {
            view.setText(model.get(NOTICE_TEXT));
        } else if (key == SHOW_BACKGROUND) {
            view.setBackgroundResource(
                    model.get(SHOW_BACKGROUND) ? R.drawable.autofill_editor_notice_background : 0);
        } else if (key == IMPORTANT_FOR_ACCESSIBILITY) {
            view.setImportantForAccessibility(
                    model.get(IMPORTANT_FOR_ACCESSIBILITY)
                            ? View.IMPORTANT_FOR_ACCESSIBILITY_YES
                            : View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }
}
