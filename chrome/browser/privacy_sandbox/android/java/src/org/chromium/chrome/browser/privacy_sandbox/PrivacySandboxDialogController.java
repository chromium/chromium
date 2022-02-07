// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;

/**
 * Controller for the dialog shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialogController {
    /**
     * Launches an appropriate dialog if necessary and returns whether that happened.
     */
    public static boolean maybeLaunchPrivacySandboxDialog(Context context, boolean isIncognito) {
        if (isIncognito) {
            return false;
        }
        @DialogType
        int dialogType = PrivacySandboxBridge.getRequiredDialogType();
        switch (dialogType) {
            case DialogType.NONE:
                return false;
            case DialogType.NOTICE:
                new PrivacySandboxDialogNotice(context).show();
                return true;
            case DialogType.CONSENT:
                new PrivacySandboxDialogConsent(context).show();
                return true;
            default:
                assert false : "Unknown DialogType value.";
                // Should not be reached.
                return false;
        }
    }
}
