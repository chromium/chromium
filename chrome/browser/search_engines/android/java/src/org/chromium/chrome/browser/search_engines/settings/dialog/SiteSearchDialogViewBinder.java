// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.dialog;

import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.widget.EditText;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class SiteSearchDialogViewBinder {
    static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (SiteSearchDialogProperties.NAME == propertyKey) {
            ((EditText) view.findViewById(R.id.name_input))
                    .setText(model.get(SiteSearchDialogProperties.NAME));
        } else if (SiteSearchDialogProperties.KEYWORD == propertyKey) {
            ((EditText) view.findViewById(R.id.shortcut_input))
                    .setText(model.get(SiteSearchDialogProperties.KEYWORD));
        } else if (SiteSearchDialogProperties.URL == propertyKey) {
            ((EditText) view.findViewById(R.id.url_input))
                    .setText(model.get(SiteSearchDialogProperties.URL));
        } else if (SiteSearchDialogProperties.INVALID_NAME_ERROR_MESSAGE == propertyKey) {
            ((TextInputLayout) view.findViewById(R.id.name_input_layout))
                    .setError(model.get(SiteSearchDialogProperties.INVALID_NAME_ERROR_MESSAGE));
        } else if (SiteSearchDialogProperties.INVALID_KEYWORD_ERROR_MESSAGE == propertyKey) {
            ((TextInputLayout) view.findViewById(R.id.shortcut_input_layout))
                    .setError(model.get(SiteSearchDialogProperties.INVALID_KEYWORD_ERROR_MESSAGE));
        } else if (SiteSearchDialogProperties.INVALID_URL_ERROR_MESSAGE == propertyKey) {
            ((TextInputLayout) view.findViewById(R.id.url_input_layout))
                    .setError(model.get(SiteSearchDialogProperties.INVALID_URL_ERROR_MESSAGE));
        } else if (SiteSearchDialogProperties.URL_ENABLED == propertyKey) {
            ((EditText) view.findViewById(R.id.url_input))
                    .setEnabled(model.get(SiteSearchDialogProperties.URL_ENABLED));
        } else if (SiteSearchDialogProperties.ON_NAME_CHANGED == propertyKey) {
            attachTextWatcher(
                    (EditText) view.findViewById(R.id.name_input),
                    model.get(SiteSearchDialogProperties.ON_NAME_CHANGED));
        } else if (SiteSearchDialogProperties.ON_KEYWORD_CHANGED == propertyKey) {
            attachTextWatcher(
                    (EditText) view.findViewById(R.id.shortcut_input),
                    model.get(SiteSearchDialogProperties.ON_KEYWORD_CHANGED));
        } else if (SiteSearchDialogProperties.ON_URL_CHANGED == propertyKey) {
            attachTextWatcher(
                    (EditText) view.findViewById(R.id.url_input),
                    model.get(SiteSearchDialogProperties.ON_URL_CHANGED));
        }
    }

    private static void attachTextWatcher(EditText editText, Callback<String> callback) {
        editText.addTextChangedListener(
                new TextWatcher() {
                    @Override
                    public void beforeTextChanged(
                            CharSequence s, int start, int count, int after) {}

                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {}

                    @Override
                    public void afterTextChanged(Editable s) {
                        callback.onResult(s.toString());
                    }
                });
    }
}
