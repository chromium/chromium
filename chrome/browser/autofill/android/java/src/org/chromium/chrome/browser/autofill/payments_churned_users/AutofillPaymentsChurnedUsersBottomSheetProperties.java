// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;

/** Properties for the Payments Churned Users bottom sheet. */
@NullMarked
/*package*/ class AutofillPaymentsChurnedUsersBottomSheetProperties {

    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>("title");

    static final PropertyKey[] ALL_KEYS = {TITLE};

    private AutofillPaymentsChurnedUsersBottomSheetProperties() {}
}
