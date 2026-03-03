// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import android.content.Context;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

import com.google.android.material.textfield.TextInputLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

@NullMarked
public class EditSearchEngineDialogCoordinator {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final TemplateUrlService mTemplateUrlService;
    @Nullable private PropertyModel mDialogModel;

    public EditSearchEngineDialogCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            TemplateUrlService templateUrlService) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mTemplateUrlService = templateUrlService;
    }

    public void show(TemplateUrl templateUrl) {
        View view = inflateAndPopulateView(templateUrl);
        ModalDialogProperties.Controller controller = createDialogController(view, templateUrl);

        String dialogTitle = mContext.getString(R.string.site_search_dialog_title_edit);
        String saveText = mContext.getString(R.string.site_search_dialog_save);
        String cancelText = mContext.getString(R.string.site_search_dialog_cancel);

        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.TITLE, dialogTitle)
                        .with(ModalDialogProperties.CUSTOM_VIEW, view)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, saveText)
                        .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, cancelText)
                        .build();

        setupTextWatchers(view, templateUrl);

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
    }

    public void dismiss() {
        if (mDialogModel != null) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        }
    }

    private View inflateAndPopulateView(TemplateUrl templateUrl) {
        View view = LayoutInflater.from(mContext).inflate(R.layout.site_search_dialog, null);

        EditText nameInput = view.findViewById(R.id.name_input);
        EditText shortcutInput = view.findViewById(R.id.shortcut_input);
        EditText urlInput = view.findViewById(R.id.url_input);

        nameInput.setText(templateUrl.getShortName());
        shortcutInput.setText(templateUrl.getKeyword());
        urlInput.setText(templateUrl.getURL());

        if (templateUrl.getPrepopulatedId() > 0) {
            urlInput.setEnabled(false);
        }

        return view;
    }

    private void setupTextWatchers(View view, TemplateUrl templateUrl) {
        TextWatcher watcher =
                new TextWatcher() {
                    @Override
                    public void beforeTextChanged(
                            CharSequence s, int start, int count, int after) {}

                    @Override
                    public void onTextChanged(CharSequence s, int start, int before, int count) {}

                    @Override
                    public void afterTextChanged(Editable s) {
                        if (mDialogModel != null) {
                            mDialogModel.set(
                                    ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                                    !areAllInputsValid(view, templateUrl));
                        }
                    }
                };

        ((EditText) view.findViewById(R.id.name_input)).addTextChangedListener(watcher);
        ((EditText) view.findViewById(R.id.shortcut_input)).addTextChangedListener(watcher);
        ((EditText) view.findViewById(R.id.url_input)).addTextChangedListener(watcher);
    }

    private boolean areAllInputsValid(View view, TemplateUrl templateUrl) {
        boolean isNameValid = validateName(view);
        boolean isKeywordValid = validateKeyword(view, templateUrl);
        boolean isUrlValid = validateUrl(view, templateUrl);

        return isNameValid && isKeywordValid && isUrlValid;
    }

    private boolean validateName(View view) {
        TextInputLayout layout = view.findViewById(R.id.name_input_layout);
        EditText input = view.findViewById(R.id.name_input);
        String name = input.getText().toString();

        if (name.length() == 0) {
            layout.setError(null);
            return false;
        }

        boolean isValid = mTemplateUrlService.isSearchEngineNameValid(name);
        layout.setError(
                isValid
                        ? null
                        : mContext.getString(R.string.site_search_dialog_input_not_valid_error));
        return isValid;
    }

    private boolean validateKeyword(View view, TemplateUrl templateUrl) {
        TextInputLayout layout = view.findViewById(R.id.shortcut_input_layout);
        EditText input = view.findViewById(R.id.shortcut_input);
        String keyword = input.getText().toString();

        if (keyword.length() == 0) {
            layout.setError(null);
            return false;
        }

        boolean isValid =
                mTemplateUrlService.isSearchEngineKeywordValidToEdit(
                        keyword, templateUrl.getKeyword());
        layout.setError(
                isValid
                        ? null
                        : mContext.getString(R.string.site_search_dialog_input_not_valid_error));
        return isValid;
    }

    private boolean validateUrl(View view, TemplateUrl templateUrl) {
        TextInputLayout layout = view.findViewById(R.id.url_input_layout);
        EditText input = view.findViewById(R.id.url_input);

        if (!input.isEnabled()) {
            layout.setError(null);
            return true;
        }

        String url = input.getText().toString();
        if (url.length() == 0) {
            layout.setError(null);
            return false;
        }

        boolean isValid =
                mTemplateUrlService.isSearchEngineUrlValidToEdit(url, templateUrl.getKeyword());
        layout.setError(
                isValid
                        ? null
                        : mContext.getString(R.string.site_search_dialog_input_not_valid_error));
        return isValid;
    }

    private ModalDialogProperties.Controller createDialogController(
            View view, TemplateUrl templateUrl) {
        return new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {
                if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                    handlePositiveClick(model, view, templateUrl);
                } else {
                    mModalDialogManager.dismissDialog(
                            model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                }
            }

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                mDialogModel = null;
            }
        };
    }

    private void handlePositiveClick(PropertyModel model, View view, TemplateUrl templateUrl) {
        EditText nameInput = view.findViewById(R.id.name_input);
        EditText shortcutInput = view.findViewById(R.id.shortcut_input);
        EditText urlInput = view.findViewById(R.id.url_input);

        String name = nameInput.getText().toString().trim();
        String newKeyword = shortcutInput.getText().toString().trim();
        String url = urlInput.getText().toString().trim();
        String originalKeyword = templateUrl.getKeyword();

        mTemplateUrlService.editSearchEngine(originalKeyword, name, newKeyword, url);
        mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }
}
