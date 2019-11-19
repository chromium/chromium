// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.components.navigation_interception.NavigationParams;

/**
 * An base class for tracking events on the overlay panel.
 * TODO(mdjones): Rename to OverlayPanelContentDelegate.
 */
public class OverlayContentDelegate {
    /**
     * Called when the panel's WebContents navigates in the main frame.
     * @param url The URL being navigated to.
     * @param isExternalUrl Whether the URL is different from the initially loaded URL.
     */
    public void onMainFrameLoadStarted(String url, boolean isExternalUrl) {}

    /**
     * Called when a page navigation results in an error page.
     * @param url The URL that caused the failure.
     * @param isExternalUrl Whether the URL is different from the initially loaded URL.
     * @param isFailure Whether the loaded page is a page with an error response.
     * @param isError Whether the loaded page is an error (interstitial) page.
     */
    public void onMainFrameNavigation(
            String url, boolean isExternalUrl, boolean isFailure, boolean isError) {}

    /**
     * Called when a page title gets updated.
     * @param title Title string
     */
    public void onTitleUpdated(String title) {}

    /*
     * Called when the URL is requested to be opened in a new, separate tab.
     * @param url The URL associated with this request.
     */
    public void onOpenNewTabRequested(String url) {}

    /**
     * Called when content started loading in the panel.
     * @param url The URL that is loading.
     */
    public void onContentLoadStarted(String url) {}

    /**
     * Called when the navigation entry has been committed.
     */
    public void onNavigationEntryCommitted() {}

    /**
     * Determine if a particular navigation should be intercepted.
     * @param externalNavHandler External navigation handler for the activity the panel is in.
     * @param navigationParams The navigation params for the current navigation.
     * @return True if the navigation should be intercepted.
     */
    public boolean shouldInterceptNavigation(ExternalNavigationHandler externalNavHandler,
            NavigationParams navigationParams) {
        return true;
    }

    // ============================================================================================
    // WebContents related events.
    // ============================================================================================

    /**
     * Called then the content visibility is changed.
     * @param isVisible True if the content is visible.
     */
    public void onVisibilityChanged(boolean isVisible) {}

    /**
     * Called when the SSL state changes.
     */

    public void onSSLStateUpdated() {}

    /**
     * Called once the WebContents has been seen.
     */
    public void onContentViewSeen() {}

    /**
     * Called once the WebContents has been created and set up completely.
     */
    public void onContentViewCreated() {}

    /**
     * Called once the WebContents has been destroyed.
     */
    public void onContentViewDestroyed() {}
}
