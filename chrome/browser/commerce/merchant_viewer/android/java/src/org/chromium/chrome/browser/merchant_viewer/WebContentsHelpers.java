// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.merchant_viewer;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/** Collection of {@link WebContents} utility methods used merchant trust experience. */
class WebContentsHelpers {
    /** Creates a {@link WebContents} instance. */
    static WebContents createWebContents(boolean initiallyHidden, boolean initializeRenderer) {
        return WebContentsHelpersJni.get().createWebContents(
                Profile.getLastUsedRegularProfile(), initiallyHidden, initializeRenderer);
    }

    /** Overrides the user agent for a {@link WebContents} instance. */
    static void setUserAgentOverride(WebContents webContents) {
        WebContentsHelpersJni.get().setUserAgentOverride(webContents, false);
    }

    @NativeMethods
    public interface Natives {
        void setUserAgentOverride(WebContents webContents, boolean overrideInNewTabs);
        WebContents createWebContents(
                Profile profile, boolean initiallyHidden, boolean initializeRenderer);
    }
}