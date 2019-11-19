// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.res.Resources;
import android.text.Editable;
import android.text.Selection;
import android.text.TextUtils;

import androidx.annotation.ColorRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.AutocompleteText;
import org.chromium.chrome.browser.omnibox.UrlBarProperties.UrlBarTextState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Handles translating the UrlBar model data to the view state.
 */
class UrlBarViewBinder {
    /**
     * @see
     * PropertyModelChangeProcessor.ViewBinder#bind(Object,
     * Object, Object)
     */
    public static void bind(PropertyModel model, UrlBar view, PropertyKey propertyKey) {
        if (UrlBarProperties.ACTION_MODE_CALLBACK.equals(propertyKey)) {
            view.setCustomSelectionActionModeCallback(
                    model.get(UrlBarProperties.ACTION_MODE_CALLBACK));
        } else if (UrlBarProperties.ALLOW_FOCUS.equals(propertyKey)) {
            view.setAllowFocus(model.get(UrlBarProperties.ALLOW_FOCUS));
        } else if (UrlBarProperties.AUTOCOMPLETE_TEXT.equals(propertyKey)) {
            AutocompleteText autocomplete = model.get(UrlBarProperties.AUTOCOMPLETE_TEXT);
            if (view.shouldAutocomplete()) {
                view.setAutocompleteText(autocomplete.userText, autocomplete.autocompleteText);
            }
        } else if (UrlBarProperties.DELEGATE.equals(propertyKey)) {
            view.setDelegate(model.get(UrlBarProperties.DELEGATE));
        } else if (UrlBarProperties.FOCUS_CHANGE_CALLBACK.equals(propertyKey)) {
            final Callback<Boolean> focusChangeCallback =
                    model.get(UrlBarProperties.FOCUS_CHANGE_CALLBACK);
            view.setOnFocusChangeListener((v, focused) -> {
                if (focused) view.setIgnoreTextChangesForAutocomplete(false);
                focusChangeCallback.onResult(focused);
            });
        } else if (UrlBarProperties.SHOW_CURSOR.equals(propertyKey)) {
            view.setCursorVisible(model.get(UrlBarProperties.SHOW_CURSOR));
        } else if (UrlBarProperties.TEXT_CONTEXT_MENU_DELEGATE.equals(propertyKey)) {
            view.setTextContextMenuDelegate(model.get(UrlBarProperties.TEXT_CONTEXT_MENU_DELEGATE));
        } else if (UrlBarProperties.TEXT_STATE.equals(propertyKey)) {
            UrlBarTextState state = model.get(UrlBarProperties.TEXT_STATE);
            view.setIgnoreTextChangesForAutocomplete(true);
            view.setText(state.text);
            view.setTextForAutofillServices(state.textForAutofillServices);
            view.setScrollState(state.scrollType, state.scrollToIndex);
            view.setIgnoreTextChangesForAutocomplete(false);

            if (view.hasFocus()) {
                if (state.selectionState == UrlBarCoordinator.SelectionState.SELECT_ALL) {
                    view.selectAll();
                } else if (state.selectionState == UrlBarCoordinator.SelectionState.SELECT_END) {
                    view.setSelection(view.getText().length());
                }
            }
        } else if (UrlBarProperties.USE_DARK_TEXT_COLORS.equals(propertyKey)) {
            updateTextColors(view, model.get(UrlBarProperties.USE_DARK_TEXT_COLORS));
        } else if (UrlBarProperties.URL_DIRECTION_LISTENER.equals(propertyKey)) {
            view.setUrlDirectionListener(model.get(UrlBarProperties.URL_DIRECTION_LISTENER));
        } else if (UrlBarProperties.URL_TEXT_CHANGE_LISTENER.equals(propertyKey)) {
            view.setUrlTextChangeListener(model.get(UrlBarProperties.URL_TEXT_CHANGE_LISTENER));
        } else if (UrlBarProperties.TEXT_CHANGED_LISTENER.equals(propertyKey)) {
            view.setTextChangedListener(model.get(UrlBarProperties.TEXT_CHANGED_LISTENER));
        } else if (UrlBarProperties.WINDOW_DELEGATE.equals(propertyKey)) {
            view.setWindowDelegate(model.get(UrlBarProperties.WINDOW_DELEGATE));
        }
    }

    private static void updateTextColors(UrlBar view, boolean useDarkTextColors) {
        int originalHighlightColor;
        Object highlightColorObj = view.getTag(R.id.highlight_color);
        if (highlightColorObj == null || !(highlightColorObj instanceof Integer)) {
            originalHighlightColor = view.getHighlightColor();
            view.setTag(R.id.highlight_color, originalHighlightColor);
        } else {
            originalHighlightColor = (Integer) highlightColorObj;
        }

        Resources resources = view.getResources();
        int textColor;
        int hintColor;
        int highlightColor;
        if (useDarkTextColors) {
            textColor = ApiCompatibilityUtils.getColor(resources, R.color.default_text_color_dark);
            hintColor =
                    ApiCompatibilityUtils.getColor(resources, R.color.locationbar_dark_hint_text);
            highlightColor = originalHighlightColor;
        } else {
            textColor = ApiCompatibilityUtils.getColor(resources, R.color.default_text_color_light);
            hintColor =
                    ApiCompatibilityUtils.getColor(resources, R.color.locationbar_light_hint_text);
            highlightColor = ApiCompatibilityUtils.getColor(
                    resources, R.color.highlight_color_on_light_text);
        }

        view.setTextColor(textColor);
        setHintTextColor(view, hintColor);
        view.setHighlightColor(highlightColor);
    }

    private static void setHintTextColor(UrlBar view, @ColorRes int textColor) {
        // Note: Setting the hint text color only takes effect if there is not text in the URL bar.
        //       To get around this, set the URL to empty before setting the hint color and revert
        //       back to the previous text after.
        boolean hasNonEmptyText = false;
        int selectionStart = 0;
        int selectionEnd = 0;
        Editable text = view.getText();
        if (!TextUtils.isEmpty(text)) {
            selectionStart = view.getSelectionStart();
            selectionEnd = view.getSelectionEnd();
            // Make sure the setText in this block does not affect the suggestions.
            view.setIgnoreTextChangesForAutocomplete(true);
            view.setText("");
            hasNonEmptyText = true;
        }
        view.setHintTextColor(textColor);
        if (hasNonEmptyText) {
            view.setText(text);
            if (view.hasFocus()) {
                Selection.setSelection(view.getText(), selectionStart, selectionEnd);
            }
            view.setIgnoreTextChangesForAutocomplete(false);
        }
    }

    private UrlBarViewBinder() {}
}
