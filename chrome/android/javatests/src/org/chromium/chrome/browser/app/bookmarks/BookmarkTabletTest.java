// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.bookmarks.BookmarkDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for the bookmark manager on tablet. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction({DeviceFormFactor.TABLET})
// TODO(crbug.com/40899175): Investigate batching.
@DoNotBatch(reason = "Test has side-effects (bookmarks, pageloads) and thus can't be batched.")
public class BookmarkTabletTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private BookmarkModel mBookmarkModel;
    private RecyclerView mItemsContainer;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel = mActivityTestRule.getActivity().getBookmarkModelForTesting();
                    mBookmarkModel.loadEmptyPartnerBookmarkShimForTesting();
                });
    }

    private void openBookmarkManager() throws InterruptedException {
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        mActivityTestRule.loadUrl(UrlConstants.BOOKMARKS_URL);
        mItemsContainer =
                mActivityTestRule.getActivity().findViewById(R.id.selectable_list_recycler_view);
        mItemsContainer.setItemAnimator(null); // Disable animation to reduce flakiness.
        mBookmarkManagerCoordinator =
                ((BookmarkPage) mActivityTestRule.getActivity().getActivityTab().getNativePage())
                        .getManagerForTesting();

        ThreadUtils.runOnUiThreadBlocking(
                () -> AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(false));
    }

    private BookmarkDelegate getBookmarkDelegate() {
        return mBookmarkManagerCoordinator.getBookmarkDelegateForTesting();
    }

    /**
     * Simulates a click on a tab, selecting it.
     *
     * @param incognito Whether or not this tab is in the incognito or normal stack.
     * @param id The id of the tab to click.
     */
    protected void selectTab(final boolean incognito, final int id) {
        ChromeTabUtils.selectTabWithAction(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                new Runnable() {
                    @Override
                    public void run() {
                        TabStripUtils.clickTab(
                                TabStripUtils.findStripLayoutTab(
                                        mActivityTestRule.getActivity(), incognito, id),
                                InstrumentationRegistry.getInstrumentation(),
                                mActivityTestRule.getActivity());
                    }
                });
    }

    @Test
    @MediumTest
    public void switchBetweenTabs_editVisibility() throws Exception {
        Tab bookmarksTab = mActivityTestRule.getActivity().getActivityTab();
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(
                mItemsContainer,
                mBookmarkManagerCoordinator.getBookmarkDelegateForTesting(),
                mBookmarkModel);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        selectTab(false, bookmarksTab.getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertFalse(
                mBookmarkManagerCoordinator
                        .getToolbarForTesting()
                        .getMenu()
                        .findItem(R.id.edit_menu_id)
                        .isVisible());
    }

    @Test
    @SmallTest
    public void testShowBookmarkManager() throws InterruptedException {
        BookmarkTestUtil.loadEmptyPartnerBookmarksForTesting(mBookmarkModel);
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Tab tab = mActivityTestRule.getActivity().getActivityTab();
                    tab.addObserver(
                            new EmptyTabObserver() {
                                NativePage mBookmarksNativePage;

                                @Override
                                public void onTitleUpdated(Tab tab) {
                                    NativePage nativePage = tab.getNativePage();
                                    // Track that there's only one instance of BookmarkPage created.
                                    if (mBookmarksNativePage != null
                                            && !mBookmarksNativePage.equals(nativePage)) {
                                        callbackHelper.notifyCalled();
                                        return;
                                    }
                                    if (nativePage != null
                                            && nativePage
                                                    .getHost()
                                                    .equals(UrlConstants.BOOKMARKS_HOST)) {
                                        mBookmarksNativePage = nativePage;
                                    }
                                }
                            });
                });
        mActivityTestRule.loadUrl(UrlConstants.BOOKMARKS_URL);
        onView(withText("Mobile bookmarks")).check(matches(isDisplayed()));
        assertEquals(0, callbackHelper.getCallCount());
    }
}
