// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors;

import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.ERROR_MESSAGE;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.IS_REQUIRED;
import static org.chromium.chrome.browser.autofill.editors.EditorProperties.FieldProperties.VALUE;

import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Predicate;

/**
 * Class used for editor field validation. It supports following types of
 * error messages:
 * 1. Message to show before used has started to edit the field.
 * 2. Message to show when required field is left empty.
 * 3. Message to show when field contains invalid input.
 */
public class EditorFieldValidator {
    @Nullable private String mInitialErrorMessage;
    @Nullable private String mRequiredErrorMessage;
    @Nullable private String mInvalidErrorMessage;
    @Nullable private Predicate<String> mValidationPredicate;

    public EditorFieldValidator() {}

    public static Builder builder() {
        return new Builder();
    }

    /** Builder for the {@link EditorFieldValidator}. */
    public static class Builder {
        private EditorFieldValidator mValidator;

        public Builder() {
            mValidator = new EditorFieldValidator();
        }

        public Builder withInitialErrorMessage(String initialErrorMessage) {
            mValidator.setInitialErrorMessage(initialErrorMessage);
            return this;
        }

        public Builder withRequiredErrorMessage(String requiredErrorMessage) {
            mValidator.setRequiredErrorMessage(requiredErrorMessage);
            return this;
        }

        public Builder withValidationPredicate(
                Predicate<String> validationPredicate, String invalidErrorMessage) {
            mValidator.setValidatorPredicate(validationPredicate, invalidErrorMessage);
            return this;
        }

        public EditorFieldValidator build() {
            return mValidator;
        }
    }

    public void setInitialErrorMessage(String initialErrorMessage) {
        mInitialErrorMessage = initialErrorMessage;
    }

    public void setRequiredErrorMessage(String requiredErrorMessage) {
        mRequiredErrorMessage = requiredErrorMessage;
    }

    public void setValidatorPredicate(
            Predicate<String> validationPredicate, String invalidErrorMessage) {
        mValidationPredicate = validationPredicate;
        mInvalidErrorMessage = invalidErrorMessage;
    }

    public void onUserEditedField() {
        mInitialErrorMessage = null;
    }

    /** Called to check the validity of the field value. */
    public void validate(PropertyModel fieldModel) {
        if (!TextUtils.isEmpty(mInitialErrorMessage)) {
            fieldModel.set(ERROR_MESSAGE, mInitialErrorMessage);
            return;
        }
        if (fieldModel.get(IS_REQUIRED)) {
            assert mRequiredErrorMessage != null;
            String value = fieldModel.get(VALUE);
            if (TextUtils.isEmpty(value) || TextUtils.getTrimmedLength(value) == 0) {
                fieldModel.set(ERROR_MESSAGE, mRequiredErrorMessage);
                return;
            }
        }
        if (mValidationPredicate != null) {
            assert mInvalidErrorMessage != null;
            if (!mValidationPredicate.test(fieldModel.get(VALUE))) {
                fieldModel.set(ERROR_MESSAGE, mInvalidErrorMessage);
                return;
            }
        }
        fieldModel.set(ERROR_MESSAGE, null);
    }
}
