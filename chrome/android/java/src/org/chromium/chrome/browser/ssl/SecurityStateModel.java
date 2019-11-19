// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ssl;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.WebContents;

/**
 * Provides a way of accessing helpers for page security state.
 */
public class SecurityStateModel {
    /**
     * Fetch the security level for a given web contents.
     *
     * @param webContents The web contents to get the security level for.
     * @return The ConnectionSecurityLevel for the specified web contents.
     *
     * @see ConnectionSecurityLevel
     */
    public static int getSecurityLevelForWebContents(WebContents webContents) {
        if (webContents == null) return ConnectionSecurityLevel.NONE;
        return SecurityStateModelJni.get().getSecurityLevelForWebContents(webContents);
    }

    private SecurityStateModel() {}

    @NativeMethods
    interface Natives {
        int getSecurityLevelForWebContents(WebContents webContents);
    }
}
