// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.dialog;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.search_engines.TemplateUrl;

/** Model that holds the values and validity states of the fields in the site search dialog. */
@NullMarked
class SiteSearchDialogDraft {
    private final String mOriginalKeyword;

    private String mNameInput = "";
    private String mKeywordInput = "";
    private String mUrlInput = "";

    private boolean mIsNameValid;
    private boolean mIsKeywordValid;
    private boolean mIsUrlValid;

    /**
     * Creates a draft model for the dialog.
     *
     * @param templateUrl The template URL to edit, or null if adding a new search engine.
     */
    static SiteSearchDialogDraft create(@Nullable TemplateUrl templateUrl) {
        if (templateUrl == null) {
            return createForAdd();
        } else {
            return createForEdit(templateUrl);
        }
    }

    private static SiteSearchDialogDraft createForAdd() {
        return new SiteSearchDialogDraft(/* originalKeyword= */ "");
    }

    private static SiteSearchDialogDraft createForEdit(TemplateUrl templateUrl) {
        SiteSearchDialogDraft model = new SiteSearchDialogDraft(templateUrl.getKeyword());
        model.setNameInput(templateUrl.getShortName());
        model.setKeywordInput(templateUrl.getKeyword());
        model.setUrlInput(templateUrl.getURL());

        // Pre-populated data is assumed valid.
        model.setNameValid(true);
        model.setKeywordValid(true);
        model.setUrlValid(true);

        return model;
    }

    private SiteSearchDialogDraft(String originalKeyword) {
        mOriginalKeyword = originalKeyword;
    }

    /** Returns the original keyword if editing, or an empty string if adding. */
    String getOriginalKeyword() {
        return mOriginalKeyword;
    }

    String getNameInput() {
        return mNameInput;
    }

    void setNameInput(String nameInput) {
        mNameInput = nameInput;
    }

    boolean isNameValid() {
        return mIsNameValid;
    }

    void setNameValid(boolean isNameValid) {
        mIsNameValid = isNameValid;
    }

    String getKeywordInput() {
        return mKeywordInput;
    }

    void setKeywordInput(String keywordInput) {
        mKeywordInput = keywordInput;
    }

    boolean isKeywordValid() {
        return mIsKeywordValid;
    }

    void setKeywordValid(boolean isKeywordValid) {
        mIsKeywordValid = isKeywordValid;
    }

    String getUrlInput() {
        return mUrlInput;
    }

    void setUrlInput(String urlInput) {
        mUrlInput = urlInput;
    }

    boolean isUrlValid() {
        return mIsUrlValid;
    }

    void setUrlValid(boolean isUrlValid) {
        mIsUrlValid = isUrlValid;
    }

    boolean areAllInputsValid() {
        return mIsNameValid && mIsKeywordValid && mIsUrlValid;
    }

    boolean isTryingToAdd() {
        return mOriginalKeyword.isEmpty();
    }
}
