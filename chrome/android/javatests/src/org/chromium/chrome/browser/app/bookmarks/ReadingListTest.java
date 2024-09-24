// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.doReturn;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.view.MenuItem;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Description;
import org.hamcrest.Matchers;
import org.hamcrest.TypeSafeMatcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.bookmarks.BookmarkPromoHeader;
import org.chromium.chrome.browser.bookmarks.BookmarkToolbar;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.TestingDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;

/** Tests for the reading list in the bookmark manager. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures({
    SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE,
    // TODO(crbug.com/344981899): ReplaceSyncPromosWithSigninPromos is disabled because bookmarks
    // account storage is disabled above, otherwise tests run into assertion failures. Long term,
    // these tests probably need to be fixed for the bookmarks account storage case rather than
    // force-disable the feature.
    ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
})
@DoNotBatch(reason = "BookmarkTest has behaviours and thus can't be batched.")
public class ReadingListTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String TEST_PAGE_TITLE_GOOGLE = "The Google";
    private static final int TEST_PORT = 12345;

    private BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private BookmarkModel mBookmarkModel;
    private RecyclerView mItemsContainer;
    // Constant but can only be initialized after parameterized test runner setup because this would
    // trigger native load / CommandLineFlag setup.
    private GURL mTestUrlA;
    private EmbeddedTestServer mTestServer;
    private @Nullable BookmarkActivity mBookmarkActivity;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel = mActivityTestRule.getActivity().getBookmarkModelForTesting();
                });
        // Use a custom port so the links are consistent for render tests.
        mActivityTestRule.getEmbeddedTestServerRule().setServerPort(TEST_PORT);
        mTestServer = mActivityTestRule.getTestServer();
        mTestUrlA = new GURL("http://a.com");
    }

    @After
    public void tearDown() throws Exception {
        if (mBookmarkActivity != null) ApplicationTestUtils.finishActivity(mBookmarkActivity);
    }

    private void openBookmarkManager() throws InterruptedException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        if (mActivityTestRule.getActivity().isTablet()) {
            mActivityTestRule.loadUrl(UrlConstants.BOOKMARKS_URL);
            mItemsContainer =
                    mActivityTestRule
                            .getActivity()
                            .findViewById(R.id.selectable_list_recycler_view);
            mItemsContainer.setItemAnimator(null); // Disable animation to reduce flakiness.
            mBookmarkManagerCoordinator =
                    ((BookmarkPage)
                                    mActivityTestRule
                                            .getActivity()
                                            .getActivityTab()
                                            .getNativePage())
                            .getManagerForTesting();
        } else {
            // phone
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
                () -> AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(false));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
    }

    void openRootFolder() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> getBookmarkDelegate().openFolder(mBookmarkModel.getRootFolderId()));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
    }

    void openReadingList() {
        onView(withText("Reading list")).perform(click());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private BookmarkId addReadingListBookmark(final String title, final GURL url)
            throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        BookmarkId bookmarkId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mBookmarkModel.addToDefaultReadingList(title, url));
        return bookmarkId;
    }

    private void setReadStatusForReadingList(BookmarkId id, boolean read) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.setReadStatusForReadingList(id, read));
    }

    private TestingDelegate getTestingDelegate() {
        return mBookmarkManagerCoordinator.getTestingDelegate();
    }

    private BookmarkId getIdByPosition(int pos) {
        return getTestingDelegate().getIdByPositionForTesting(pos);
    }

    private BookmarkDelegate getBookmarkDelegate() {
        return mBookmarkManagerCoordinator.getBookmarkDelegateForTesting();
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testOpenBookmarkManagerWhenDefaultToRootEnabled()
            throws InterruptedException, ExecutionException {
        openBookmarkManager();
        BookmarkDelegate delegate = getBookmarkDelegate();
        BookmarkToolbar toolbar = mBookmarkManagerCoordinator.getToolbarForTesting();

        // We should default to the root bookmark.
        Assert.assertEquals(BookmarkUiMode.FOLDER, delegate.getCurrentUiMode());
        Assert.assertEquals("chrome-native://bookmarks/folder/0", BookmarkUtils.getLastUsedUrl());
        Assert.assertEquals("Bookmarks", toolbar.getTitle());

        // When opening "Mobile bookmarks", we should come back to it when within the same session.
        ThreadUtils.runOnUiThreadBlocking(
                () -> delegate.openFolder(mBookmarkModel.getMobileFolderId()));
        Assert.assertEquals("Mobile bookmarks", toolbar.getTitle());
        Assert.assertEquals(
                SelectableListToolbar.NavigationButton.NORMAL_VIEW_BACK,
                toolbar.getNavigationButtonForTests());
        Assert.assertFalse(toolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        // Close bookmarks.
        ThreadUtils.runOnUiThreadBlocking(
                () -> toolbar.onMenuItemClick(toolbar.getMenu().findItem(R.id.close_menu_id)));
        ApplicationTestUtils.waitForActivityState(mBookmarkActivity, Stage.DESTROYED);

        // Reopen and make sure we're back in "Mobile bookmarks".
        Assert.assertEquals(BookmarkUiMode.FOLDER, delegate.getCurrentUiMode());
        Assert.assertEquals("chrome-native://bookmarks/folder/3", BookmarkUtils.getLastUsedUrl());
    }

    @Test
    @SmallTest
    public void testReadingListItemMenuItems() throws Exception {
        addReadingListBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);

        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        openRootFolder();
        openReadingList();

        // Open the three-dot menu and verify the menu options being shown.
        View readingListItem = mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        View more = readingListItem.findViewById(R.id.more);

        ThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        onView(withText("Select")).check(matches(isDisplayed()));
        onView(withText("Edit")).check(matches(isDisplayed()));
        onView(withText("Delete")).check(matches(isDisplayed()));
        onView(withText("Mark as read")).check(matches(isDisplayed()));
        onView(withText("Mark as unread")).check(doesNotExist());
        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testReadingListItemMenuItems_ReadItem() throws Exception {
        BookmarkId id = addReadingListBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.setReadStatusForReadingList(id, /* read= */ true);
                });

        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        openRootFolder();
        openReadingList();

        // Open the three-dot menu and verify the menu options being shown.
        View readingListItem = mItemsContainer.findViewHolderForAdapterPosition(3).itemView;
        View more = readingListItem.findViewById(R.id.more);

        ThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        onView(withText("Select")).check(matches(isDisplayed()));
        onView(withText("Edit")).check(matches(isDisplayed()));
        onView(withText("Delete")).check(matches(isDisplayed()));
        onView(withText("Mark as read")).check(doesNotExist());
        onView(withText("Mark as unread")).check(matches(isDisplayed()));
        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testSearchReadingList_Deletion() throws Exception {
        addReadingListBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);

        openBookmarkManager();
        openRootFolder();
        openReadingList();

        // Enter search UI, but don't enter any search key word.
        ThreadUtils.runOnUiThreadBlocking(getBookmarkDelegate()::openSearchUi);
        Assert.assertEquals(
                "Wrong state, should be searching",
                BookmarkUiMode.SEARCHING,
                getBookmarkDelegate().getCurrentUiMode());

        runOnUiThreadBlocking(
                () ->
                        mBookmarkManagerCoordinator
                                .getTestingDelegate()
                                .searchForTesting(TEST_PAGE_TITLE_GOOGLE));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // Delete the reading list page in search state.
        View readingListItem = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        View more = readingListItem.findViewById(R.id.more);
        ThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Delete")).check(matches(isDisplayed())).perform(click());
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testReadingListEmptyStateView() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        openRootFolder();
        openReadingList();

        // We should see an empty view with reading list text.
        onView(withId(R.id.empty_state_icon)).check(matches(isDisplayed()));
        onView(withText(R.string.reading_list_manager_empty_state)).check(matches(isDisplayed()));
        onView(withText(R.string.reading_list_manager_save_page_to_read_later))
                .check(matches(isDisplayed()));

        // Open other folders will show the default empty view text.
        ThreadUtils.runOnUiThreadBlocking(
                () -> getBookmarkDelegate().openFolder(mBookmarkModel.getMobileFolderId()));
        onView(withId(R.id.empty_state_icon)).check(matches(isDisplayed()));
        onView(withText(R.string.bookmark_manager_empty_state)).check(matches(isDisplayed()));
        onView(withText(R.string.bookmark_manager_back_to_page_by_adding_bookmark))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testReadingListOpenInRegularTab() throws Exception {
        addReadingListBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);

        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        openRootFolder();
        openReadingList();

        View readingListRow = mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        Assert.assertEquals(
                "The 2nd view should be reading list.",
                BookmarkType.READING_LIST,
                getIdByPosition(2).getType());
        ThreadUtils.runOnUiThreadBlocking(() -> TouchCommon.singleClickView(readingListRow));

        ChromeTabbedActivity activity = BookmarkTestUtil.waitForTabbedActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activityTab = activity.getActivityTab();
                    Criteria.checkThat(activityTab, Matchers.notNullValue());
                    Criteria.checkThat(activityTab.getUrl(), Matchers.notNullValue());
                    Criteria.checkThat(activityTab.getUrl(), Matchers.is(mTestUrlA));
                    Criteria.checkThat(activityTab.isIncognito(), Matchers.is(false));
                });
        pressBack();
        BookmarkTestUtil.waitForBookmarkActivity();

        onView(withText("Reading list")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testReadingListOpenInIncognitoTab() throws Exception {
        addReadingListBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);

        mActivityTestRule.loadUrlInNewTab(UrlConstants.NTP_NON_NATIVE_URL, /* incognito= */ true);

        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        openRootFolder();
        openReadingList();

        View readingListRow = mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        Assert.assertEquals(
                "The 2nd view should be reading list.",
                BookmarkType.READING_LIST,
                getIdByPosition(2).getType());
        ThreadUtils.runOnUiThreadBlocking(() -> TouchCommon.singleClickView(readingListRow));

        ChromeTabbedActivity activity = BookmarkTestUtil.waitForTabbedActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activityTab = activity.getActivityTab();
                    Criteria.checkThat(activityTab, Matchers.notNullValue());
                    Criteria.checkThat(activityTab.getUrl(), Matchers.notNullValue());
                    Criteria.checkThat(activityTab.getUrl(), Matchers.is(mTestUrlA));
                    Criteria.checkThat(activityTab.isIncognito(), Matchers.is(true));
                });
        pressBack();
        BookmarkTestUtil.waitForBookmarkActivity();

        onView(withText("Reading list")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testReadingListFolderShown() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        openRootFolder();

        // Reading list should show in the root folder.
        onView(withText("Reading list")).check(matches(isDisplayed()));
        onView(withText("Reading list"))
                .check(
                        matches(
                                new TypeSafeMatcher<View>() {
                                    @Override
                                    public void describeTo(Description description) {
                                        description.appendText(
                                                "No overflow menu for reading list folder");
                                    }

                                    @Override
                                    public boolean matchesSafely(View view) {
                                        return ((View) view.getParent().getParent().getParent())
                                                        .findViewById(R.id.more)
                                                        .getVisibility()
                                                == View.GONE;
                                    }
                                }));
    }

    @Test
    @SmallTest
    public void testReadingListFolderShownOneUnreadPage() throws Exception {
        // Add two reading list items and set one as read.
        setReadStatusForReadingList(
                addReadingListBookmark("a", new GURL("https://a.com/reading_list_0")), true);
        addReadingListBookmark("b", new GURL("https://b.com/reading_list_0"));

        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkManagerCoordinator
                            .getBookmarkUiPrefsForTesting()
                            .setBookmarkRowDisplayPref(BookmarkRowDisplayPref.COMPACT);
                });
        openRootFolder();

        onView(withText("Reading list (1)")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testReadingListFolderShownMultipleUnreadPages() throws Exception {
        // Add three reading list items and set one as read.
        setReadStatusForReadingList(
                addReadingListBookmark("a", new GURL("https://a.com/reading_list_0")), true);
        addReadingListBookmark("b", new GURL("https://b.com/reading_list_1"));
        addReadingListBookmark("c", new GURL("https://c.com/reading_list_1"));

        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkManagerCoordinator
                            .getBookmarkUiPrefsForTesting()
                            .setBookmarkRowDisplayPref(BookmarkRowDisplayPref.COMPACT);
                });
        openRootFolder();

        onView(withText("Reading list (2)")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testReadingListFolderShown_SetReadingListStatus() throws Exception {
        // Add three reading list items and set one as read.
        BookmarkId id1 = addReadingListBookmark("a", new GURL("https://a.com/reading_list_0"));
        BookmarkId id2 = addReadingListBookmark("b", new GURL("https://a.com/reading_list_1"));

        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkManagerCoordinator
                            .getBookmarkUiPrefsForTesting()
                            .setBookmarkRowDisplayPref(BookmarkRowDisplayPref.COMPACT);
                });
        openRootFolder();
        onView(withText("Reading list (2)")).check(matches(isDisplayed()));

        setReadStatusForReadingList(id1, true);
        onView(withText("Reading list (1)")).check(matches(isDisplayed()));

        setReadStatusForReadingList(id2, true);
        onView(withText("Reading list (0)")).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testReadingListItemsInSelectionMode() throws Exception {
        addReadingListBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);

        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        openRootFolder();
        openReadingList();

        // Select a reading list item. Verify the toolbar menu buttons being shown.
        onView(withText(TEST_PAGE_TITLE_GOOGLE)).perform(longClick());

        BookmarkToolbar toolbar = mBookmarkManagerCoordinator.getToolbarForTesting();
        Assert.assertTrue(
                "Read later items should have move option",
                toolbar.getMenu().findItem(R.id.selection_mode_move_menu_id).isVisible());
        Assert.assertTrue(
                "Read later items should have edit option",
                toolbar.getMenu().findItem(R.id.selection_mode_edit_menu_id).isVisible());
        Assert.assertTrue(
                "Read later items should have delete option",
                toolbar.getMenu().findItem(R.id.selection_mode_delete_menu_id).isVisible());
        Assert.assertTrue(
                "Read later items should have mark as read",
                toolbar.getMenu().findItem(R.id.reading_list_mark_as_read_id).isVisible());

        MenuItem mockMenuItem = Mockito.mock(MenuItem.class);
        doReturn(R.id.reading_list_mark_as_read_id).when(mockMenuItem).getItemId();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    toolbar.onMenuItemClick(mockMenuItem);
                });

        Assert.assertFalse(
                "Selection menu should be hidden after a click.",
                toolbar.getMenu().findItem(R.id.selection_mode_move_menu_id).isVisible());
        Assert.assertFalse(
                "Selection menu should be hidden after a click.",
                toolbar.getMenu().findItem(R.id.selection_mode_edit_menu_id).isVisible());
        Assert.assertFalse(
                "Selection menu should be hidden after a click.",
                toolbar.getMenu().findItem(R.id.selection_mode_delete_menu_id).isVisible());
        Assert.assertFalse(
                "Selection menu should be hidden after a click.",
                toolbar.getMenu().findItem(R.id.reading_list_mark_as_read_id).isVisible());
    }

    @Test
    @SmallTest
    public void testReadingListItemsInSelectionMode_SearchMode() throws Exception {
        addReadingListBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);

        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        openRootFolder();
        openReadingList();

        ThreadUtils.runOnUiThreadBlocking(getBookmarkDelegate()::openSearchUi);

        BookmarkToolbar toolbar = mBookmarkManagerCoordinator.getToolbarForTesting();
        Assert.assertFalse(
                "Menu items shouldn't be visible in search.",
                toolbar.getMenu().findItem(R.id.selection_mode_move_menu_id).isVisible());
        Assert.assertFalse(
                "Menu items shouldn't be visible in search.",
                toolbar.getMenu().findItem(R.id.selection_mode_edit_menu_id).isVisible());
        Assert.assertFalse(
                "Menu items shouldn't be visible in search.",
                toolbar.getMenu().findItem(R.id.selection_mode_delete_menu_id).isVisible());
        Assert.assertFalse(
                "Menu items shouldn't be visible in search.",
                toolbar.getMenu().findItem(R.id.reading_list_mark_as_read_id).isVisible());
    }
}
