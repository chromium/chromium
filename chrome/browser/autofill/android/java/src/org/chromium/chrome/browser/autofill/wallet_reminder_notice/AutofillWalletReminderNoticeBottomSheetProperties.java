// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the Wallet Reminder Notice bottom sheet. */
@NullMarked
/*package*/ class AutofillWalletReminderNoticeBottomSheetProperties {
    static final ReadableObjectPropertyKey<String> TITLE = new ReadableObjectPropertyKey<>("title");

    static final WritableObjectPropertyKey<Runnable> ON_GOT_IT_CLICK_ACTION =
            new WritableObjectPropertyKey<>("on_got_it_click_action");

    static final PropertyKey[] ALL_KEYS = {TITLE, ON_GOT_IT_CLICK_ACTION};

    private AutofillWalletReminderNoticeBottomSheetProperties() {}
}
