// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.when;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.lifecycle.Stage;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.bookmarks.BookmarkActionBar;
import org.chromium.chrome.browser.bookmarks.BookmarkDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkItemsAdapter;
import org.chromium.chrome.browser.bookmarks.BookmarkManager;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.bookmarks.BookmarkPromoHeader;
import org.chromium.chrome.browser.bookmarks.BookmarkRow;
import org.chromium.chrome.browser.bookmarks.BookmarkUIState;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.ReadingListFeatures;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.read_later.ReadingListUtils;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;

/**
 * Tests for the reading list in the bookmark manager.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.READ_LATER})
@DoNotBatch(reason = "BookmarkTest has behaviours and thus can't be batched.")
public class ReadingListTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TEST_PAGE_URL_GOOGLE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_TITLE_GOOGLE = "The Google";
    private static final String TEST_PAGE_URL_FOO = "/chrome/test/data/android/test.html";
    ;
    private static final int TEST_PORT = 12345;

    private BookmarkManager mManager;
    private BookmarkModel mBookmarkModel;
    private RecyclerView mItemsContainer;
    // Constant but can only be initialized after parameterized test runner setup because this would
    // trigger native load / CommandLineFlag setup.
    private GURL mTestUrlA;
    private GURL mTestPage;
    private GURL mTestPageFoo;
    private EmbeddedTestServer mTestServer;
    private @Nullable BookmarkActivity mBookmarkActivity;
    @Mock
    private SyncService mSyncService;
    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel = mActivityTestRule.getActivity().getBookmarkModelForTesting();

            // Emulate sync disabled so promos are shown.
            when(mSyncService.isSyncRequested()).thenReturn(false);
            SyncService.overrideForTests(mSyncService);
        });
        // Use a custom port so the links are consistent for render tests.
        mActivityTestRule.getEmbeddedTestServerRule().setServerPort(TEST_PORT);
        mTestServer = mActivityTestRule.getTestServer();
        mTestUrlA = new GURL("http://a.com");
        mTestPage = new GURL(mTestServer.getURL(TEST_PAGE_URL_GOOGLE));
        mTestPageFoo = new GURL(mTestServer.getURL(TEST_PAGE_URL_FOO));
    }

    @After
    public void tearDown() throws Exception {
        if (mBookmarkActivity != null) ApplicationTestUtils.finishActivity(mBookmarkActivity);
        if (mActionTester != null) mActionTester.tearDown();
    }

    private void openBookmarkManager() throws InterruptedException {
        if (mActivityTestRule.getActivity().isTablet()) {
            mActivityTestRule.loadUrl(UrlConstants.BOOKMARKS_URL);
            mItemsContainer = mActivityTestRule.getActivity().findViewById(
                    R.id.selectable_list_recycler_view);
            mItemsContainer.setItemAnimator(null); // Disable animation to reduce flakiness.
            mManager = ((BookmarkPage) mActivityTestRule.getActivity()
                                .getActivityTab()
                                .getNativePage())
                               .getManagerForTesting();
        } else {
            // phone
            mBookmarkActivity = ActivityTestUtils.waitForActivity(
                    InstrumentationRegistry.getInstrumentation(), BookmarkActivity.class,
                    new MenuUtils.MenuActivityTrigger(InstrumentationRegistry.getInstrumentation(),
                            mActivityTestRule.getActivity(), R.id.all_bookmarks_menu_id));
            mItemsContainer = mBookmarkActivity.findViewById(R.id.selectable_list_recycler_view);
            mItemsContainer.setItemAnimator(null); // Disable animation to reduce flakiness.
            mManager = mBookmarkActivity.getManagerForTesting();
        }

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.getDragStateDelegate().setA11yStateForTesting(false));
    }

    private BookmarkId addReadingListBookmark(final String title, final GURL url)
            throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addToReadingList(title, url));
    }

    private BookmarkItemsAdapter getReorderAdapter() {
        return (BookmarkItemsAdapter) getAdapter();
    }

    private RecyclerView.Adapter getAdapter() {
        return mItemsContainer.getAdapter();
    }

    private BookmarkId getIdByPosition(int pos) {
        return getReorderAdapter().getIdByPosition(pos);
    }

    private BookmarkManager getBookmarkManager() {
        return (BookmarkManager) getReorderAdapter().getDelegateForTesting();
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @Features.EnableFeatures({ChromeFeatureList.READ_LATER + ":use_root_bookmark_as_default/true"})
    public void testOpenBookmarkManagerWhenDefaultToRootEnabled()
            throws InterruptedException, ExecutionException {
        openBookmarkManager();
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        BookmarkDelegate delegate = getBookmarkManager();
        BookmarkActionBar toolbar = ((BookmarkManager) delegate).getToolbarForTests();

        // We should default to the root bookmark.
        Assert.assertTrue(ReadingListFeatures.shouldUseRootFolderAsDefaultForReadLater());
        Assert.assertEquals(BookmarkUIState.STATE_FOLDER, delegate.getCurrentState());
        Assert.assertEquals("chrome-native://bookmarks/folder/0",
                BookmarkUtils.getLastUsedUrl(mActivityTestRule.getActivity()));
        Assert.assertEquals("Bookmarks", toolbar.getTitle());

        // When opening "Mobile bookmarks", we should come back to it when within the same session.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> delegate.openFolder(mBookmarkModel.getMobileFolderId()));
        Assert.assertEquals("Mobile bookmarks", toolbar.getTitle());
        Assert.assertEquals(SelectableListToolbar.NAVIGATION_BUTTON_BACK,
                toolbar.getNavigationButtonForTests());
        Assert.assertFalse(toolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        // Close bookmarks.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> toolbar.onMenuItemClick(toolbar.getMenu().findItem(R.id.close_menu_id)));
        ApplicationTestUtils.waitForActivityState(mBookmarkActivity, Stage.DESTROYED);

        // Reopen and make sure we're back in "Mobile bookmarks".
        Assert.assertEquals(BookmarkUIState.STATE_FOLDER, delegate.getCurrentState());
        Assert.assertEquals("chrome-native://bookmarks/folder/3",
                BookmarkUtils.getLastUsedUrl(mActivityTestRule.getActivity()));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.READ_LATER})
    public void testReadingListItemMenuItems() throws Exception {
        addReadingListBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.openFolder(mBookmarkModel.getRootFolderId()));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // Open the "Reading list" folder.
        onView(withText("Reading list")).perform(click());
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // Open the three-dot menu and verify the menu options being shown.
        View readingListItem = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        View more = readingListItem.findViewById(R.id.more);

        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        onView(withText("Select")).check(matches(isDisplayed()));
        onView(withText("Edit")).check(doesNotExist());
        onView(withText("Delete")).check(matches(isDisplayed()));
        onView(withText("Mark as read")).check(matches(isDisplayed()));
        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());

        // Click "Mark as read". The page should be moved to read section.
        onView(withText("Mark as read")).perform(click());
        onView(withText("Read")).check(matches(isDisplayed()));

        // A read page should not have "Mark as read" menu option.
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Select")).check(matches(isDisplayed()));
        onView(withText("Edit")).check(doesNotExist());
        onView(withText("Delete")).check(matches(isDisplayed()));
        onView(withText("Mark as read")).check(doesNotExist());
        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1231219")
    @Features.EnableFeatures({ChromeFeatureList.READ_LATER})
    public void testSearchReadingList_Deletion() throws Exception {
        addReadingListBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        CriteriaHelper.pollUiThread(() -> mBookmarkModel.getReadingListItem(mTestUrlA) != null);

        // Open the "Reading list" folder.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.openFolder(mBookmarkModel.getRootFolderId()));
        onView(withText("Reading list")).perform(click());
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // Enter search UI, but don't enter any search key word.
        TestThreadUtils.runOnUiThreadBlocking(mManager::openSearchUI);
        Assert.assertEquals("Wrong state, should be searching", BookmarkUIState.STATE_SEARCHING,
                mManager.getCurrentState());
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // Delete the reading list page in search state.
        View readingListItem = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        View more = readingListItem.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Delete")).check(matches(isDisplayed())).perform(click());
        CriteriaHelper.pollUiThread(() -> mBookmarkModel.getReadingListItem(mTestUrlA) == null);
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testReadingListEmptyView() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();

        BookmarkTestUtil.waitForBookmarkModelLoaded();
        // Open the "Reading list" folder.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.openFolder(mBookmarkModel.getRootFolderId()));
        onView(withText("Reading list")).perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // We should see an empty view with reading list text.
        onView(withText(R.string.reading_list_empty_list_title)).check(matches(isDisplayed()));

        // Open other folders will show the default empty view text.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.openFolder(mBookmarkModel.getMobileFolderId()));
        onView(withText(R.string.bookmarks_folder_empty)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testReadingListOpenInCCT() throws Exception {
        addReadingListBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        CriteriaHelper.pollUiThread(() -> mBookmarkModel.getReadingListItem(mTestUrlA) != null);

        // Open the "Reading list" folder.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.openFolder(mBookmarkModel.getRootFolderId()));
        onView(withText("Reading list")).perform(click());
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        View readingListRow = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("The 2nd view should be reading list.", BookmarkType.READING_LIST,
                getReorderAdapter().getIdByPosition(1).getType());

        // Click a reading list item, the page should be opened in a CCT.
        CustomTabActivity activity = ApplicationTestUtils.waitForActivityWithClass(
                CustomTabActivity.class, Stage.CREATED, () -> { readingListRow.performClick(); });
        CriteriaHelper.pollUiThread(() -> activity.getActivityTab() != null);
        Intent customTabIntent = activity.getInitialIntent();
        Assert.assertFalse(customTabIntent.hasExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertTrue(activity.getActivityTab().getUrl().equals(mTestUrlA)); });
        pressBack();
        onView(withText("Reading list")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.READ_LATER + ":use_cct/false"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @DisabledTest(message = "crbug.com/1369307")
    public void testReadingListOpenInRegularTab() throws Exception {
        addReadingListBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        CriteriaHelper.pollUiThread(() -> mBookmarkModel.getReadingListItem(mTestUrlA) != null);

        // Open the "Reading list" folder.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.openFolder(mBookmarkModel.getRootFolderId()));
        onView(withText("Reading list")).perform(click());
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        View readingListRow = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("The 2nd view should be reading list.", BookmarkType.READING_LIST,
                getReorderAdapter().getIdByPosition(1).getType());
        TestThreadUtils.runOnUiThreadBlocking(() -> TouchCommon.singleClickView(readingListRow));

        ChromeTabbedActivity activity = BookmarkTestUtil.waitForTabbedActivity();
        CriteriaHelper.pollUiThread(() -> {
            Tab activityTab = activity.getActivityTab();
            Criteria.checkThat(activityTab, Matchers.notNullValue());
            Criteria.checkThat(activityTab.getUrl(), Matchers.notNullValue());
            Criteria.checkThat(activityTab.getUrl(), Matchers.is(mTestUrlA));
            Criteria.checkThat(activityTab.isIncognito(), Matchers.is(false));
        });
        pressBack();
        onView(withText("Reading list")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.READ_LATER + ":use_cct/false"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    @DisabledTest(message = "crbug.com/1369307")
    public void testReadingListOpenInIncognitoTab() throws Exception {
        addReadingListBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        mActivityTestRule.loadUrlInNewTab("about:blank", /*incognito=*/true);
        openBookmarkManager();
        CriteriaHelper.pollUiThread(() -> mBookmarkModel.getReadingListItem(mTestUrlA) != null);

        // Open the "Reading list" folder.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.openFolder(mBookmarkModel.getRootFolderId()));
        onView(withText("Reading list")).perform(click());
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        View readingListRow = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("The 2nd view should be reading list.", BookmarkType.READING_LIST,
                getIdByPosition(1).getType());
        TestThreadUtils.runOnUiThreadBlocking(() -> TouchCommon.singleClickView(readingListRow));

        ChromeTabbedActivity activity = BookmarkTestUtil.waitForTabbedActivity();
        CriteriaHelper.pollUiThread(() -> {
            Tab activityTab = activity.getActivityTab();
            Criteria.checkThat(activityTab, Matchers.notNullValue());
            Criteria.checkThat(activityTab.getUrl(), Matchers.notNullValue());
            Criteria.checkThat(activityTab.getUrl(), Matchers.is(mTestUrlA));
            Criteria.checkThat(activityTab.isIncognito(), Matchers.is(true));
        });
        pressBack();
        onView(withText("Reading list")).check(matches(isDisplayed()));
    }

    /**
     * Verifies the top level elements with the reading list folder.
     * Layout:
     *  - Reading list folder.
     *  - Divider
     *  - Mobile bookmark folder.
     */
    @Test
    @SmallTest
    @Features.DisableFeatures({ChromeFeatureList.SHOPPING_LIST})
    public void testReadingListFolderShown() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.openFolder(mBookmarkModel.getRootFolderId()));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // Reading list should show in the root folder.
        View readingListRow = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        Assert.assertEquals("No overflow menu for reading list folder.", View.GONE,
                readingListRow.findViewById(R.id.more).getVisibility());
        Assert.assertEquals("The 1st view should be reading list.", BookmarkType.READING_LIST,
                getReorderAdapter().getIdByPosition(0).getType());
        onView(withText("Reading list")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testReadingListFolderShownOneUnreadPage() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        addReadingListBookmark("a", new GURL("https://a.com/reading_list_0"));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mManager.openFolder(mBookmarkModel.getRootFolderId()); });
        onView(withText("Reading list")).check(matches(isDisplayed()));
        onView(withText("1 unread page")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testReadingListFolderShownMultipleUnreadPages() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        addReadingListBookmark("a", new GURL("https://a.com/reading_list_0"));
        addReadingListBookmark("b", new GURL("https://a.com/reading_list_1"));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mManager.openFolder(mBookmarkModel.getRootFolderId()); });
        onView(withText("Reading list")).check(matches(isDisplayed()));
        onView(withText("2 unread pages")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testShowBookmarkManagerReadingListPage() {
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.loadEmptyPartnerBookmarkShimForTesting());
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        if (mActivityTestRule.getActivity().isTablet()) {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> ReadingListUtils.showReadingList(/*isIncognito=*/false));
        } else {
            mBookmarkActivity = ApplicationTestUtils.waitForActivityWithClass(
                    BookmarkActivity.class, Stage.CREATED,
                    () -> ReadingListUtils.showReadingList(/*isIncognito=*/false));
        }

        onView(withText("Reading list")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Features.
    EnableFeatures({ChromeFeatureList.READ_LATER + ":add_to_reading_list_in_app_menu/true"})
    public void testAddToReadingListFromAppMenu() throws Exception {
        mActivityTestRule.loadUrl(mTestPage);

        // Click "Add to Reading List" to add the current tab.
        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(), R.id.add_to_reading_list_menu_id);
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        CriteriaHelper.pollUiThread(() -> mBookmarkModel.getReadingListItem(mTestPage) != null);

        // All actions with BookmarkModel needs to run on UI thread.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkItem item = mBookmarkModel.getReadingListItem(mTestPage);
            Assert.assertEquals(BookmarkType.READING_LIST, item.getId().getType());
            Assert.assertEquals(mTestPage, item.getUrl());
            Assert.assertEquals(TEST_PAGE_TITLE_GOOGLE, item.getTitle());
        });

        BookmarkTestUtil.waitForOfflinePageSaved(mTestPage);
    }

    @Test
    @SmallTest
    public void testReadingListItemsInSelectionMode() throws Exception {
        addReadingListBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        BookmarkPromoHeader.forcePromoStateForTests(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.openFolder(mBookmarkModel.getRootFolderId()));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // Open the "Reading list" folder.
        onView(withText("Reading list")).perform(click());
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // Select a reading list item. Verify the toolbar menu buttons being shown.
        BookmarkRow bookmarkRow =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        TouchCommon.longPressView(bookmarkRow);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        BookmarkActionBar toolbar = mManager.getToolbarForTests();
        Assert.assertFalse("Read later items shouldn't have move option",
                toolbar.getMenu().findItem(R.id.selection_mode_move_menu_id).isVisible());
        Assert.assertFalse("Read later items shouldn't have edit option",
                toolbar.getMenu().findItem(R.id.selection_mode_edit_menu_id).isVisible());
        Assert.assertTrue("Read later items should have delete option",
                toolbar.getMenu().findItem(R.id.selection_mode_delete_menu_id).isVisible());
    }
}
