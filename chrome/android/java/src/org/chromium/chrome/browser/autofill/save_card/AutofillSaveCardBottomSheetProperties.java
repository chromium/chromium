// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import com.google.common.collect.ImmutableList;

import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

import java.util.function.Consumer;

/*package*/ class AutofillSaveCardBottomSheetProperties {
    /** Legal messages. */
    static class LegalMessage {
        /** Legal message lines. */
        final ImmutableList<LegalMessageLine> mLines;

        /** The link for the legal message. */
        final Consumer<String> mLink;

        /**
         * Constructs legal messages.
         *
         * @param lines The legal message lines.
         * @param link The link for the legal message.
         */
        LegalMessage(ImmutableList<LegalMessageLine> lines, Consumer<String> link) {
            mLines = lines;
            mLink = link;
        }
    }

    /** The prompt message for the bottom sheet. */
    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>();

    /** The description text. */
    static final ReadableObjectPropertyKey<String> DESCRIPTION = new ReadableObjectPropertyKey<>();

    /** The icon for the logo of the server upload save card. */
    static final ReadableIntPropertyKey LOGO_ICON = new ReadableIntPropertyKey();

    /** The description for the card. */
    static final ReadableObjectPropertyKey<String> CARD_DESCRIPTION =
            new ReadableObjectPropertyKey<>();

    /** The icon for the card. */
    static final ReadableIntPropertyKey CARD_ICON = new ReadableIntPropertyKey();

    /** The label for the card. */
    static final ReadableObjectPropertyKey<String> CARD_LABEL = new ReadableObjectPropertyKey<>();

    /** The sub-label for the card. */
    static final ReadableObjectPropertyKey<String> CARD_SUB_LABEL =
            new ReadableObjectPropertyKey<>();

    /** Legal messages. */
    static final ReadableObjectPropertyKey<AutofillSaveCardBottomSheetProperties.LegalMessage>
            LEGAL_MESSAGE = new ReadableObjectPropertyKey<>();

    /** The label for the button that saves a card to the server. */
    static final ReadableObjectPropertyKey<String> ACCEPT_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();

    /** The label for the button that declines the card save. */
    static final ReadableObjectPropertyKey<String> CANCEL_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();

    static final WritableBooleanPropertyKey SHOW_LOADING_STATE = new WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
        TITLE,
        DESCRIPTION,
        LOGO_ICON,
        CARD_DESCRIPTION,
        CARD_ICON,
        CARD_LABEL,
        CARD_SUB_LABEL,
        LEGAL_MESSAGE,
        ACCEPT_BUTTON_LABEL,
        CANCEL_BUTTON_LABEL,
        SHOW_LOADING_STATE
    };

    /** Do not instantiate. */
    private AutofillSaveCardBottomSheetProperties() {}
}
