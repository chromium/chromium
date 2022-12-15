// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.core.utils;

import com.ark.browser.core.UserAgentManager;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/**
 * A utility class to expose content functionality.
 */
public class ContentUtils {
    /**
     * @return The user agent string of Chrome.
     */
    public static String getBrowserUserAgent() {
        return ContentUtilsJni.get().getBrowserUserAgent();
    }

    /**
     * Set the user agent used for override. Currently, the only use case we have
     * for overriding a user agent involves spoofing a desktop Linux user agent
     * for "Request desktop site". Set it for WebContents so that it is available
     * when a NavigationEntry requires the user agent to be overridden.
     */
    public static void setUserAgentOverride(WebContents webContents, boolean overrideInNewTabs) {
        ContentUtilsJni.get().setUserAgentOverride(webContents, overrideInNewTabs);
    }

    public static void setUserAgentOverride(WebContents webContents, UserAgentManager.UserAgent userAgent) {
        ContentUtilsJni.get().setUserAgent(webContents, userAgent.getString(), userAgent.isMobile());
    }

    public static void setLoadsImagesAutomatically(Profile profile, boolean loadsImagesAutomatically) {
        ContentUtilsJni.get().setLoadsImagesAutomatically(profile, loadsImagesAutomatically);
    }

    public static void setImagesEnabled(Profile profile, boolean enable) {
        ContentUtilsJni.get().setImagesEnabled(profile, enable);
    }

    @NativeMethods
    interface Natives {
        String getBrowserUserAgent();
        void setUserAgentOverride(WebContents webContents, boolean overrideInNewTabs);
        void setUserAgent(WebContents webContents, String ua, boolean isMobile);
        void setLoadsImagesAutomatically(Profile profile, boolean loadsImagesAutomatically);
        void setImagesEnabled(Profile profile, boolean enable);
    }
}
