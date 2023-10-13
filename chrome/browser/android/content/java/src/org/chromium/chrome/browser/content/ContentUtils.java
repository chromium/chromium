// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content;

import org.jni_zero.NativeMethods;

import org.chromium.content_public.browser.WebContents;

/** A utility class to expose content functionality. */
public class ContentUtils {
    /**
     * @return The user agent string of Chrome.
     */
    public static String getBrowserUserAgent() {
        return ContentUtilsJni.get().getBrowserUserAgent();
    }

    /**
     * Set the user agent used for override. Currently, the only use case we have for overriding a
     * user agent involves spoofing a desktop Linux user agent for "Request desktop site". Set it
     * for WebContents so that it is available when a NavigationEntry requires the user agent to be
     * overridden.
     */
    public static void setUserAgentOverride(WebContents webContents, boolean overrideInNewTabs) {
        ContentUtilsJni.get().setUserAgentOverride(webContents, overrideInNewTabs);
    }

    @NativeMethods
    interface Natives {
        String getBrowserUserAgent();

        void setUserAgentOverride(WebContents webContents, boolean overrideInNewTabs);
    }
}
