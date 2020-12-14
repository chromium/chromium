// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.ui.account_picker;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * This class is used to handle state of feature flags in the project
 * MobileIdentityConsistency.
 *
 * TODO(crbug.com/1155123): Change the class to package private after modularization
 */
public class AccountPickerFeatureUtils {
    private static final String DISMISS_BUTTON_PARAM = "dismiss_button";
    private static final String HIDE_DISMISS_BUTTON = "hide";

    /**
     * TODO(crbug.com/1155123): Change the method to package private after modularization
     */
    public static boolean shouldHideDismissButton() {
        return HIDE_DISMISS_BUTTON.equals(ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY_VAR, DISMISS_BUTTON_PARAM));
    }
}
