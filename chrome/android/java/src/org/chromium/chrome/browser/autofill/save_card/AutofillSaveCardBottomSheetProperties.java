// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.autofill.payments.LegalMessage;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

@NullMarked
/*package*/ class AutofillSaveCardBottomSheetProperties {
    /** The prompt message for the bottom sheet. */
    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>();

    /** The description text. */
    static final ReadableObjectPropertyKey<String> DESCRIPTION = new ReadableObjectPropertyKey<>();

    /** The icon for the logo of the server upload save card. */
    static final ReadableIntPropertyKey LOGO_ICON = new ReadableIntPropertyKey();

    /** The accessibility description for the logo of the server upload save card bottom sheet. */
    static final ReadableObjectPropertyKey<String> LOGO_ICON_DESCRIPTION =
            new ReadableObjectPropertyKey<>();

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
    static final ReadableObjectPropertyKey<LegalMessage> LEGAL_MESSAGE =
            new ReadableObjectPropertyKey<>();

    /** The label for the button that saves a card to the server. */
    static final ReadableObjectPropertyKey<String> ACCEPT_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();

    /** The label for the button that declines the card save. */
    static final ReadableObjectPropertyKey<String> CANCEL_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();

    /** Indicates whether the bottom sheet is in a loading state. */
    static final WritableBooleanPropertyKey SHOW_LOADING_STATE = new WritableBooleanPropertyKey();

    /** The description for the loading view. */
    static final ReadableObjectPropertyKey<String> LOADING_DESCRIPTION =
            new ReadableObjectPropertyKey<>();

    /** The Google Pay pill logo. */
    static final ReadableIntPropertyKey GOOGLE_PAY_PILL_LOGO = new ReadableIntPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
        TITLE,
        DESCRIPTION,
        LOGO_ICON,
        LOGO_ICON_DESCRIPTION,
        CARD_DESCRIPTION,
        CARD_ICON,
        CARD_LABEL,
        CARD_SUB_LABEL,
        LEGAL_MESSAGE,
        ACCEPT_BUTTON_LABEL,
        CANCEL_BUTTON_LABEL,
        SHOW_LOADING_STATE,
        LOADING_DESCRIPTION,
        GOOGLE_PAY_PILL_LOGO,
    };

    /** Do not instantiate. */
    private AutofillSaveCardBottomSheetProperties() {}
}
