// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.email_verification;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** Property keys for the email verification bottom sheet model. */
@NullMarked
public class EmailVerificationBottomSheetProperties {
    /** The title of the email verification prompt. */
    public static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>();

    /** The description explaining that the email will be confirmed automatically. */
    public static final ReadableObjectPropertyKey<String> DESCRIPTION =
            new ReadableObjectPropertyKey<>();

    /** The label on the confirm ("Verify") button. */
    public static final ReadableObjectPropertyKey<String> CONFIRM_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();

    /** The label on the cancel ("Not now") button. */
    public static final ReadableObjectPropertyKey<String> CANCEL_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>();

    /** Whether the drag handle is visible. */
    public static final ReadableBooleanPropertyKey DRAG_HANDLE_VISIBLE =
            new ReadableBooleanPropertyKey();

    /** The callback when the confirm button is clicked. */
    public static final ReadableObjectPropertyKey<Runnable> ON_CONFIRM_CLICKED =
            new ReadableObjectPropertyKey<>();

    /** The callback when the cancel button is clicked. */
    public static final ReadableObjectPropertyKey<Runnable> ON_CANCEL_CLICKED =
            new ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        TITLE,
        DESCRIPTION,
        CONFIRM_BUTTON_LABEL,
        CANCEL_BUTTON_LABEL,
        DRAG_HANDLE_VISIBLE,
        ON_CONFIRM_CLICKED,
        ON_CANCEL_CLICKED
    };

    private EmailVerificationBottomSheetProperties() {}
}
