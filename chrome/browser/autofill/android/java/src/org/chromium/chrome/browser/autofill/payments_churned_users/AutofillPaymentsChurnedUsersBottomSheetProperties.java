// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** Properties for the Payments Churned Users bottom sheet. */
@NullMarked
/*package*/ class AutofillPaymentsChurnedUsersBottomSheetProperties {

    static final ReadableIntPropertyKey HEADER_ICON = new ReadableIntPropertyKey("header_icon");
    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>("title");
    static final ReadableObjectPropertyKey<String> DESCRIPTION =
            new ReadableObjectPropertyKey<>("description");

    static final PropertyKey[] ALL_KEYS = {HEADER_ICON, TITLE, DESCRIPTION};

    private AutofillPaymentsChurnedUsersBottomSheetProperties() {}
}
