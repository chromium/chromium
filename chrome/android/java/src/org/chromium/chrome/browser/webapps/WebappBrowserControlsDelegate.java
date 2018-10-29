// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.text.TextUtils;

import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.components.security_state.ConnectionSecurityLevel;

class WebappBrowserControlsDelegate extends TabStateBrowserControlsVisibilityDelegate {
    private final WebappActivity mActivity;

    public WebappBrowserControlsDelegate(WebappActivity activity, Tab tab) {
        super(tab);
        mActivity = activity;
    }

    @Override
    public boolean canShowBrowserControls() {
        if (!super.canShowBrowserControls()) return false;

        return shouldShowBrowserControls(mActivity.scopePolicy(), mActivity.getWebappInfo(),
                mTab.getUrl(), mTab.getSecurityLevel());
    }

    @Override
    public boolean canAutoHideBrowserControls() {
        return canAutoHideBrowserControls(mTab.getSecurityLevel());
    }

    static boolean canAutoHideBrowserControls(int securityLevel) {
        // Allow auto-hiding browser controls unless they are shown because of low security level.
        return !shouldShowBrowserControlsForSecurityLevel(securityLevel);
    }

    /**
     * Returns whether the browser controls should be shown when a webapp is navigated to
     * {@code url} given the page's security level.
     * @param info data of a Web App
     * @param url The webapp's current URL
     * @param securityLevel The security level for the webapp's current URL.
     * @return Whether the browser controls should be shown for {@code url}.
     */
    static boolean shouldShowBrowserControls(@WebappScopePolicy.Type int scopePolicy,
            WebappInfo info, String url, int securityLevel) {
        // Do not show browser controls when URL is not ready yet.
        if (TextUtils.isEmpty(url)) return false;

        return shouldShowBrowserControlsForUrl(scopePolicy, info, url)
                || shouldShowBrowserControlsForDisplayMode(info)
                || shouldShowBrowserControlsForSecurityLevel(securityLevel);
    }

    /**
     * Determines whether the close button should be shown in the PWA toolbar.
     *
     * This is called by {@link WebappActivity}, but contained here as it uses the same concepts
     * that are used to determine visibility of the browser toolbar.
     */
    static boolean shouldShowToolbarCloseButton(WebappActivity activity) {
        // Show if we're on the URL requiring browser controls, i.e. off-scope.
        return shouldShowBrowserControlsForUrl(activity.scopePolicy(), activity.getWebappInfo(),
                       activity.getActivityTab().getUrl())
                // Also keep shown if toolbar is not visible, so that during the in and off-scope
                // transitions we avoid button flickering when toolbar is appearing/disappearing.
                || !shouldShowBrowserControls(activity.scopePolicy(), activity.getWebappInfo(),
                           activity.getActivityTab().getUrl(),
                           activity.getActivityTab().getSecurityLevel());
    }

    /**
     * Returns whether the browser controls should be shown when a webapp is navigated to
     * {@code url}.
     */
    private static boolean shouldShowBrowserControlsForUrl(
            @WebappScopePolicy.Type int scopePolicy, WebappInfo webappInfo, String url) {
        return !WebappScopePolicy.isUrlInScope(scopePolicy, webappInfo, url);
    }

    private static boolean shouldShowBrowserControlsForSecurityLevel(int securityLevel) {
        return securityLevel == ConnectionSecurityLevel.DANGEROUS;
    }

    private static boolean shouldShowBrowserControlsForDisplayMode(WebappInfo info) {
        return info.displayMode() != WebDisplayMode.STANDALONE
                && info.displayMode() != WebDisplayMode.FULLSCREEN;
    }
}
