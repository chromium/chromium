// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

/**
 * Represents the LaunchParams values to be sent to a web app on app launch and whether the launch
 * triggered a navigation or not.
 */
public class WebAppLaunchParams {
    /**
     * Whether this launch triggered a navigation that needs to be awaited before sending the launch
     * params to the document.
     */
    public final boolean startNewNavigation;

    /** The app being launched, used for scope validation. */
    public final String packageName;

    /**
     * The URL the web app was launched with. Note that redirects may cause us to enqueue in a
     * different URL, we still report the original launch target URL in the launch params.
     */
    public final String targetUrl;

    public WebAppLaunchParams(boolean startNewNavigation, String targetUrl, String packageName) {
        this.startNewNavigation = startNewNavigation;
        this.targetUrl = targetUrl;
        this.packageName = packageName;
    }
}
