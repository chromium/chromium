// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.ImportantFormFactors;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.signin.signin_promo.SigninPromoCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/** Tests for the bookmark opener. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@ImportantFormFactors(DeviceFormFactor.ONLY_TABLET)
@DoNotBatch(reason = "Tabs can't be closed reliably between tests.")
public class BookmarkOpenerTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private BookmarkOpener mBookmarkOpener;

    private BookmarkModel mBookmarkModel;
    private BookmarkActivity mBookmarkActivity;
    private BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private RecyclerView mItemsContainer;

    private UserActionTester mActionTester;
    private WebPageStation mPage;

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnBlankPage();
        mActionTester = new UserActionTester();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel = mActivityTestRule.getActivity().getBookmarkModelForTesting();
                });
    }

    @After
    public void tearDown() throws Exception {
        if (mBookmarkActivity != null) ApplicationTestUtils.finishActivity(mBookmarkActivity);
        if (mActionTester != null) mActionTester.tearDown();
    }

    /**
     * Opens the bookmark manager and waits for it to be ready. Passing pageStation since some tests
     * could open bookmark in another window.
     */
    private void openBookmarkManager(CtaPageStation pageStation) {
        SigninPromoCoordinator.disablePromoForTesting();
        BookmarkPromoHeader.forcePromoVisibilityForTesting(false);

        if (pageStation.getActivity().isTablet()) {
            pageStation =
                    pageStation.loadWebPageProgrammatically(UrlConstants.BOOKMARKS_NATIVE_URL);
            mItemsContainer =
                    pageStation.getActivity().findViewById(R.id.selectable_list_recycler_view);
            mItemsContainer.setItemAnimator(null); // Disable animation to reduce flakiness.
            mBookmarkManagerCoordinator =
                    ((BookmarkPage) pageStation.getTab().getNativePage()).getManagerForTesting();
        } else {
            // Phone
            mBookmarkActivity =
                    ActivityTestUtils.waitForActivity(
                            InstrumentationRegistry.getInstrumentation(),
                            BookmarkActivity.class,
                            new MenuUtils.MenuActivityTrigger(
                                    InstrumentationRegistry.getInstrumentation(),
                                    mActivityTestRule.getActivity(),
                                    R.id.all_bookmarks_menu_id));
            mItemsContainer = mBookmarkActivity.findViewById(R.id.selectable_list_recycler_view);
            mItemsContainer.setItemAnimator(null); // Disable animation to reduce flakiness.
            mBookmarkManagerCoordinator = mBookmarkActivity.getManagerForTesting();
        }

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(false);
                    mBookmarkOpener = mBookmarkManagerCoordinator.getBookmarkOpenerForTesting();
                });
    }

    void openRootFolder() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> getBookmarkDelegate().openFolder(mBookmarkModel.getRootFolderId()));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
    }

    void openMobileBookmarks() {
        openRootFolder();

        // Mobile bookmarks is merged into all bookmarks when improved bookmark is enabled.
        onView(withText("Mobile bookmarks")).perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    void openReadingList() {
        openRootFolder();

        onView(withText("Reading list")).perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private BookmarkDelegate getBookmarkDelegate() {
        return mBookmarkManagerCoordinator.getBookmarkDelegateForTesting();
    }

    @Test
    @MediumTest
    public void testOpenBookmarkInCurrentTab() {
        GURL url = new GURL(UrlConstants.NTP_URL);
        BookmarkId id = addMobileBookmark("test", url);

        ChromeTabbedActivity cta = mPage.getActivity();
        openBookmarkManager(mPage);
        openMobileBookmarks();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkOpener.openBookmarkInCurrentTab(id, /* incognito= */ false));
        CriteriaHelper.pollUiThread(() -> cta.getActivityTab().getUrl().equals(url));
        assertEquals(
                1,
                ChromeTabUtils.getTabCountOnUiThread(
                        cta.getTabModelSelector().getModel(/* incognito= */ false)));

        assertTrue(mActionTester.getActions().contains("MobileBookmarkManagerEntryOpened"));
        assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting("Bookmarks.OpenBookmarkType"));
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkTimeInterval2.Normal"));
    }

    @Test
    @MediumTest
    public void testOpenBookmarkInCurrentTab_ReadingList() {
        GURL url = new GURL("https://google.com"); // Chrome URLs not allowed for reading list
        BookmarkId id = addReadingListBookmark("test", url);

        ChromeTabbedActivity cta = mPage.getActivity();
        openBookmarkManager(mPage);
        openReadingList();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkOpener.openBookmarkInCurrentTab(id, /* incognito= */ false));
        CriteriaHelper.pollUiThread(() -> cta.getActivityTab().getUrl().equals(url));
        assertEquals(
                "Reading List will always open in a new tab",
                2,
                ChromeTabUtils.getTabCountOnUiThread(
                        cta.getTabModelSelector().getModel(/* incognito= */ false)));

        assertTrue(mActionTester.getActions().contains("MobileBookmarkManagerEntryOpened"));
        assertEquals(
                1, RecordHistogram.getHistogramTotalCountForTesting("Bookmarks.OpenBookmarkType"));
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkTimeInterval2.ReadingList"));
    }

    @Test
    @MediumTest
    public void testOpenBookmarkInCurrentTab_Incognito() {
        GURL url = new GURL(UrlConstants.NTP_URL);
        BookmarkId id = addMobileBookmark("test", url);

        IncognitoNewTabPageStation incognitoPageStation = mPage.openNewIncognitoTabOrWindowFast();
        ChromeTabbedActivity cta = incognitoPageStation.getActivity();
        openBookmarkManager(incognitoPageStation);
        openMobileBookmarks();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkOpener.openBookmarkInCurrentTab(id, /* incognito= */ true));
        CriteriaHelper.pollUiThread(() -> cta.getActivityTab().getUrl().equals(url));
        assertEquals(
                1,
                ChromeTabUtils.getTabCountOnUiThread(
                        cta.getTabModelSelector().getModel(/* incognito= */ true)));
    }

    @Test
    @MediumTest
    public void testOpenBookmarksInNewTabs() {
        GURL url = new GURL(UrlConstants.ABOUT_URL);

        List<BookmarkId> ids = new ArrayList<>();
        ids.add(addMobileBookmark("test", url));
        ids.add(addMobileBookmark("test1", new GURL(UrlConstants.NTP_NON_NATIVE_URL)));
        ids.add(addMobileBookmark("test2", new GURL(UrlConstants.NTP_NON_NATIVE_URL)));

        ChromeTabbedActivity cta = mPage.getActivity();
        openBookmarkManager(mPage);
        openMobileBookmarks();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkOpener.openBookmarksInNewTabs(ids, /* incognito= */ false));
        CriteriaHelper.pollUiThread(() -> cta.getActivityTab().getUrl().equals(url));
        assertEquals(
                4,
                ChromeTabUtils.getTabCountOnUiThread(
                        cta.getTabModelSelector().getModel(/* incognito= */ false)));

        assertTrue(
                mActionTester.getActions().contains("MobileBookmarkManagerMultipleEntriesOpened"));
        assertEquals(
                3,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.MultipleOpened.OpenBookmarkType"));
        assertEquals(
                3,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.MultipleOpened.OpenBookmarkTimeInterval2.Normal"));
    }

    @Test
    @MediumTest
    public void testOpenBookmarksInNewTabs_Incognito() {
        GURL url = new GURL(UrlConstants.ABOUT_URL);

        List<BookmarkId> ids = new ArrayList<>();
        ids.add(addMobileBookmark("test", url));
        ids.add(addMobileBookmark("test1", new GURL(UrlConstants.NTP_NON_NATIVE_URL)));
        ids.add(addMobileBookmark("test2", new GURL(UrlConstants.NTP_NON_NATIVE_URL)));

        IncognitoNewTabPageStation incognitoPageStation = mPage.openNewIncognitoTabOrWindowFast();
        ChromeTabbedActivity cta = incognitoPageStation.getActivity();
        openBookmarkManager(incognitoPageStation);
        openMobileBookmarks();

        ThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkOpener.openBookmarksInNewTabs(ids, /* incognito= */ true));
        CriteriaHelper.pollUiThread(() -> cta.getActivityTab().getUrl().equals(url));
        assertEquals(
                4,
                ChromeTabUtils.getTabCountOnUiThread(
                        cta.getTabModelSelector().getModel(/* incognito= */ true)));
    }

    private BookmarkId addMobileBookmark(final String title, GURL url) {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule.getActivityTestRule());
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mBookmarkModel.addBookmark(
                                mBookmarkModel.getMobileFolderId(), 0, title, url));
    }

    private BookmarkId addReadingListBookmark(final String title, final GURL url) {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule.getActivityTestRule());
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        BookmarkId bookmarkId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mBookmarkModel.addToDefaultReadingList(title, url));
        return bookmarkId;
    }
}
