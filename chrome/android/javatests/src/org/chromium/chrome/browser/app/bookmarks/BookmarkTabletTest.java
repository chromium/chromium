// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.bookmarks.BookmarkDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.TabStripUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/** Tests for the bookmark manager on tablet. */
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction({UiRestriction.RESTRICTION_TYPE_TABLET})
// TODO(crbug.com/1426138): Investigate batching.
@DoNotBatch(reason = "Test has side-effects (bookmarks, pageloads) and thus can't be batched.")
public class BookmarkTabletTest {
    // clang-format on
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private BookmarkModel mBookmarkModel;
    private RecyclerView mItemsContainer;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel = mActivityTestRule.getActivity().getBookmarkModelForTesting();
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

        TestThreadUtils.runOnUiThreadBlocking(
                () -> getBookmarkDelegate().getDragStateDelegate().setA11yStateForTesting(false));
    }

    private BookmarkDelegate getBookmarkDelegate() {
        return mBookmarkManagerCoordinator.getBookmarkDelegateForTesting();
    }

    /**
     * Simulates a click on a tab, selecting it.
     * @param incognito Whether or not this tab is in the incognito or normal stack.
     * @param id The id of the tab to click.
     */
    protected void selectTab(final boolean incognito, final int id) {
        ChromeTabUtils.selectTabWithAction(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), new Runnable() {
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
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer,
                mBookmarkManagerCoordinator.getBookmarkDelegateForTesting(), mBookmarkModel);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_URL);
        selectTab(false, bookmarksTab.getId());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Assert.assertFalse(mBookmarkManagerCoordinator.getToolbarForTesting()
                                   .getMenu()
                                   .findItem(R.id.edit_menu_id)
                                   .isVisible());
    }

    @Test
    @SmallTest
    public void testShowBookmarkManager() throws InterruptedException {
        BookmarkTestUtil.loadEmptyPartnerBookmarksForTesting(mBookmarkModel);
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkUtils.showBookmarkManager(mActivityTestRule.getActivity(),
                    mBookmarkModel.getMobileFolderId(), /*isIncognito=*/false);
        });

        CriteriaHelper.pollUiThread(
                ()
                        -> mActivityTestRule.getActivity().getActivityTab().getNativePage() != null
                        && mActivityTestRule.getActivity().getActivityTab().getNativePage()
                                        instanceof BookmarkPage);
    }
}
