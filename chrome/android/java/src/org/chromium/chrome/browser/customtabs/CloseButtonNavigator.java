// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.browserservices.BrowserServicesActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;

import javax.inject.Inject;

/**
 * Allows navigation to the most recent page that matches a criteria when the Custom Tabs close
 * button is pressed. We call this page the landing page.
 *
 * For example, in Trusted Web Activities we only show the close button when the user has left the
 * verified origin. If the user then pressed the close button, we want to navigate back to the
 * verified origin instead of closing the Activity.
 *
 * Thread safety: Should only be called on UI thread.
 * Native: Requires native.
 */
@ActivityScope
public class CloseButtonNavigator {
    @Nullable private PageCriteria mLandingPageCriteria;
    private final BrowserServicesActivityTabController mTabController;
    private final CustomTabActivityTabProvider mTabProvider;

    @Inject
    public CloseButtonNavigator(BrowserServicesActivityTabController tabController,
            CustomTabActivityTabProvider tabProvider) {
        mTabController = tabController;
        mTabProvider = tabProvider;
    }

    // TODO(peconn): Replace with Predicate<T> when we can use Java 8 libraries.
    /** An interface that allows specifying if a URL matches some criteria. */
    public interface PageCriteria {
        /** Whether the given |url| matches the criteria. */
        boolean matches(String url);
    }

    /** Sets the criteria for the page to go back to. */
    public void setLandingPageCriteria(PageCriteria criteria) {
        assert mLandingPageCriteria == null : "Conflicting criteria for close button navigation.";

        mLandingPageCriteria = criteria;
    }

    private boolean isLandingPage(String url) {
        return mLandingPageCriteria != null && mLandingPageCriteria.matches(url);
    }

    /**
     * Handles navigation and Tab closures that should occur when the close button is pressed. It
     * searches for a landing page in the history of the current Tab and then closes it if none are
     * found. This continues until a landing page is found or all Tabs are closed.
     */
    public void navigateOnClose() {
        while (mTabProvider.getTab() != null) {
            // See if there's a close button navigation in our current Tab.
            NavigationController navigationController = getNavigationController();
            if (navigationController != null && navigateSingleTab(getNavigationController())) {
                return;
            }

            mTabController.closeTab();

            // Check whether the close button navigation would have stopped on the newly revealed
            // Tab. We don't check this at the start of the loop (or make navigateSingleTab
            // consider the current Tab) because in that case if the user started on a landing page,
            // we would not navigate at all. (Admittedly though this case would never happen at the
            // time of writing since landing pages don't show the close button).
            Tab nextTab = mTabProvider.getTab();
            if (nextTab != null && isLandingPage(mTabProvider.getTab().getUrl())) {
                return;
            }
        }
    }

    /**
     * Navigates to the most recent landing page on the current Tab. Returns {@code false} if no
     * criteria for what is a landing page has been given or no such page can be found.
     */
    private boolean navigateSingleTab(@Nullable NavigationController controller) {
        if (mLandingPageCriteria == null || controller == null) return false;

        NavigationHistory history = controller.getNavigationHistory();
        for (int i = history.getCurrentEntryIndex() - 1; i >= 0; i--) {
            String url = history.getEntryAtIndex(i).getUrl();
            if (!isLandingPage(url)) continue;

            controller.goToNavigationIndex(i);
            return true;
        }

        return false;
    }

    @Nullable
    private NavigationController getNavigationController() {
        Tab tab = mTabProvider.getTab();
        if (tab == null) return null;
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return null;
        return webContents.getNavigationController();
    }
}
