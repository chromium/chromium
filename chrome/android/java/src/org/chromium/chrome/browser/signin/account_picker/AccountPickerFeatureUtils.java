// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * This class is used to handle state of feature flags in the project
 * MobileIdentityConsistency.
 */
public class AccountPickerFeatureUtils {
    private static final String DISMISS_BUTTON_PARAM = "dismiss_button";
    private static final String DISMISS_BUTTON_NO_THANKS = "no_thanks";

    static boolean shouldShowNoThanksOnDismissButton() {
        return DISMISS_BUTTON_NO_THANKS.equals(ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY_VAR, DISMISS_BUTTON_PARAM));
    }
}
