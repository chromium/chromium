// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.content_public.browser.WebContents;

/**
 * Extension class of WebXrVrTestFramework that allows explicitly specifying whether or not the
 * permission prompt is expected.
 */
public class WebXrVrPermissionTestFramework extends WebXrVrTestFramework {
    private boolean mPermissionPromptExpected = true;

    public WebXrVrPermissionTestFramework(ChromeActivityTestRule testRule) {
        super(testRule);
    }

    /**
     * Sets whether or not the permission prompt is expected to be shown.
     *
     * @param permissionPromptExpected whether or not to expect the permission prompt
     */
    public void setPermissionPromptExpected(boolean permissionPromptExpected) {
        mPermissionPromptExpected = permissionPromptExpected;
    }

    /**
     * Determines whether or not the permission prompt is expected to be shown.
     *
     * @param webContents The webContents of the tab to check if it expects the permission prompt.
     */
    @Override
    public boolean shouldExpectPermissionPrompt(WebContents webContents) {
        return mPermissionPromptExpected;
    }
}
