// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import android.graphics.Bitmap;

import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

import java.util.ArrayList;
import java.util.LinkedList;

/** The model of the autofill virtual card enrollment bottom sheet UI. */
/*package*/ abstract class AutofillVcnEnrollBottomSheetProperties {
    /** The prompt message for the bottom sheet. */
    /*package*/ static final ReadableObjectPropertyKey<String> MESSAGE_TEXT =
            new ReadableObjectPropertyKey<>();

    /**
     * A list of three elements: (1) the text that describes what a virtual card does, including a
     * "learn more" link text, (2) The "learn more" link text, and (3) the URL to open when the
     * "learn more" link is tapped.
     */
    /*package*/ static final ReadableObjectPropertyKey<ArrayList<String>> DESCRIPTION_TEXT =
            new ReadableObjectPropertyKey<>();

    /**
     * The accessibility description for the container that displays the issuer icon, card label,
     * and card description.
     */
    /*package*/ static final ReadableObjectPropertyKey<String>
            CARD_CONTAINER_ACCESSIBILITY_DESCRIPTION = new ReadableObjectPropertyKey<>();

    /** The icon for the card. */
    /*package*/ static final ReadableObjectPropertyKey<Bitmap> ISSUER_ICON =
            new ReadableObjectPropertyKey<>();

    /** The label for the card. */
    /*package*/ static final ReadableObjectPropertyKey<String> CARD_LABEL =
            new ReadableObjectPropertyKey<>();

    /** The description for the card. */
    /*package*/ static final ReadableObjectPropertyKey<String> CARD_DESCRIPTION =
            new ReadableObjectPropertyKey<>();

    /** Legal messages from Google Pay. */
    /*package*/ static final ReadableObjectPropertyKey<LinkedList<LegalMessageLine>>
            GOOGLE_LEGAL_MESSAGES = new ReadableObjectPropertyKey<>();

    /** Legal messages from the issuer bank. */
    /*package*/ static final ReadableObjectPropertyKey<LinkedList<LegalMessageLine>>
            ISSUER_LEGAL_MESSAGES = new ReadableObjectPropertyKey<>();

    /** The label for the button that enrolls a virtual card. */
    /*package*/ static final ReadableObjectPropertyKey<String> ACCEPT_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();

    /** The label for the button that cancels enrollment. */
    /*package*/ static final ReadableObjectPropertyKey<String> CANCEL_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();

    /*package*/ static final PropertyKey[] ALL_KEYS = {MESSAGE_TEXT, DESCRIPTION_TEXT,
            CARD_CONTAINER_ACCESSIBILITY_DESCRIPTION, ISSUER_ICON, CARD_LABEL, CARD_DESCRIPTION,
            GOOGLE_LEGAL_MESSAGES, ISSUER_LEGAL_MESSAGES, ACCEPT_BUTTON_LABEL, CANCEL_BUTTON_LABEL};
}
