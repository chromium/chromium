// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sessions;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * The SessionTabHelper Java wrapper to allow communicating with the native SessionTabHelper
 * object.
 */
public class SessionTabHelper {
    /**
     * If WebContents has a SessionTabHelper (probably because it was used as the contents of a
     * tab), returns a session tab id. Returns -1 if the WebContents has no SessionTabHelper. See
     * SessionTabHelper::IdForTab() in session_tab_helper.h
     *
     * @param tab The WebContents to get the tab id for.
     */
    public static int sessionIdForTab(WebContents webContents) {
        return SessionTabHelperJni.get().idForTab(webContents);
    }

    @NativeMethods
    interface Natives {
        int idForTab(WebContents webContents);
    }
}
