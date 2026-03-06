// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.dialog;

import android.content.Context;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Predicate;

/** Contains the logic for the add/edit site search dialog. */
@NullMarked
public class SiteSearchDialogMediator implements ModalDialogProperties.Controller {
    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final TemplateUrlService mTemplateUrlService;

    private PropertyModel mCustomViewModel;
    @Nullable private PropertyModel mDialogModel;
    private SiteSearchDialogDraft mDraft;
    private SiteSearchDialogSaveAction mSaveAction;
    @Nullable private TemplateUrl mTemplateUrl;

    public SiteSearchDialogMediator(
            Context context,
            ModalDialogManager modalDialogManager,
            TemplateUrlService templateUrlService) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mTemplateUrlService = templateUrlService;
    }

    /**
     * Initializes the mediator with the given custom view model, save action, and optional template
     * URL.
     *
     * @param customViewModel The custom view model to use for the dialog.
     * @param saveAction The save action to use for the dialog.
     * @param templateUrl The template URL to edit, or null if trying to add a new search engine.
     */
    @Initializer
    void initialize(
            PropertyModel customViewModel,
            SiteSearchDialogSaveAction saveAction,
            @Nullable TemplateUrl templateUrl) {
        mCustomViewModel = customViewModel;
        mSaveAction = saveAction;
        mTemplateUrl = templateUrl;
        mDraft = SiteSearchDialogDraft.create(mTemplateUrl);
    }

    void show(PropertyModel dialogModel) {
        assert mDraft != null;

        mDialogModel = dialogModel;
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP);
        updateDialogState();
    }

    void onNameChanged(String name) {
        mDraft.setNameInput(name.trim());
        mDraft.setNameValid(
                validateField(
                        mDraft.getNameInput(),
                        input -> mTemplateUrlService.isSearchEngineNameValid(input),
                        SiteSearchDialogProperties.INVALID_NAME_ERROR_MESSAGE));
        updateDialogState();
    }

    void onKeywordChanged(String keyword) {
        mDraft.setKeywordInput(keyword.trim());
        mDraft.setKeywordValid(
                validateField(
                        mDraft.getKeywordInput(),
                        input ->
                                mDraft.isTryingToAdd()
                                        ? mTemplateUrlService.isSearchEngineKeywordValidToAdd(input)
                                        : mTemplateUrlService.isSearchEngineKeywordValidToEdit(
                                                input, mDraft.getOriginalKeyword()),
                        SiteSearchDialogProperties.INVALID_KEYWORD_ERROR_MESSAGE));
        updateDialogState();
    }

    void onUrlChanged(String url) {
        mDraft.setUrlInput(url.trim());
        mDraft.setUrlValid(
                validateField(
                        mDraft.getUrlInput(),
                        input ->
                                mDraft.isTryingToAdd()
                                        ? mTemplateUrlService.isSearchEngineUrlValidToAdd(input)
                                        : mTemplateUrlService.isSearchEngineUrlValidToEdit(
                                                input, mDraft.getOriginalKeyword()),
                        SiteSearchDialogProperties.INVALID_URL_ERROR_MESSAGE));
        updateDialogState();
    }

    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            mSaveAction.onSave(
                    mDraft.getNameInput(), mDraft.getKeywordInput(), mDraft.getUrlInput());
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        } else {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        mDialogModel = null;
    }

    void dismiss() {
        if (mDialogModel != null) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        }
    }

    private boolean validateField(
            String input,
            Predicate<String> validator,
            PropertyModel.WritableObjectPropertyKey<String> errorKey) {
        if (mCustomViewModel == null) return false;
        if (input.isEmpty()) {
            mCustomViewModel.set(errorKey, null);
            return false;
        }
        boolean isValid = validator.test(input);
        mCustomViewModel.set(
                errorKey,
                isValid
                        ? null
                        : mContext.getString(R.string.site_search_dialog_input_not_valid_error));
        return isValid;
    }

    private void updateDialogState() {
        if (mDialogModel != null) {
            mDialogModel.set(
                    ModalDialogProperties.POSITIVE_BUTTON_DISABLED, !mDraft.areAllInputsValid());
        }
    }
}
