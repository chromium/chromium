// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import java.util.ArrayList;
import java.util.List;

/**
 * Model wrapper for a data item to contain errors.
 *
 * @param <T> The type that an instance of this class is created for, such as
 *            {@link AssistantAutofillProfile}, {@link AssistantPaymentInstrument}, etc.
 */
public abstract class AssistantOptionModel<T> {
    public T mOption;
    public List<String> mErrors;

    public AssistantOptionModel(T option, List<String> errors) {
        this.mOption = option;
        this.mErrors = errors;
    }

    public AssistantOptionModel(T option) {
        this(option, new ArrayList<>());
    }

    public boolean isComplete() {
        return mErrors.isEmpty();
    }

    /** Model wrapper for an {@link AssistantAutofillProfile}. */
    public static class ContactModel extends AssistantOptionModel<AssistantAutofillProfile> {
        private final boolean mCanEdit;

        public ContactModel(
                AssistantAutofillProfile contact, List<String> errors, boolean canEdit) {
            super(contact, errors);
            mCanEdit = canEdit;
        }

        public ContactModel(AssistantAutofillProfile contact) {
            super(contact);
            mCanEdit = true;
        }

        public boolean canEdit() {
            return mCanEdit;
        }
    }

    /** Model wrapper for an {@link AssistantAutofillProfile}. */
    public static class AddressModel extends AssistantOptionModel<AssistantAutofillProfile> {
        public AddressModel(AssistantAutofillProfile address, List<String> errors) {
            super(address, errors);
        }

        public AddressModel(AssistantAutofillProfile address) {
            super(address);
        }
    }

    /** Model wrapper for an {@code AssistantPaymentInstrument}. */
    public static class PaymentInstrumentModel
            extends AssistantOptionModel<AssistantPaymentInstrument> {
        public PaymentInstrumentModel(
                AssistantPaymentInstrument paymentInstrument, List<String> errors) {
            super(paymentInstrument, errors);
        }

        public PaymentInstrumentModel(AssistantPaymentInstrument paymentInstrument) {
            super(paymentInstrument);
        }
    }
}
