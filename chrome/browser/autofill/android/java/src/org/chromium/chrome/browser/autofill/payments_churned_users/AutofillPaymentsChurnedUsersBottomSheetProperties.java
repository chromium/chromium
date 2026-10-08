// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Properties for the Payments Churned Users bottom sheet. */
@NullMarked
/*package*/ class AutofillPaymentsChurnedUsersBottomSheetProperties {

    static final ReadableIntPropertyKey HEADER_ICON = new ReadableIntPropertyKey("header_icon");
    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>("title");
    static final ReadableObjectPropertyKey<String> DESCRIPTION =
            new ReadableObjectPropertyKey<>("description");
    static final ReadableObjectPropertyKey<String> ACCEPT_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>("accept_button_label");
    static final ReadableObjectPropertyKey<String> CANCEL_BUTTON_LABEL =
            new ReadableObjectPropertyKey<>("cancel_button_label");
    static final ReadableObjectPropertyKey<Runnable> ON_ACCEPT_CLICKED =
            new ReadableObjectPropertyKey<>("on_accept_clicked");
    static final ReadableObjectPropertyKey<Runnable> ON_CANCEL_CLICKED =
            new ReadableObjectPropertyKey<>("on_cancel_clicked");
    static final WritableBooleanPropertyKey SHOW_LOADING_STATE =
            new WritableBooleanPropertyKey("show_loading_state");

    static final PropertyKey[] ALL_KEYS = {
        HEADER_ICON,
        TITLE,
        DESCRIPTION,
        ACCEPT_BUTTON_LABEL,
        CANCEL_BUTTON_LABEL,
        ON_ACCEPT_CLICKED,
        ON_CANCEL_CLICKED,
        SHOW_LOADING_STATE,
    };

    private AutofillPaymentsChurnedUsersBottomSheetProperties() {}
}
