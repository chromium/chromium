// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.WebContents;

/**
 * Extension class of WebXrVrTestFramework that allows explicitly specifying whether or not the
 * consent dialog is expected.
 */
public class WebXrVrConsentTestFramework extends WebXrVrTestFramework {
    private boolean mConsentDialogExpected = true;

    public WebXrVrConsentTestFramework(ChromeActivityTestRule testRule) {
        super(testRule);
    }

    /**
     * Sets whether or not the consent dialog is expected to be shown.
     *
     * @param consentDialogExpected whether or not to expect the consent dialog
     */
    public void setConsentDialogExpected(boolean consentDialogExpected) {
        mConsentDialogExpected = consentDialogExpected;
    }

    /**
     * Determines whether or not the consent dialog is expected to be shown.
     *
     * @param webContents The webContents of the tab to check if it expects the consent dialog.
     */
    @Override
    public boolean shouldExpectConsentDialog(WebContents webContents) {
        return mConsentDialogExpected;
    }
}
