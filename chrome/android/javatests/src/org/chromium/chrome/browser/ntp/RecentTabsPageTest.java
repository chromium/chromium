// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.common.ContentUrlConstants;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Instrumentation tests for {@link RecentTabsPage}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RetryOnFailure
public class RecentTabsPageTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private FakeRecentlyClosedTabManager mManager;
    private Tab mTab;
    private RecentTabsPage mPage;

    @Before
    public void setUp() throws Exception {
        mManager = new FakeRecentlyClosedTabManager();
        RecentTabsManager.setRecentlyClosedTabManagerForTests(mManager);
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
        mPage = loadRecentTabsPage();
    }

    @After
    public void tearDown() {
        leaveRecentTabsPage();
        RecentTabsManager.setRecentlyClosedTabManagerForTests(null);
    }

    @Test
    @MediumTest
    @Feature({"RecentTabsPage"})
    public void testRecentlyClosedTabs() throws ExecutionException {
        // Set a recently closed tab and confirm a view is rendered for it.
        List<RecentlyClosedTab> tabs = setRecentlyClosedTabs(1);
        Assert.assertEquals(1, mManager.getRecentlyClosedTabs(1).size());
        String title = tabs.get(0).title;
        View view = waitForView(title);

        // Clear the recently closed tabs with the context menu and confirm the view is gone.
        invokeContextMenu(view, RecentTabsRowAdapter.RecentlyClosedTabsGroup.ID_REMOVE_ALL);
        Assert.assertEquals(0, mManager.getRecentlyClosedTabs(1).size());
        waitForViewToDisappear(title);
    }

    /**
     * Generates the specified number of {@link RecentlyClosedTab} instances and sets them on the
     * manager.
     */
    private List<RecentlyClosedTab> setRecentlyClosedTabs(final int tabCount) {
        final List<RecentlyClosedTab> tabs = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (int i = 0; i < tabCount; i++) {
                tabs.add(new RecentlyClosedTab(i, "RecentlyClosedTab title " + i, "url " + i));
            }
            mManager.setRecentlyClosedTabs(tabs);
        });
        return tabs;
    }

    private RecentTabsPage loadRecentTabsPage() {
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        CriteriaHelper.pollUiThread(new Criteria("RecentTabsPage never fully loaded") {
            @Override
            public boolean isSatisfied() {
                return mTab.getNativePage() instanceof RecentTabsPage;
            }
        });
        Assert.assertTrue(mTab.getNativePage() instanceof RecentTabsPage);
        return (RecentTabsPage) mTab.getNativePage();
    }

    /**
     * Leaves and destroys the {@link RecentTabsPage} by navigating the tab to {@code about:blank}.
     */
    private void leaveRecentTabsPage() {
        mActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        CriteriaHelper.pollUiThread(new Criteria("RecentTabsPage is still there") {
            @Override
            public boolean isSatisfied() {
                return !(mTab.getNativePage() instanceof RecentTabsPage);
            }
        });
    }

    /**
     * Waits for the view with the specified text to appear.
     */
    private View waitForView(final String text) {
        final ArrayList<View> views = new ArrayList<>();
        CriteriaHelper.pollUiThread(new Criteria("Could not find view with this text: " + text) {
            @Override
            public boolean isSatisfied() {
                mPage.getView().findViewsWithText(views, text, View.FIND_VIEWS_WITH_TEXT);
                return views.size() == 1;
            }
        });
        return views.get(0);
    }

    /**
     * Waits for the view with the specified text to disappear.
     */
    private void waitForViewToDisappear(final String text) {
        CriteriaHelper.pollUiThread(new Criteria("View with this text is still present: " + text) {
            @Override
            public boolean isSatisfied() {
                ArrayList<View> views = new ArrayList<>();
                mPage.getView().findViewsWithText(views, text, View.FIND_VIEWS_WITH_TEXT);
                return views.isEmpty();
            }
        });
    }

    private void invokeContextMenu(View view, int contextMenuItemId) throws ExecutionException {
        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), view);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                mActivityTestRule.getActivity(), contextMenuItemId, 0));
    }
}
