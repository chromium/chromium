// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.WebContents;

import javax.inject.Inject;

/**
 * Closes the tab or navigates back when the Custom Tabs close button is pressed. The algorithm
 * depends on whether the tab is a child tab - {@link Tab#getParentId()} != Tab.INVALID_TAB_ID.
 *
 * If the tab is not a child tab:
 * Navigates to the most recent page which matches a criteria. We call this page the landing page.
 * For instance, Trusted Web Activities show the close button when the user has left the verified
 * origin. If the user then presses the close button, we want to navigate back to the verified
 * origin instead of closing the Activity.
 *
 * If the tab is a child tab:
 * Webapps: Closes the current tab
 * Other: Same algorithm as non-child tabs.
 *
 * Thread safety: Should only be called on UI thread.
 * Native: Requires native.
 */
@ActivityScope
public class CloseButtonNavigator {
    @Nullable private PageCriteria mLandingPageCriteria;
    private final CustomTabActivityTabController mTabController;
    private final CustomTabActivityTabProvider mTabProvider;
    private final boolean mButtonClosesChildTab;

    @Inject
    public CloseButtonNavigator(CustomTabActivityTabController tabController,
            CustomTabActivityTabProvider tabProvider,
            BrowserServicesIntentDataProvider intentDataProvider) {
        mTabController = tabController;
        mTabProvider = tabProvider;
        mButtonClosesChildTab = intentDataProvider.isWebappOrWebApkActivity();
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
     * Handles navigation and Tab closures that should occur when the close button is pressed.
     */
    public void navigateOnClose() {
        // If the tab is a child tab and |mButtonClosesChildTab| == true, close the child tab.
        Tab currentTab = mTabProvider.getTab();
        boolean isFromChildTab = (currentTab != null
                && CriticalPersistedTabData.from(currentTab).getParentId() != Tab.INVALID_TAB_ID);
        if (isFromChildTab && mButtonClosesChildTab) {
            mTabController.closeTab();
            return;
        }

        // Search for a landing page in the history of the current Tab and then close if if none
        // found. Continue until a landing page is found or all Tabs are closed.
        int numTabsClosed = 0;
        while (mTabProvider.getTab() != null) {
            // See if there's a close button navigation in our current Tab.
            NavigationController navigationController = getNavigationController();
            if (navigationController != null && navigateSingleTab(getNavigationController())) {
                if (isFromChildTab) {
                    recordChildTabScopeAlgorithmClosesOneTab(false);
                }
                return;
            }

            mTabController.closeTab();
            ++numTabsClosed;

            // Check whether the close button navigation would have stopped on the newly revealed
            // Tab. We don't check this at the start of the loop (or make navigateSingleTab
            // consider the current Tab) because in that case if the user started on a landing page,
            // we would not navigate at all.
            Tab nextTab = mTabProvider.getTab();
            if (nextTab != null && isLandingPage(nextTab.getUrl().getSpec())) {
                if (isFromChildTab) {
                    recordChildTabScopeAlgorithmClosesOneTab(numTabsClosed == 1);
                }
                return;
            }
        }

        if (numTabsClosed > 0) {
            RecordHistogram.recordCount100Histogram(
                    "CustomTabs.TabCounts.OnClosingAllTabs", numTabsClosed);
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
            String url = history.getEntryAtIndex(i).getUrl().getSpec();
            if (!isLandingPage(url)) continue;

            controller.goToNavigationIndex(i);
            return true;
        }

        return false;
    }

    /**
     * Records how often the "navigate to landing page" algorithm for child tabs for Custom Tabs and
     * Trusted Web Activities produces the same behaviour as the webapp "close current tab"
     * algorithm.
     */
    private void recordChildTabScopeAlgorithmClosesOneTab(boolean closesOneTab) {
        RecordHistogram.recordBooleanHistogram(
                "CustomTabs.CloseButton.ChildTab.ScopeAlgorithm.ClosesOneTab", closesOneTab);
    }

    private @Nullable NavigationController getNavigationController() {
        Tab tab = mTabProvider.getTab();
        if (tab == null) return null;
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return null;
        return webContents.getNavigationController();
    }
}
