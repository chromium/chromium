// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.startup;

import android.content.Context;
import android.text.Editable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;

import org.chromium.base.Callback;
import org.chromium.base.ui.KeyboardUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonStyles;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.EmptyTextWatcher;
import org.chromium.url.GURL;

@NullMarked
/* package */ class OnStartupUrlDialogCoordinator {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final @ModalDialogType int mDialogType;
    private @Nullable PropertyModel mDialogModel;
    private boolean mIsEditMode;

    /* package */ OnStartupUrlDialogCoordinator(
            Context context,
            ModalDialogManager modalDialogManager,
            @ModalDialogType int dialogType) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mDialogType = dialogType;
    }

    /* package */ void show(@Nullable String initialUrl, Callback<String> onSaveCallback) {
        if (mDialogModel != null) {
            return;
        }
        boolean isEditMode = initialUrl != null;
        mIsEditMode = isEditMode;
        View customView =
                LayoutInflater.from(mContext)
                        .inflate(R.layout.on_startup_url_dialog, /* root= */ null);
        EditText urlEditText = customView.findViewById(R.id.url_edit_text);

        SimpleModalDialogController controller =
                new SimpleModalDialogController(
                        mModalDialogManager,
                        dismissalCause -> {
                            mDialogModel = null;
                            mIsEditMode = false;
                            if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                                GURL validatedUrl =
                                        getValidatedUrl(urlEditText.getText().toString());
                                if (validatedUrl != null) {
                                    onSaveCallback.onResult(validatedUrl.getSpec());
                                }
                            }
                        });

        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(
                                ModalDialogProperties.TITLE,
                                mContext.getResources(),
                                isEditMode
                                        ? R.string.on_startup_edit_page
                                        : R.string.on_startup_add_new_page)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContext.getResources(),
                                isEditMode ? R.string.save : R.string.add)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContext.getResources(),
                                R.string.cancel)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, true)
                        .build();

        urlEditText.addTextChangedListener(
                new EmptyTextWatcher() {
                    @Override
                    public void afterTextChanged(Editable editable) {
                        boolean isValid = getValidatedUrl(editable.toString()) != null;
                        dialogModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, !isValid);
                    }
                });
        urlEditText.setOnEditorActionListener(
                (view, actionId, event) -> {
                    if (actionId == EditorInfo.IME_ACTION_DONE) {
                        if (!dialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED)) {
                            controller.onClick(dialogModel, ButtonType.POSITIVE);
                        }
                        return true;
                    }
                    return false;
                });

        if (isEditMode) {
            urlEditText.setText(initialUrl);
            urlEditText.setSelection(urlEditText.getText().length());
        }

        mDialogModel = dialogModel;
        mModalDialogManager.showDialog(dialogModel, mDialogType);
        urlEditText.requestFocus();
        KeyboardUtils.showKeyboard(urlEditText);
    }

    /* package */ void dismissIfEditing() {
        if (mDialogModel != null && mIsEditMode) {
            destroy();
        }
    }

    /* package */ void destroy() {
        if (mDialogModel != null) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.ACTIVITY_DESTROYED);
            mDialogModel = null;
            mIsEditMode = false;
        }
    }

    // Normalizes the user input and accepts only valid HTTP and HTTPS URLs.
    private static @Nullable GURL getValidatedUrl(String input) {
        GURL fixedUrl = UrlFormatter.fixupUrl(input.trim());
        return fixedUrl.isValid() && UrlUtilities.isHttpOrHttps(fixedUrl) ? fixedUrl : null;
    }
}
