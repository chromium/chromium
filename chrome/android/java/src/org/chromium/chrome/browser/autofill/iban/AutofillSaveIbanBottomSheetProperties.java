// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import android.view.View.OnClickListener;

import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

import java.util.List;
import java.util.function.Consumer;

/*package*/ class AutofillSaveIbanBottomSheetProperties {
    /** Legal messages. */
    static class LegalMessage {
        /** Legal message lines. */
        final List<LegalMessageLine> mLines;

        /** The link for the legal message. */
        final Consumer<String> mLink;

        /**
         * Constructs legal message.
         *
         * @param lines The legal message lines.
         * @param link The link for the legal message.
         */
        LegalMessage(List<LegalMessageLine> lines, Consumer<String> link) {
            mLines = lines;
            mLink = link;
        }
    }

    static final ReadableIntPropertyKey LOGO_ICON = new ReadableIntPropertyKey();

    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey();

    static final ReadableObjectPropertyKey<String> DESCRIPTION = new ReadableObjectPropertyKey();

    static final ReadableObjectPropertyKey<String> IBAN_VALUE = new ReadableObjectPropertyKey();

    static final ReadableObjectPropertyKey<String> ACCEPT_BUTTON_LABEL =
            new ReadableObjectPropertyKey();

    static final ReadableObjectPropertyKey<String> CANCEL_BUTTON_LABEL =
            new ReadableObjectPropertyKey();

    static final ReadableObjectPropertyKey<OnClickListener> ON_ACCEPT_BUTTON_CLICK_ACTION =
            new ReadableObjectPropertyKey<>();

    static final ReadableObjectPropertyKey<OnClickListener> ON_CANCEL_BUTTON_CLICK_ACTION =
            new ReadableObjectPropertyKey<>();

    static final ReadableObjectPropertyKey<AutofillSaveIbanBottomSheetProperties.LegalMessage>
            LEGAL_MESSAGE = new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        LOGO_ICON,
        TITLE,
        DESCRIPTION,
        IBAN_VALUE,
        ACCEPT_BUTTON_LABEL,
        CANCEL_BUTTON_LABEL,
        ON_ACCEPT_BUTTON_CLICK_ACTION,
        ON_CANCEL_BUTTON_CLICK_ACTION,
        LEGAL_MESSAGE,
    };

    /** Do not instantiate. */
    private AutofillSaveIbanBottomSheetProperties() {}
}
