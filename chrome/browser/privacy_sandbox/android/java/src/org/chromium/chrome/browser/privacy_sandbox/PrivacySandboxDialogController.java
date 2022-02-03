// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;

/**
 * Controller for the dialog shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialogController {
    public static boolean maybeLaunchPrivacySandboxDialog(Context context) {
        // TODO(crbug.com/1286276): Add logic for conditional display.
        PrivacySandboxDialog dialog = new PrivacySandboxDialog(context);
        dialog.show();
        return true;
    }
}
