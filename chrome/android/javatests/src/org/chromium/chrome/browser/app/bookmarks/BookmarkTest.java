// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.verify;

import static org.chromium.components.browser_ui.widget.highlight.ViewHighlighterTestUtils.checkHighlightOff;
import static org.chromium.components.browser_ui.widget.highlight.ViewHighlighterTestUtils.checkHighlightPulse;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlockingNoException;

import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.bookmarks.BookmarkDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkFolderRow;
import org.chromium.chrome.browser.bookmarks.BookmarkItemRow;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerCoordinator;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkPage;
import org.chromium.chrome.browser.bookmarks.BookmarkPromoHeader;
import org.chromium.chrome.browser.bookmarks.BookmarkRow;
import org.chromium.chrome.browser.bookmarks.BookmarkToolbar;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkShoppingItemRow;
import org.chromium.chrome.browser.bookmarks.TestingDelegate;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.NumberRollView;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.ViewType;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncService.SyncStateChangedListener;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;

/** Tests for the bookmark manager. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
// TODO(1406059): Disabling the shopping CPA should not be a requirement for these tests.
@DisableFeatures({
    ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING,
    ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS
})
// TODO(crbug.com/1426138): Investigate batching.
@DoNotBatch(reason = "BookmarkTest has behaviours and thus can't be batched.")
public class BookmarkTest {
    private static final String TEST_PAGE_URL_GOOGLE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE_TITLE_GOOGLE = "The Google";
    private static final String TEST_PAGE_TITLE_GOOGLE2 = "Google";
    private static final String TEST_PAGE_URL_FOO = "/chrome/test/data/android/test.html";
    private static final String TEST_PAGE_TITLE_FOO = "Foo";
    private static final String TEST_FOLDER_TITLE = "Test folder";
    private static final String TEST_FOLDER_TITLE2 = "Test folder 2";
    private static final String TEST_TITLE_A = "a";
    private static final int TEST_PORT = 12345;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SyncService mSyncService;
    @Captor private ArgumentCaptor<SyncStateChangedListener> mSyncStateChangedListenerCaptor;

    private BookmarkModel mBookmarkModel;
    // Constant but can only be initialized after parameterized test runner setup because this would
    // trigger native load / CommandLineFlag setup.
    private GURL mTestUrlA;
    private GURL mTestPage;
    private GURL mTestPageFoo;
    private EmbeddedTestServer mTestServer;

    // Page/Activity specific, set/updated when bookmarks UI is opened.
    private @Nullable BookmarkActivity mBookmarkActivity;
    private BookmarkManagerCoordinator mBookmarkManagerCoordinator;
    private RecyclerView mItemsContainer;
    private BookmarkDelegate mDelegate;
    private DragReorderableRecyclerViewAdapter mAdapter;
    private BookmarkToolbar mToolbar;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        ShoppingFeatures.setShoppingListEligibleForTesting(false);
        mActivityTestRule.startMainActivityOnBlankPage();
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel = mActivityTestRule.getActivity().getBookmarkModelForTesting();
                    SyncServiceFactory.setInstanceForTesting(mSyncService);
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
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @SmallTest
    public void testAddBookmark() throws Exception {
        mActivityTestRule.loadUrl(mTestPage);
        // Check partner bookmarks are lazily loaded.
        assertFalse(mBookmarkModel.isBookmarkModelLoaded());

        // Click star button to bookmark the current tab.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                R.id.bookmark_this_page_id);
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        // All actions with BookmarkModel needs to run on UI thread.
        runOnUiThreadBlocking(
                () -> {
                    BookmarkId id =
                            mBookmarkModel.getUserBookmarkIdForTab(
                                    mActivityTestRule.getActivity().getActivityTabProvider().get());
                    assertTrue(
                            "The test page is not added as bookmark: ",
                            mBookmarkModel.doesBookmarkExist(id));
                    BookmarkItem item = mBookmarkModel.getBookmarkById(id);
                    assertEquals(mBookmarkModel.getDefaultFolder(), item.getParentId());
                    assertEquals(mTestPage, item.getUrl());
                    assertEquals(TEST_PAGE_TITLE_GOOGLE, item.getTitle());
                });

        BookmarkTestUtil.waitForOfflinePageSaved(mTestPage);

        // Click the star button again to launch the edit activity.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                R.id.bookmark_this_page_id);
        BookmarkTestUtil.waitForEditActivity().finish();
    }

    @Test
    @SmallTest
    public void testAddBookmarkToOtherFolder() {
        mActivityTestRule.loadUrl(mTestPage);
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        // Set default folder as "Other Folder".
        runOnUiThreadBlocking(
                () -> {
                    ChromeSharedPreferences.getInstance()
                            .writeString(
                                    ChromePreferenceKeys.BOOKMARKS_LAST_USED_PARENT,
                                    mBookmarkModel.getOtherFolderId().toString());
                });
        // Click star button to bookmark the current tab.
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                R.id.bookmark_this_page_id);
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        // All actions with BookmarkModel needs to run on UI thread.
        runOnUiThreadBlocking(
                () -> {
                    BookmarkId id =
                            mBookmarkModel.getUserBookmarkIdForTab(
                                    mActivityTestRule.getActivity().getActivityTabProvider().get());
                    assertTrue(
                            "The test page is not added as bookmark: ",
                            mBookmarkModel.doesBookmarkExist(id));
                    BookmarkItem item = mBookmarkModel.getBookmarkById(id);
                    assertEquals(
                            "Bookmark added in a wrong default folder.",
                            mBookmarkModel.getOtherFolderId(),
                            item.getParentId());
                });
    }

    @Test
    @SmallTest
    public void testOpenBookmark() throws InterruptedException, ExecutionException {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        assertTrue(
                "Grid view does not contain added bookmark: ",
                isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
        final View title = getViewWithText(mItemsContainer, TEST_PAGE_TITLE_GOOGLE);
        runOnUiThreadBlocking(() -> TouchCommon.singleClickView(title));
        ChromeTabbedActivity activity = BookmarkTestUtil.waitForTabbedActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activityTab = activity.getActivityTab();
                    Criteria.checkThat(activityTab, Matchers.notNullValue());
                    Criteria.checkThat(activityTab.getUrl(), Matchers.notNullValue());
                    Criteria.checkThat(activityTab.getUrl(), is(mTestPage));
                });
    }

    @Test
    @SmallTest
    public void testUrlComposition() {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        runOnUiThreadBlocking(
                () -> {
                    BookmarkId mobileId = mBookmarkModel.getMobileFolderId();
                    BookmarkId bookmarkBarId = mBookmarkModel.getDesktopFolderId();
                    BookmarkId otherId = mBookmarkModel.getOtherFolderId();
                    assertEquals(
                            "chrome-native://bookmarks/folder/" + mobileId,
                            BookmarkUiState.createFolderUrl(mobileId).toString());
                    assertEquals(
                            "chrome-native://bookmarks/folder/" + bookmarkBarId,
                            BookmarkUiState.createFolderUrl(bookmarkBarId).toString());
                    assertEquals(
                            "chrome-native://bookmarks/folder/" + otherId,
                            BookmarkUiState.createFolderUrl(otherId).toString());
                });
    }

    @Test
    @SmallTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testShowBookmarkManager_Phone() throws InterruptedException {
        BookmarkTestUtil.loadEmptyPartnerBookmarksForTesting(mBookmarkModel);
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        runOnUiThreadBlocking(
                () -> {
                    BookmarkUtils.showBookmarkManager(
                            mActivityTestRule.getActivity(),
                            mBookmarkModel.getMobileFolderId(),
                            /* isIncognito= */ false);
                });

        BookmarkTestUtil.waitForBookmarkActivity();

        // Assign so it's cleaned up after the test.
        mBookmarkActivity = (BookmarkActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    @Test
    @SmallTest
    public void testOpenBookmarkManagerFolder() throws InterruptedException {
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        BookmarkTestUtil.waitForBookmarkModelLoaded();

        assertEquals(BookmarkUiMode.FOLDER, mDelegate.getCurrentUiMode());
        assertEquals(
                "chrome-native://bookmarks/folder/3",
                BookmarkUtils.getLastUsedUrl(mActivityTestRule.getActivity()));
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testFolderNavigation_Phone() throws InterruptedException, ExecutionException {
        BookmarkId testFolder = addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        // Check that we are in the mobile bookmarks folder.
        assertEquals("Mobile bookmarks", mToolbar.getTitle());
        assertEquals(NavigationButton.BACK, mToolbar.getNavigationButtonForTests());
        assertFalse(mToolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        // Open the new test folder.
        runOnUiThreadBlocking(() -> mDelegate.openFolder(testFolder));

        // Check that we are in the editable test folder.
        assertEquals(TEST_FOLDER_TITLE, mToolbar.getTitle());
        assertEquals(NavigationButton.BACK, mToolbar.getNavigationButtonForTests());
        assertTrue(mToolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        runOnUiThreadBlocking(
                () -> mBookmarkModel.setBookmarkTitle(testFolder, TEST_FOLDER_TITLE2));

        // Check that the test folder reflects name changes.
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mToolbar.getTitle(), equalTo(TEST_FOLDER_TITLE2)));

        // Call BookmarkToolbar#onClick() to activate the navigation button.
        runOnUiThreadBlocking(() -> mToolbar.onClick(mToolbar));

        // Check that we are back in the mobile folder
        assertEquals("Mobile bookmarks", mToolbar.getTitle());
        assertEquals(NavigationButton.BACK, mToolbar.getNavigationButtonForTests());
        assertFalse(mToolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        // Call BookmarkToolbar#onClick() to activate the navigation button.
        runOnUiThreadBlocking(() -> mToolbar.onClick(mToolbar));

        // Check that we are in the root folder.
        assertEquals("Bookmarks", mToolbar.getTitle());
        assertEquals(NavigationButton.NONE, mToolbar.getNavigationButtonForTests());
        assertFalse(mToolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());
    }

    // TODO(twellington): Write a folder navigation test for tablets that waits for the Tab hosting
    //                    the native page to update its url after navigations.

    @Test
    @MediumTest
    public void testSearchBookmarks() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        assertEquals(BookmarkUiMode.FOLDER, mDelegate.getCurrentUiMode());
        assertEquals("Wrong number of items before starting search.", 3, mAdapter.getItemCount());

        runOnUiThreadBlocking(mDelegate::openSearchUi);

        assertEquals(BookmarkUiMode.SEARCHING, mDelegate.getCurrentUiMode());
        assertEquals(
                "Wrong number of items after showing search UI. The promo should be hidden.",
                2,
                mAdapter.getItemCount());

        searchBookmarks("Google");
        assertEquals("Wrong number of items after searching.", 1, mAdapter.getItemCount());

        BookmarkId newBookmark = addBookmark(TEST_PAGE_TITLE_GOOGLE2, mTestPage);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Wrong number of items after bookmark added while searching.",
                            mAdapter.getItemCount(),
                            is(2));
                });

        removeBookmark(newBookmark);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Wrong number of items after bookmark removed while searching.",
                            mAdapter.getItemCount(),
                            is(1));
                });

        searchBookmarks("Non-existent page");
        assertEquals(
                "Wrong number of items after searching for non-existent item.",
                0,
                mAdapter.getItemCount());

        runOnUiThreadBlocking(() -> mToolbar.hideSearchView());
        assertEquals("Wrong number of items after closing search UI.", 3, mAdapter.getItemCount());
        assertEquals(BookmarkUiMode.FOLDER, mDelegate.getCurrentUiMode());
        assertEquals(TEST_FOLDER_TITLE, mToolbar.getTitle());
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testSearchBookmarks_pressBack() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        BookmarkId googleId = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        assertEquals(
                Boolean.TRUE,
                mBookmarkManagerCoordinator.getHandleBackPressChangedSupplier().get());

        runOnUiThreadBlocking(mDelegate::openSearchUi);

        assertEquals(BookmarkUiMode.SEARCHING, mDelegate.getCurrentUiMode());
        assertEquals(
                "Wrong number of items after showing search UI. The promo should be hidden.",
                2,
                mAdapter.getItemCount());

        assertEquals(
                Boolean.TRUE,
                mBookmarkManagerCoordinator.getHandleBackPressChangedSupplier().get());

        // Exit search UI.
        pressBackButton();
        assertNotEquals(BookmarkUiMode.SEARCHING, mDelegate.getCurrentUiMode());

        // Enter search UI again.
        runOnUiThreadBlocking(mDelegate::openSearchUi);

        searchBookmarks("Google");
        assertEquals("Wrong number of items after searching.", 1, mAdapter.getItemCount());

        BookmarkRow itemView = getBookmarkItemRow(0);
        assertEquals(googleId, itemView.getItem());

        toggleSelectionThroughMoreMenu(0);

        // Make sure the Item "test" is selected.
        CriteriaHelper.pollUiThread(
                itemView::isChecked, "Expected item \"test\" to become selected");

        pressBackButton();

        // Clear selection but still in search UI.
        CriteriaHelper.pollUiThread(
                () -> !itemView.isChecked(), "Expected item \"test\" to become not selected");
        assertEquals(BookmarkUiMode.SEARCHING, mDelegate.getCurrentUiMode());
        assertEquals(
                Boolean.TRUE,
                mBookmarkManagerCoordinator.getHandleBackPressChangedSupplier().get());

        // Exit search UI.
        pressBackButton();
        assertEquals(BookmarkUiMode.FOLDER, mDelegate.getCurrentUiMode());

        // Exit folder.
        assertEquals(
                Boolean.TRUE,
                mBookmarkManagerCoordinator.getHandleBackPressChangedSupplier().get());
        pressBackButton();
        assertEquals(BookmarkUiMode.FOLDER, mDelegate.getCurrentUiMode());

        // Exit bookmark activity.
        assertEquals(
                Boolean.FALSE,
                mBookmarkManagerCoordinator.getHandleBackPressChangedSupplier().get());
        runOnUiThreadBlocking(mBookmarkActivity.getOnBackPressedDispatcher()::onBackPressed);
        ApplicationTestUtils.waitForActivityState(mBookmarkActivity, Stage.DESTROYED);
    }

    @Test
    @MediumTest
    public void testSearchBookmarks_Delete_FromInitialQuery() throws Exception {
        // Inspired by https://crbug.com/1434600. Selected item deletion happens during the initial
        // query that's still showing the folder's children.
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        BookmarkId testFolder = addFolder(TEST_FOLDER_TITLE);
        BookmarkId testFolder2 = addFolder(TEST_FOLDER_TITLE2);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, testFolder);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, testFolder2);

        openBookmarkManager();
        openFolder(testFolder);

        View search_view = mToolbar.findViewById(R.id.search_view);
        clickToolbarMenuItem(R.id.search_menu_id);

        // Despite being in search mode, this is the initial query state, and the previous 1
        // bookmark inside of testFolder should be shown.
        assertEquals(search_view.getVisibility(), View.VISIBLE);
        assertEquals(1, mAdapter.getItemCount());

        toggleSelectionThroughMoreMenu(0);

        // Selecting an item should cause the search view to be hidden, replaced with menu items
        // that operate on the selected rows.
        assertNotEquals(search_view.getVisibility(), View.VISIBLE);

        clickToolbarMenuItem(R.id.selection_mode_delete_menu_id);

        // Should now be kicked back into an empty search string query, not the initial query. This
        // is why 3 items should now be visible, the two folders and the other url bookmark.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(search_view.getVisibility(), is(View.VISIBLE));
                    Criteria.checkThat(mAdapter.getItemCount(), is(3));
                });
    }

    @Test
    @MediumTest
    public void testSearchBookmarks_Delete() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        BookmarkId testFolder = addFolder(TEST_FOLDER_TITLE);
        addFolder(TEST_FOLDER_TITLE2, testFolder);
        BookmarkId testBookmark = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, testFolder);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, testFolder);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(testFolder);

        assertEquals(
                "Wrong state, should be in folder",
                BookmarkUiMode.FOLDER,
                mDelegate.getCurrentUiMode());
        assertEquals("Wrong number of items before starting search.", 3, mAdapter.getItemCount());

        // Start searching without entering a query. This won't change the items displayed which
        // are currently testFolder's children (3).
        runOnUiThreadBlocking(mDelegate::openSearchUi);
        assertEquals(
                "Wrong state, should be searching",
                BookmarkUiMode.SEARCHING,
                mDelegate.getCurrentUiMode());
        assertEquals("Wrong number of items before starting search.", 3, mAdapter.getItemCount());

        // Select testFolder2 and delete it. This deletion will refresh the current search, which
        // right now is the empty string. This will return all bookmarks (3).
        toggleSelectionThroughMoreMenu(2);
        clickToolbarMenuItem(R.id.selection_mode_delete_menu_id);

        // Should still be searching with the folder gone.
        assertEquals("Wrong number of items.", 3, mAdapter.getItemCount());

        // Start searching, enter a query. This query will match all remaining bookmarks (1).
        searchBookmarks("Google");
        assertEquals("Wrong number of items after searching.", 1, mAdapter.getItemCount());

        // Remove the bookmark.
        removeBookmark(testBookmark);

        // The user should still be searching, and the bookmark should be gone. We're refreshing
        // the search query again here, but in this case it's now "Google".
        pollForModeAndCount(BookmarkUiMode.SEARCHING, 0);

        // Undo the deletion.
        runOnUiThreadBlocking(
                () -> mBookmarkManagerCoordinator.getUndoControllerForTesting().onAction(null));

        // The user should still be searching, and the bookmark should reappear. Refreshing the
        // search yet again, now with the "Google" search matching returning 1 result.
        pollForModeAndCount(BookmarkUiMode.SEARCHING, 1);
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.SHOPPING_LIST})
    public void testSearchBookmarks_DeleteFolderWithChildrenInResults() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        BookmarkId testFolder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, testFolder);
        openBookmarkManager();

        // Start searching, enter a query.
        runOnUiThreadBlocking(mDelegate::openSearchUi);
        assertEquals(
                "Wrong state, should be searching",
                BookmarkUiMode.SEARCHING,
                mDelegate.getCurrentUiMode());
        searchBookmarks("test");
        assertEquals("Wrong number of items after searching.", 2, mAdapter.getItemCount());

        // Remove the bookmark.
        removeBookmark(testFolder);

        // The user should still be searching, and the bookmark should be gone.
        pollForModeAndCount(BookmarkUiMode.SEARCHING, 0);

        // Undo the deletion.
        runOnUiThreadBlocking(
                () -> mBookmarkManagerCoordinator.getUndoControllerForTesting().onAction(null));

        // The user should still be searching, and the bookmark should reappear.
        pollForModeAndCount(BookmarkUiMode.SEARCHING, 2);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    @DisabledTest(message = "crbug.com/testBookmarkFolderIcon")
    public void testBookmarkFolderIcon(boolean nightModeEnabled) throws Exception {
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        mRenderTestRule.render(
                mBookmarkManagerCoordinator.getView(), "bookmark_manager_one_folder");

        BookmarkRow itemView = getBookmarkRow(0);

        toggleSelectionThroughMoreMenu(0);

        // Make sure the Item "test" is selected.
        CriteriaHelper.pollUiThread(
                itemView::isChecked, "Expected item \"test\" to become selected");

        mRenderTestRule.render(
                mBookmarkManagerCoordinator.getView(), "bookmark_manager_folder_selected");
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // Tablets don't have a close button.
    public void testCloseBookmarksWhileStillLoading() throws Exception {
        BookmarkManagerCoordinator.preventLoadingForTesting(true);

        openBookmarkManager();

        clickToolbarMenuItem(R.id.close_menu_id);

        ApplicationTestUtils.waitForActivityState(mBookmarkActivity, Stage.DESTROYED);

        BookmarkManagerCoordinator.preventLoadingForTesting(false);
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE}) // see crbug.com/1429025
    public void testEditHiddenWhileStillLoading() throws Exception {
        BookmarkManagerCoordinator.preventLoadingForTesting(true);

        openBookmarkManager();

        assertFalse(mToolbar.getMenu().findItem(R.id.edit_menu_id).isVisible());

        BookmarkManagerCoordinator.preventLoadingForTesting(false);
    }

    @Test
    @MediumTest
    public void testStopSpinnerOnEmptyFolder() throws Exception {
        // Cannot have a promo if we're going to have 0 elements in RecyclerView.
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);

        // Force BookmarkModel to be loaded so we can get a folder id later.
        loadBookmarkModel();

        // This will cause opening the bookmarks UI to load the mobile folder.
        runOnUiThreadBlocking(
                () -> {
                    BookmarkId folderId = mBookmarkModel.getMobileFolderId();
                    String prefUrl = BookmarkUiState.createFolderUrl(folderId).toString();
                    BookmarkUtils.setLastUsedUrl(mActivityTestRule.getActivity(), prefUrl);
                });

        // Prevent loading so we can verify we see the spinner initially.
        BookmarkManagerCoordinator.preventLoadingForTesting(true);
        openBookmarkManager();

        // Loading view a child of the SelectableListLayout, not the RecyclerView.
        View parent = (View) mItemsContainer.getParent();
        View loadingView = parent.findViewById(R.id.loading_view);

        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(loadingView.getVisibility(), is(View.VISIBLE)));

        // The idea is that the manager should now be able to figure out what rows it can populate.
        // However if there are no rows created, because we have an empty folder, no events
        // naturally reach the SelectableListLayout's observer. So the manager will have to manually
        // notify.
        BookmarkManagerCoordinator.preventLoadingForTesting(false);
        runOnUiThreadBlocking(mBookmarkManagerCoordinator::finishLoadingForTesting);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(loadingView.getVisibility(), is(View.GONE));
                });
    }

    @Test
    @MediumTest
    public void testEndIconVisibilityInSelectionMode() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_TITLE_A, mTestUrlA);

        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        BookmarkRow test = getBookmarkRow(2);
        View testMoreButton = test.findViewById(R.id.more);
        View testDragHandle = test.getDragHandleViewForTesting();

        BookmarkRow testFolderA = getBookmarkRow(1);
        View aMoreButton = testFolderA.findViewById(R.id.more);
        View aDragHandle = testFolderA.getDragHandleViewForTesting();

        toggleSelectionThroughMoreMenu(2);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(test::isChecked, "Expected item \"test\" to become selected");

        assertEquals(
                "Expected bookmark toolbar to be selection mode",
                mToolbar.getCurrentViewType(),
                ViewType.SELECTION_VIEW);
        assertEquals(
                "Expected more button of selected item to be gone when drag is active.",
                View.GONE,
                testMoreButton.getVisibility());
        assertEquals(
                "Expected drag handle of selected item to be visible when drag is active.",
                View.VISIBLE,
                testDragHandle.getVisibility());
        assertTrue(
                "Expected drag handle to be enabled when drag is active.",
                testDragHandle.isEnabled());

        assertEquals(
                "Expected more button of unselected item to be gone when drag is active.",
                View.GONE,
                aMoreButton.getVisibility());
        assertEquals(
                "Expected drag handle of unselected item to be visible when drag is active.",
                View.VISIBLE,
                aDragHandle.getVisibility());
        assertFalse(
                "Expected drag handle of unselected item to be disabled when drag is active.",
                aDragHandle.isEnabled());
    }

    @Test
    @MediumTest
    public void testEndIconVisibilityInSearchMode() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        addFolder(TEST_TITLE_A);

        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        BookmarkRow test = getBookmarkRow(2);
        View testMoreButton = test.findViewById(R.id.more);
        View testDragHandle = test.getDragHandleViewForTesting();

        BookmarkRow a = getBookmarkRow(1);
        View aMoreButton = a.findViewById(R.id.more);
        View aDragHandle = a.getDragHandleViewForTesting();

        clickToolbarMenuItem(R.id.search_menu_id);
        CriteriaHelper.pollUiThread(() -> mToolbar.isSearching(), "Expected to enter search mode");

        // When searching, the promo is removed. Index 1 is now `test`.
        toggleSelectionThroughMoreMenu(1);
        CriteriaHelper.pollUiThread(test::isChecked, "Expected item \"test\" to become selected");

        assertEquals(
                "Expected drag handle of selected item to be gone "
                        + "when selection mode is activated from search.",
                View.GONE,
                testDragHandle.getVisibility());
        assertEquals(
                "Expected more button of selected item to be visible "
                        + "when selection mode is activated from search.",
                View.VISIBLE,
                testMoreButton.getVisibility());
        assertFalse(
                "Expected more button of selected item to be disabled "
                        + "when selection mode is activated from search.",
                testMoreButton.isEnabled());

        assertEquals(
                "Expected drag handle of unselected item to be gone "
                        + "when selection mode is activated from search.",
                View.GONE,
                aDragHandle.getVisibility());
        assertEquals(
                "Expected more button of unselected item to be visible "
                        + "when selection mode is activated from search.",
                View.VISIBLE,
                aMoreButton.getVisibility());
        assertFalse(
                "Expected more button of unselected item to be disabled "
                        + "when selection mode is activated from search.",
                aMoreButton.isEnabled());
    }

    @Test
    @MediumTest
    public void testSmallDrag_Up_BookmarksOnly() throws Exception {
        List<BookmarkId> initial = new ArrayList<>();
        List<BookmarkId> expected = new ArrayList<>();
        BookmarkId fooId = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo);
        BookmarkId googleId = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        BookmarkId aId = addBookmark(TEST_TITLE_A, mTestUrlA);

        // When bookmarks are added, they are added to the top of the list.
        // The current bookmark order is the reverse of the order in which they were added.
        initial.add(aId);
        initial.add(googleId);
        initial.add(fooId);

        runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Bookmarks were not added in the expected order.",
                            initial,
                            mBookmarkModel
                                    .getChildIds(mBookmarkModel.getDefaultFolder())
                                    .subList(0, 3));
                });

        expected.add(fooId);
        expected.add(aId);
        expected.add(googleId);

        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        // Callback occurs upon changes inside of the bookmark model.
        CallbackHelper modelReorderHelper = new CallbackHelper();
        BookmarkModelObserver bookmarkModelObserver =
                new BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        modelReorderHelper.notifyCalled();
                    }
                };

        // Perform registration to make callbacks work.
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.addObserver(bookmarkModelObserver);
                });

        BookmarkRow foo = getBookmarkRow(3);
        assertEquals("Wrong bookmark item selected.", TEST_PAGE_TITLE_FOO, foo.getTitle());
        toggleSelectionThroughMoreMenu(3);

        // Starts as last bookmark (2nd index) and ends as 0th bookmark (promo header not included).
        simulateDragForTestsOnUiThread(3, 1);

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        runOnUiThreadBlocking(
                () -> {
                    List<BookmarkId> observed =
                            mBookmarkModel.getChildIds(mBookmarkModel.getDefaultFolder());
                    // Exclude partner bookmarks folder
                    assertEquals(expected, observed.subList(0, 3));
                    assertTrue("The selected item should stay selected", foo.isItemSelected());
                });

        // After a drag is finished, the toolbar menu items should still reflect the selected state.
        // Check inspired by https://crbug.com/1434566.
        assertTrue(mToolbar.getMenu().findItem(R.id.selection_mode_edit_menu_id).isVisible());
        assertTrue(mToolbar.getMenu().findItem(R.id.selection_mode_move_menu_id).isVisible());
        assertTrue(mToolbar.getMenu().findItem(R.id.selection_mode_delete_menu_id).isVisible());
        assertTrue(mToolbar.getMenu().findItem(R.id.selection_open_in_new_tab_id).isVisible());
        assertTrue(
                mToolbar.getMenu().findItem(R.id.selection_open_in_incognito_tab_id).isVisible());
    }

    @Test
    @MediumTest
    public void testSmallDrag_Down_FoldersOnly() throws Exception {
        List<BookmarkId> initial = new ArrayList<>();
        List<BookmarkId> expected = new ArrayList<>();
        BookmarkId aId = addFolder("a");
        BookmarkId bId = addFolder("b");
        BookmarkId cId = addFolder("c");
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);

        initial.add(testId);
        initial.add(cId);
        initial.add(bId);
        initial.add(aId);

        runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Bookmarks were not added in the expected order.",
                            initial,
                            mBookmarkModel
                                    .getChildIds(mBookmarkModel.getDefaultFolder())
                                    .subList(0, 4));
                });

        expected.add(cId);
        expected.add(bId);
        expected.add(aId);
        expected.add(testId);

        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        // Callback occurs upon changes inside of the bookmark model.
        CallbackHelper modelReorderHelper = new CallbackHelper();
        BookmarkModelObserver bookmarkModelObserver =
                new BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        modelReorderHelper.notifyCalled();
                    }
                };

        // Perform registration to make callbacks work.
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.addObserver(bookmarkModelObserver);
                });

        BookmarkFolderRow test = getBookmarkFolderRow(1);
        assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE, test.getTitle());

        toggleSelectionThroughMoreMenu(1);

        // Starts as 0th bookmark (not counting promo header) and ends as last (index 3).
        simulateDragForTestsOnUiThread(1, 4);

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        runOnUiThreadBlocking(
                () -> {
                    List<BookmarkId> observed =
                            mBookmarkModel.getChildIds(mBookmarkModel.getDefaultFolder());
                    // Exclude partner bookmarks folder
                    assertEquals(expected, observed.subList(0, 4));
                    assertTrue("The selected item should stay selected", test.isItemSelected());
                });
    }

    @Test
    @MediumTest
    public void testSmallDrag_Down_MixedFoldersAndBookmarks() throws Exception {
        List<BookmarkId> initial = new ArrayList<>();
        List<BookmarkId> expected = new ArrayList<>();
        BookmarkId aId = addFolder("a");
        BookmarkId bId = addBookmark("b", new GURL("http://b.com"));
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);

        initial.add(testId);
        initial.add(bId);
        initial.add(aId);

        runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Bookmarks were not added in the expected order.",
                            initial,
                            mBookmarkModel
                                    .getChildIds(mBookmarkModel.getDefaultFolder())
                                    .subList(0, 3));
                });

        expected.add(bId);
        expected.add(testId);
        expected.add(aId);

        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        // Callback occurs upon changes inside of the bookmark model.
        CallbackHelper modelReorderHelper = new CallbackHelper();
        BookmarkModelObserver bookmarkModelObserver =
                new BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        modelReorderHelper.notifyCalled();
                    }
                };
        // Perform registration to make callbacks work.
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.addObserver(bookmarkModelObserver);
                });

        BookmarkFolderRow test = getBookmarkFolderRow(1);
        assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE, test.getTitle());

        toggleSelectionThroughMoreMenu(1);

        // Starts as 0th bookmark (not counting promo header) and ends at the 1st index.
        simulateDragForTestsOnUiThread(1, 2);

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        runOnUiThreadBlocking(
                () -> {
                    List<BookmarkId> observed =
                            mBookmarkModel.getChildIds(mBookmarkModel.getDefaultFolder());
                    // Exclude partner bookmarks folder
                    assertEquals(expected, observed.subList(0, 3));
                    assertTrue("The selected item should stay selected", test.isItemSelected());
                });
    }

    @Test
    @MediumTest
    public void testPromoDraggability() throws Exception {
        addFolder(TEST_FOLDER_TITLE);

        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        ViewHolder promo = getViewHolder(0);

        toggleSelectionThroughMoreMenu(1);

        assertFalse(
                "Promo header should not be passively draggable",
                isViewHolderPassivelyDraggable(promo));
        assertFalse(
                "Promo header should not be actively draggable",
                isViewHoldersActivelyDraggable(promo));
    }

    @Test
    @MediumTest
    public void testPartnerFolderDraggability() throws Exception {
        addFolderWithPartner(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        ViewHolder partner = getViewHolder(2);

        toggleSelectionThroughMoreMenu(1);

        assertFalse(
                "Partner bookmarks folder should not be passively draggable",
                isViewHolderPassivelyDraggable(partner));
        assertFalse(
                "Partner bookmarks folder should not be actively draggable",
                isViewHoldersActivelyDraggable(partner));
    }

    @Test
    @MediumTest
    public void testUnselectedItemDraggability() throws Exception {
        addBookmark("a", mTestUrlA);
        addFolder(TEST_FOLDER_TITLE);

        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        ViewHolder viewHolder = getViewHolder(1);
        assertEquals(
                "Wrong bookmark item selected.",
                TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) viewHolder.itemView).getTitle());

        toggleSelectionThroughMoreMenu(2);

        assertTrue(
                "Unselected rows should be passively draggable",
                isViewHolderPassivelyDraggable(viewHolder));
        assertFalse(
                "Unselected rows should not be actively draggable",
                isViewHoldersActivelyDraggable(viewHolder));
    }

    @Test
    @MediumTest
    public void testCannotSelectPromo() throws Exception {
        addFolder(TEST_FOLDER_TITLE);

        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();

        View promo = getViewHolder(0).itemView;
        TouchCommon.longPressView(promo);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        assertFalse(
                "Expected that we would not be in selection mode "
                        + "after long pressing on promo view.",
                mDelegate.getSelectionDelegate().isSelectionEnabled());
    }

    @Test
    @MediumTest
    public void testCannotSelectPartner() throws Exception {
        addFolderWithPartner(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();

        View partner = getViewHolder(2).itemView;
        TouchCommon.longPressView(partner);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        assertFalse(
                "Expected that we would not be in selection mode "
                        + "after long pressing on partner bookmark.",
                mDelegate.getSelectionDelegate().isSelectionEnabled());
    }

    @Test
    @MediumTest
    public void testMoveUpMenuItem() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);

        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        View google = getViewHolder(2).itemView;
        assertEquals(
                "Wrong bookmark item selected.",
                TEST_PAGE_TITLE_GOOGLE,
                ((BookmarkItemRow) google).getTitle());
        View more = google.findViewById(R.id.more);
        runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move up")).perform(click());

        // Confirm that the "Google" bookmark is now on top, and that the "test" folder is 2nd
        assertTrue((getBookmarkRow(1)).getTitle().equals(TEST_PAGE_TITLE_GOOGLE));
        assertTrue((getBookmarkRow(2)).getTitle().equals(TEST_FOLDER_TITLE));
    }

    @Test
    @MediumTest
    public void testMoveDownMenuItem() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        BookmarkFolderRow testFolder = getBookmarkFolderRow(1);
        assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE, testFolder.getTitle());
        ListMenuButton more = testFolder.findViewById(R.id.more);
        runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move down")).perform(click());

        // Confirm that the "Google" bookmark is now on top, and that the "test" folder is 2nd
        assertTrue((getBookmarkRow(1)).getTitle().equals(TEST_PAGE_TITLE_GOOGLE));
        assertTrue((getBookmarkRow(2)).getTitle().equals(TEST_FOLDER_TITLE));
    }

    @Test
    @MediumTest
    public void testMoveDownGoneForBottomElement() throws Exception {
        addBookmarkWithPartner(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        addFolderWithPartner(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        BookmarkItemRow google = getBookmarkItemRow(2);
        assertEquals("Wrong bookmark item selected.", TEST_PAGE_TITLE_GOOGLE, google.getTitle());
        View more = google.findViewById(R.id.more);
        runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveUpGoneForTopElement() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestUrlA);
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        BookmarkFolderRow testFolder = getBookmarkFolderRow(1);
        assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE, testFolder.getTitle());
        ListMenuButton more = testFolder.findViewById(R.id.more);
        runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move up")).check(doesNotExist());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1046653")
    public void testMoveButtonsGoneInSearchMode() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();

        clickToolbarMenuItem(R.id.search_menu_id);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(() -> mToolbar.isSearching(), "Expected to enter search mode");

        BookmarkFolderRow testFolder = getBookmarkFolderRow(0);
        assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE, testFolder.getTitle());
        View more = testFolder.findViewById(R.id.more);
        runOnUiThreadBlocking(more::callOnClick);

        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveButtonsGoneWithOneBookmark() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        BookmarkFolderRow testFolder = getBookmarkFolderRow(1);
        assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE, testFolder.getTitle());
        View more = testFolder.findViewById(R.id.more);
        runOnUiThreadBlocking(more::callOnClick);

        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveButtonsGoneForPartnerBookmarks() throws Exception {
        loadFakePartnerBookmarkShimForTesting();
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();

        // Open partner bookmarks folder.
        BookmarkId partnerFolderId =
                runOnUiThreadBlocking(() -> mBookmarkModel.getPartnerFolderId());
        openFolder(partnerFolderId);

        assertEquals(
                "Wrong number of items in partner bookmark folder.", 2, mAdapter.getItemCount());

        // Verify that bookmark 1 is editable (so more button can be triggered) but not movable.
        BookmarkId partnerBookmarkId1 = getIdByPosition(0);
        runOnUiThreadBlocking(
                () -> {
                    BookmarkItem partnerBookmarkItem1 =
                            mBookmarkModel.getBookmarkById(partnerBookmarkId1);
                    partnerBookmarkItem1.forceEditableForTesting();
                    assertEquals(
                            "Incorrect bookmark type for item 1",
                            BookmarkType.PARTNER,
                            partnerBookmarkId1.getType());
                    assertFalse(
                            "Partner item 1 should not be movable",
                            BookmarkUtils.isMovable(mBookmarkModel, partnerBookmarkItem1));
                    assertTrue(
                            "Partner item 1 should be editable", partnerBookmarkItem1.isEditable());
                });

        // Verify that bookmark 2 is editable (so more button can be triggered) but not movable.
        View partnerBookmarkView1 = getBookmarkRow(0);
        View more1 = partnerBookmarkView1.findViewById(R.id.more);
        runOnUiThreadBlocking(more1::callOnClick);
        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());

        // Verify that bookmark 2 is not movable.
        BookmarkId partnerBookmarkId2 = getIdByPosition(1);
        runOnUiThreadBlocking(
                () -> {
                    BookmarkItem partnerBookmarkItem2 =
                            mBookmarkModel.getBookmarkById(partnerBookmarkId2);
                    partnerBookmarkItem2.forceEditableForTesting();
                    assertEquals(
                            "Incorrect bookmark type for item 2",
                            BookmarkType.PARTNER,
                            partnerBookmarkId2.getType());
                    assertFalse(
                            "Partner item 2 should not be movable",
                            BookmarkUtils.isMovable(mBookmarkModel, partnerBookmarkItem2));
                    assertTrue(
                            "Partner item 2 should be editable", partnerBookmarkItem2.isEditable());
                });

        // Verify that bookmark 2 does not have move up/down items.
        View partnerBookmarkView2 = getBookmarkRow(1);
        View more2 = partnerBookmarkView2.findViewById(R.id.more);
        runOnUiThreadBlocking(more2::callOnClick);
        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.SHOPPING_LIST})
    public void testTopLevelFolderUpdateAfterSync() throws Exception {
        // Set up the test and open the bookmark manager to the Mobile Bookmarks folder.
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        openBookmarkManager();

        // Add a bookmark to the Other Bookmarks folder.
        runOnUiThreadBlocking(
                () -> {
                    assertNotNull(
                            mBookmarkModel.addBookmark(
                                    mBookmarkModel.getOtherFolderId(), 0, TEST_TITLE_A, mTestUrlA));
                });

        verify(mSyncService, atLeast(1))
                .addSyncStateChangedListener(mSyncStateChangedListenerCaptor.capture());
        for (SyncStateChangedListener syncStateChangedListener :
                mSyncStateChangedListenerCaptor.getAllValues()) {
            runOnUiThreadBlocking(syncStateChangedListener::syncStateChanged);
        }
        runOnUiThreadBlocking(getTestingDelegate()::simulateSignInForTesting);

        assertEquals(
                "Expected promo, \"Reading List\", \"Mobile Bookmarks\" and \"Other Bookmarks\""
                        + " folder to appear!",
                4,
                mAdapter.getItemCount());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1369091")
    public void testShowInFolder_NoScroll() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        // Enter search mode.
        enterSearch();

        // Click "Show in folder".
        View testFolder = getBookmarkFolderRow(0);
        clickMoreButtonOnFirstItem(TEST_FOLDER_TITLE);
        onView(withText("Show in folder")).perform(click());

        // Assert that the view pulses.
        assertTrue(
                "Expected bookmark row to pulse after clicking \"show in folder\"!",
                checkHighlightPulse(testFolder));

        // Enter search mode again.
        enterSearch();

        assertTrue(
                "Expected bookmark row to not be highlighted " + "after entering search mode",
                checkHighlightOff(testFolder));

        // Click "Show in folder" again.
        clickMoreButtonOnFirstItem(TEST_FOLDER_TITLE);
        onView(withText("Show in folder")).perform(click());
        assertTrue(
                "Expected bookmark row to pulse after clicking \"show in folder\" a 2nd time!",
                checkHighlightPulse(testFolder));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1434777")
    public void testShowInFolder_Scroll() throws Exception {
        addFolder(TEST_FOLDER_TITLE); // Index 8
        addBookmark(TEST_TITLE_A, mTestUrlA);
        addBookmark(TEST_PAGE_TITLE_FOO, new GURL("http://foo.com"));
        addFolder(TEST_PAGE_TITLE_GOOGLE2);
        addFolder("B");
        addFolder("C");
        addFolder("D");
        addFolder("E"); // Index 1
        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();

        // Enter search mode.
        enterSearch();

        searchBookmarks(TEST_FOLDER_TITLE);

        // This should be the only (& therefore 0-indexed) item.
        clickMoreButtonOnFirstItem(TEST_FOLDER_TITLE);

        // Show in folder.
        onView(withText("Show in folder")).perform(click());

        // This should be in the 8th position now.
        BookmarkFolderRow testFolderInList = getBookmarkFolderRow(8);
        assertFalse("Expected list to scroll bookmark item into view", testFolderInList == null);
        assertEquals(
                "Wrong bookmark item selected.",
                TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolderInList).getTitle());
        assertTrue(
                "Expected highlight to pulse on after scrolling to the item!",
                checkHighlightPulse(testFolderInList));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1434777")
    public void testShowInFolder_OpenOtherFolder() throws Exception {
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);
        runOnUiThreadBlocking(() -> mBookmarkModel.addBookmark(testId, 0, TEST_TITLE_A, mTestUrlA));
        BookmarkPromoHeader.forcePromoStateForTesting(
                SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE);
        openBookmarkManager();

        // Enter search mode.
        enterSearch();
        searchBookmarks(mTestUrlA.getSpec());

        // This should be the only (& therefore 0-indexed) item.
        clickMoreButtonOnFirstItem(TEST_TITLE_A);

        // Show in folder.
        onView(withText("Show in folder")).perform(click());
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);

        // Make sure that we're in the right folder (index 1 because of promo).
        BookmarkItemRow itemA = getBookmarkItemRow(1);
        assertEquals("Wrong bookmark item selected.", TEST_TITLE_A, itemA.getTitle());

        assertTrue(
                "Expected highlight to pulse after opening an item in another folder!",
                checkHighlightPulse(itemA));

        // Open mobile bookmarks folder, then go back to the subfolder.
        openFolder(mBookmarkModel.getMobileFolderId());
        openFolder(testId);

        BookmarkItemRow itemASecondView = getBookmarkItemRow(1);
        assertEquals("Wrong bookmark item selected.", TEST_TITLE_A, itemASecondView.getTitle());
        assertTrue(
                "Expected highlight to not be highlighted after exiting and re-entering folder!",
                checkHighlightOff(itemASecondView));
    }

    @Test
    @SmallTest
    public void testAddBookmarkInBackgroundWithSelection() throws Exception {
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        assertEquals(1, mAdapter.getItemCount());
        toggleSelectionThroughMoreMenu(0);

        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.addBookmark(folder, 1, TEST_PAGE_TITLE_GOOGLE, mTestPage);
                });

        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        runOnUiThreadBlocking(
                () -> {
                    assertTrue(isItemPresentInBookmarkList(TEST_PAGE_TITLE_FOO));
                    assertTrue(isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
                    assertEquals(2, mAdapter.getItemCount());
                    assertTrue(
                            "The selected row should be kept selected",
                            getBookmarkRow(0).isItemSelected());
                });
    }

    @Test
    @SmallTest
    public void testDeleteAllSelectedBookmarksInBackground() throws Exception {
        // Select one bookmark and then remove that in background.
        // In the meantime, the toolbar changes from selection mode to normal mode.
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        BookmarkId googleId = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        addBookmark(TEST_TITLE_A, mTestUrlA, folder);
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        assertEquals(3, mAdapter.getItemCount());
        toggleSelectionThroughMoreMenu(1);
        CallbackHelper helper = new CallbackHelper();
        runOnUiThreadBlocking(
                () -> {
                    mDelegate.getSelectionDelegate().addObserver((x) -> helper.notifyCalled());
                });

        removeBookmark(googleId);

        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        helper.waitForCallback(0, 1);
        runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Item is not deleted",
                            isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
                    assertEquals(2, mAdapter.getItemCount());
                    assertEquals(
                            "Bookmark View should be back to normal view",
                            mToolbar.getCurrentViewType(),
                            ViewType.NORMAL_VIEW);
                });
    }

    @Test
    @SmallTest
    public void testDeleteSomeSelectedBookmarksInBackground() throws Exception {
        // selected on bookmarks and then remove one of them in background
        // in the meantime, the toolbar stays in selection mode
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        BookmarkId googleId = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage, folder);
        addBookmark(TEST_TITLE_A, mTestUrlA, folder);
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        assertEquals(3, mAdapter.getItemCount());
        toggleSelectionThroughLongPress(1);
        toggleSelectionThroughLongPress(0);
        CallbackHelper helper = new CallbackHelper();

        runOnUiThreadBlocking(
                () -> {
                    mDelegate.getSelectionDelegate().addObserver((x) -> helper.notifyCalled());
                });

        removeBookmark(googleId);

        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        helper.waitForCallback(0, 1);
        runOnUiThreadBlocking(
                () -> {
                    assertFalse(
                            "Item is not deleted",
                            isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
                    assertEquals(2, mAdapter.getItemCount());
                    assertTrue(
                            "Item selected should not be cleared",
                            getBookmarkRow(0).isItemSelected());
                    assertEquals(
                            "Should stay in selection mode because there is one selected",
                            mToolbar.getCurrentViewType(),
                            ViewType.SELECTION_VIEW);
                });
    }

    @Test
    @SmallTest
    public void testUpdateSelectedBookmarkInBackground() throws Exception {
        BookmarkId folder = addFolder(TEST_FOLDER_TITLE);
        BookmarkId id = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folder);
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();

        // Open the new folder where these bookmarks were created.
        openFolder(folder);

        assertEquals(1, mAdapter.getItemCount());
        toggleSelectionThroughMoreMenu(0);
        CallbackHelper helper = new CallbackHelper();
        runOnUiThreadBlocking(
                () ->
                        mBookmarkModel.addObserver(
                                new BookmarkModelObserver() {
                                    @Override
                                    public void bookmarkModelChanged() {
                                        helper.notifyCalled();
                                    }
                                }));

        runOnUiThreadBlocking(() -> mBookmarkModel.setBookmarkTitle(id, TEST_PAGE_TITLE_GOOGLE));

        helper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        runOnUiThreadBlocking(
                () -> {
                    assertFalse(isItemPresentInBookmarkList(TEST_PAGE_TITLE_FOO));
                    assertTrue(isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
                    assertEquals(1, mAdapter.getItemCount());
                    assertTrue(
                            "The selected row should stay selected",
                            getBookmarkRow(0).isItemSelected());
                });
    }

    @Test
    @MediumTest
    public void testBookmarksDoesNotRecordLaunchMetrics() throws Throwable {
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM));

        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        openBookmarkManager();

        pressBack();
        BookmarkTestUtil.waitForTabbedActivity();
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM));

        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        onView(withText(TEST_PAGE_TITLE_GOOGLE)).perform(click());
        BookmarkTestUtil.waitForTabbedActivity();
        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        LaunchCauseMetrics.LAUNCH_CAUSE_HISTOGRAM));
    }

    /**
     * Test that we record Bookmarks.BookmarkTestUtil.openBookmarkManager.PerProfileType when
     * R.id.all_bookmarks_menu_id is clicked in regular mode.
     *
     * <p>Please note that this test doesn't run for tablet because of the way bookmark manager is
     * opened for tablets via BookmarkTestUtil.openBookmarkManager test method which circumvents the
     * click of R.id.all_bookmarks_menu_id, this doesn't happen in actual case and the metric indeed
     * gets recorded in tablets.
     */
    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testRecordsHistogramWhenBookmarkManagerOpened_InRegular() throws Throwable {
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType"));

        openBookmarkManager();
        pressBack();
        BookmarkTestUtil.waitForTabbedActivity();

        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType"));

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType",
                        BrowserProfileType.REGULAR));
    }

    /**
     * Test that we record Bookmarks.OpenBookmarkManager.PerProfileType when
     * R.id.all_bookmarks_menu_id is clicked in Incognito mode.
     *
     * <p>Please note that this test doesn't run for tablet because of the way bookmark manager is
     * opened for tablets via openBookmarkManager test method which circumvents the click of
     * R.id.all_bookmarks_menu_id. This doesn't happen in actual case and the metric indeed gets
     * recorded in tablets.
     */
    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testRecordsHistogramWhenBookmarkManagerOpened_InIncognito() throws Throwable {
        assertEquals(
                0,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType"));

        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);
        openBookmarkManager();
        pressBack();
        BookmarkTestUtil.waitForTabbedActivity();

        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType"));

        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Bookmarks.OpenBookmarkManager.PerProfileType",
                        BrowserProfileType.INCOGNITO));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarksVisualRefreshFolders() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        addFolder(TEST_FOLDER_TITLE);
        addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        mRenderTestRule.render(
                mBookmarkManagerCoordinator.getView(), "bookmarks_visual_refresh_folders");
        BookmarkRow itemView = getBookmarkRow(0);

        toggleSelectionThroughMoreMenu(0);

        // Make sure the Item "test" is selected.
        CriteriaHelper.pollUiThread(
                itemView::isChecked, "Expected item \"test\" to become selected");

        mRenderTestRule.render(
                mBookmarkManagerCoordinator.getView(), "bookmarks_visual_refresh_folders_selected");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testBookmarksVisualRefreshBookmarksAndFolder() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();
        BookmarkTestUtil.openMobileBookmarks(mItemsContainer, mDelegate, mBookmarkModel);

        mRenderTestRule.render(
                mBookmarkManagerCoordinator.getView(),
                "bookmarks_visual_refresh_bookmarksandfolders");
        BookmarkRow itemView1 = getBookmarkRow(0);
        BookmarkRow itemView2 = getBookmarkRow(1);

        toggleSelectionThroughLongPress(0);
        toggleSelectionThroughLongPress(1);

        // Make sure the Item "test" is selected.
        CriteriaHelper.pollUiThread(
                itemView1::isChecked, "Expected item \"test\" to become selected");
        CriteriaHelper.pollUiThread(
                itemView2::isChecked, "Expected item \"test\" to become selected");

        mRenderTestRule.render(
                mBookmarkManagerCoordinator.getView(),
                "bookmarks_compact_visual_refresh_bookmarksandfolders_selected");
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testShoppingFilterInBookmarks() throws InterruptedException, ExecutionException {
        ShoppingFeatures.setShoppingListEligibleForTesting(true);
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        runOnUiThreadBlocking(
                () -> {
                    mDelegate.openFolder(mBookmarkModel.getRootFolderId());
                });

        onView(withText("Tracked products")).perform(click());

        // Check that we are in the mobile bookmarks folder.
        assertEquals("Tracked products", mToolbar.getTitle());
        assertEquals(NavigationButton.BACK, mToolbar.getNavigationButtonForTests());
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testShoppingDataPresentButFeatureDisabled()
            throws InterruptedException, ExecutionException {
        ShoppingFeatures.setShoppingListEligibleForTesting(true);
        BookmarkId id = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        PowerBookmarkMeta.Builder meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setProductClusterId(1234L).build());
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.setPowerBookmarkMeta(id, meta.build());
                });
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        openBookmarkManager();
        BookmarkTestUtil.waitForBookmarkModelLoaded();

        BookmarkRow itemView = getBookmarkRow(0);
        assertNotEquals(PowerBookmarkShoppingItemRow.class, itemView.getClass());
    }

    @Test
    @MediumTest
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testModelUpdateDuringEmptySearchAndSelection()
            throws InterruptedException, ExecutionException {
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        BookmarkId folderId = addFolder(TEST_FOLDER_TITLE);
        BookmarkId itemId = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo, folderId);
        openBookmarkManager();
        openFolder(folderId);
        assertEquals(1, mAdapter.getItemCount());

        // Inspired by https://crbug.com/1445826. Start a search but don't type anything, to get
        // into the empty query state. Then select an item. This combinations of states is tricky
        // for our logic to deal with correctly.
        clickToolbarMenuItem(R.id.search_menu_id);
        toggleSelectionThroughLongPress(0);

        // Users would tap the R.id.edit_menu_id and edit the bookmark title, but we're going to
        // fake it to avoid a dependency on the UI shown to edit bookmark.
        runOnUiThreadBlocking(
                () -> mBookmarkModel.setBookmarkTitle(itemId, TEST_PAGE_TITLE_GOOGLE));

        // The item title change comes back async from the model, so we need to poll for it.
        CriteriaHelper.pollUiThread(
                () -> {
                    return TEST_PAGE_TITLE_GOOGLE.equals(getBookmarkRow(0).getTitle());
                },
                "Title never updated to " + TEST_PAGE_TITLE_GOOGLE);

        // We should be kicked out of the empty query state at this point, but still selecting the
        // item. If events are not fired in the right order, the title/nav buttons will be broken.
        assertTrue(getBookmarkRow(0).isItemSelected());
        assertFalse(mToolbar.isSearching());
        assertTrue(TextUtils.isEmpty(mToolbar.getTitle()));
        // Don't try to validate the current number of selected items, just visibility.
        NumberRollView numberRollView = mToolbar.findViewById(R.id.selection_mode_number);
        assertEquals(View.VISIBLE, numberRollView.getVisibility());
        assertEquals(NavigationButton.BACK, mToolbar.getNavigationButtonForTests());
    }

    /**
     * Loads a non-empty partner bookmarks folder for testing. The partner bookmarks folder will
     * appear in the mobile bookmarks folder.
     */
    private void loadFakePartnerBookmarkShimForTesting() {
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.loadFakePartnerBookmarkShimForTesting();
                });
        BookmarkTestUtil.waitForBookmarkModelLoaded();
    }

    private void openBookmarkManager() throws InterruptedException {
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
            // Phone.
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

        mDelegate = mBookmarkManagerCoordinator.getBookmarkDelegateForTesting();
        mAdapter = (DragReorderableRecyclerViewAdapter) mItemsContainer.getAdapter();
        mToolbar = mBookmarkManagerCoordinator.getToolbarForTesting();

        runOnUiThreadBlocking(
                () -> AccessibilityState.setIsAnyAccessibilityServiceEnabledForTesting(false));
    }

    private boolean isItemPresentInBookmarkList(final String expectedTitle) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() {
                        for (int i = 0; i < mAdapter.getItemCount(); i++) {
                            BookmarkId item = getIdByPosition(i);

                            if (item == null) continue;

                            String actualTitle = mBookmarkModel.getBookmarkTitle(item);
                            if (TextUtils.equals(actualTitle, expectedTitle)) {
                                return true;
                            }
                        }
                        return false;
                    }
                });
    }

    /**
     * Adds a bookmark in the scenario where we have partner bookmarks.
     *
     * @param title The title of the bookmark to add.
     * @param url The url of the bookmark to add.
     * @return The BookmarkId of the added bookmark.
     * @throws ExecutionException If something goes wrong while we are trying to add the bookmark.
     */
    private BookmarkId addBookmarkWithPartner(String title, GURL url) throws ExecutionException {
        BookmarkTestUtil.loadEmptyPartnerBookmarksForTesting(mBookmarkModel);
        return runOnUiThreadBlocking(
                () -> mBookmarkModel.addBookmark(mBookmarkModel.getDefaultFolder(), 0, title, url));
    }

    /**
     * Adds a folder in the scenario where we have partner bookmarks.
     *
     * @param title The title of the folder to add.
     * @return The BookmarkId of the added folder.
     * @throws ExecutionException If something goes wrong while we are trying to add the bookmark.
     */
    private BookmarkId addFolderWithPartner(String title) throws ExecutionException {
        BookmarkTestUtil.loadEmptyPartnerBookmarksForTesting(mBookmarkModel);
        return runOnUiThreadBlocking(
                () -> mBookmarkModel.addFolder(mBookmarkModel.getDefaultFolder(), 0, title));
    }

    private void simulateDragForTestsOnUiThread(int start, int end) {
        runOnUiThreadBlocking(() -> mAdapter.simulateDragForTests(start, end));
    }

    private boolean isViewHolderPassivelyDraggable(ViewHolder viewHolder) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mAdapter.isPassivelyDraggable(viewHolder));
    }

    private boolean isViewHoldersActivelyDraggable(ViewHolder viewHolder) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mAdapter.isActivelyDraggable(viewHolder));
    }

    private TestingDelegate getTestingDelegate() {
        return mBookmarkManagerCoordinator.getTestingDelegate();
    }

    private void enterSearch() throws Exception {
        clickToolbarMenuItem(R.id.search_menu_id);
        CriteriaHelper.pollUiThread(
                () -> {
                    return mToolbar.isSearching();
                },
                "Expected to enter search mode");
    }

    private void clickMoreButtonOnFirstItem(String expectedBookmarkItemTitle) throws Exception {
        BookmarkRow firstItem = getBookmarkRow(0);
        assertEquals(
                "Wrong bookmark item selected.", expectedBookmarkItemTitle, firstItem.getTitle());
        View more = firstItem.findViewById(R.id.more);
        runOnUiThreadBlocking(more::performClick);
    }

    /**
     * Returns the View that has the given text.
     *
     * @param viewGroup The group to which the view belongs.
     * @param expectedText The expected description text.
     * @return The unique view, if one exists. Throws an exception if one doesn't exist.
     */
    private static View getViewWithText(final ViewGroup viewGroup, final String expectedText) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                new Callable<View>() {
                    @Override
                    public View call() {
                        ArrayList<View> outViews = new ArrayList<>();
                        ArrayList<View> matchingViews = new ArrayList<>();
                        viewGroup.findViewsWithText(
                                outViews, expectedText, View.FIND_VIEWS_WITH_TEXT);
                        // outViews includes all views whose text contains expectedText as a
                        // case-insensitive substring. Filter these views to find only exact string
                        // matches.
                        for (View v : outViews) {
                            if (TextUtils.equals(
                                    ((TextView) v).getText().toString(), expectedText)) {
                                matchingViews.add(v);
                            }
                        }
                        assertEquals(
                                "Exactly one item should be present.", 1, matchingViews.size());
                        return matchingViews.get(0);
                    }
                });
    }

    private void toggleSelectionThroughLongPress(int index) {
        toggleSelectionAndEndAnimation(
                index,
                (bookmarkRow) -> {
                    runOnUiThreadBlocking(
                            () -> {
                                bookmarkRow.onLongClick(bookmarkRow);
                            });
                });
    }

    private void toggleSelectionThroughMoreMenu(int index) {
        toggleSelectionAndEndAnimation(
                index,
                (bookmarkRow) -> {
                    View moreButton = bookmarkRow.findViewById(R.id.more);
                    assertEquals(View.VISIBLE, moreButton.getVisibility());
                    assertTrue(moreButton.isEnabled());
                    runOnUiThreadBlockingNoException(moreButton::callOnClick);

                    // Doesn't have a stable id to look up with. Use resolved text instead.
                    String selectText =
                            bookmarkRow.getResources().getString(R.string.bookmark_item_select);
                    onView(withText(selectText)).perform(click());
                });
    }

    private void toggleSelectionAndEndAnimation(int index, Callback<BookmarkRow> toggleRowImpl) {
        BookmarkRow bookmarkRow = getBookmarkRow(index);
        boolean wasInitiallySelected = bookmarkRow.isItemSelected();
        toggleRowImpl.onResult(bookmarkRow);
        runOnUiThreadBlocking(
                () -> {
                    bookmarkRow.endAnimationsForTests();
                    mToolbar.endAnimationsForTesting();
                });
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        assertNotEquals(wasInitiallySelected, bookmarkRow.isItemSelected());
    }

    private BookmarkId addBookmark(final String title, GURL url, BookmarkId parent)
            throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        return runOnUiThreadBlocking(() -> mBookmarkModel.addBookmark(parent, 0, title, url));
    }

    private BookmarkId addBookmark(final String title, final GURL url) throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        return runOnUiThreadBlocking(
                () -> mBookmarkModel.addBookmark(mBookmarkModel.getDefaultFolder(), 0, title, url));
    }

    private BookmarkId addFolder(final String title) throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        return runOnUiThreadBlocking(
                () -> mBookmarkModel.addFolder(mBookmarkModel.getDefaultFolder(), 0, title));
    }

    private BookmarkId addFolder(final String title, BookmarkId parent) throws ExecutionException {
        BookmarkTestUtil.readPartnerBookmarks(mActivityTestRule);
        return runOnUiThreadBlocking(() -> mBookmarkModel.addFolder(parent, 0, title));
    }

    private void removeBookmark(final BookmarkId bookmarkId) {
        runOnUiThreadBlocking(() -> mBookmarkModel.deleteBookmark(bookmarkId));
    }

    private BookmarkId getIdByPosition(int pos) {
        return getTestingDelegate().getIdByPositionForTesting(pos);
    }

    private void searchBookmarks(final String query) {
        runOnUiThreadBlocking(() -> getTestingDelegate().searchForTesting(query));
        // If the RecyclerView is GONE, it will never perform layout, and never stabilize.
        if (mItemsContainer.getVisibility() == View.VISIBLE) {
            RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        }
    }

    private void openFolder(BookmarkId folder) {
        runOnUiThreadBlocking(() -> mDelegate.openFolder(folder));
        // If the RecyclerView is GONE, it will never perform layout, and never stabilize.
        if (mItemsContainer.getVisibility() == View.VISIBLE) {
            RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
        }
    }

    private void pressBackButton() {
        runOnUiThreadBlocking(mBookmarkActivity.getOnBackPressedDispatcher()::onBackPressed);
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mItemsContainer);
    }

    private void clickToolbarMenuItem(@IdRes int menuId) throws ExecutionException {
        runOnUiThreadBlocking(() -> mToolbar.onMenuItemClick(mToolbar.getMenu().findItem(menuId)));
    }

    private ViewHolder getViewHolder(int index) {
        return mItemsContainer.findViewHolderForAdapterPosition(index);
    }

    private BookmarkRow getBookmarkRow(int index) {
        return getRowGeneric(BookmarkRow.class, index);
    }

    private BookmarkFolderRow getBookmarkFolderRow(int index) {
        return getRowGeneric(BookmarkFolderRow.class, index);
    }

    private BookmarkItemRow getBookmarkItemRow(int index) {
        return getRowGeneric(BookmarkItemRow.class, index);
    }

    private <T extends View> T getRowGeneric(Class<T> clazz, int index) {
        View view = getViewHolder(index).itemView;
        assertTrue(
                "Found " + view.getClass() + " expected " + clazz,
                clazz.isAssignableFrom(view.getClass()));
        return (T) view;
    }

    private void loadBookmarkModel() {
        runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel.finishLoadingBookmarkModel(() -> {});
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mBookmarkModel.isBookmarkModelLoaded(), is(true));
                });
    }

    private void pollForModeAndCount(@BookmarkUiMode int uiMode, int itemCount) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(mDelegate.getCurrentUiMode(), is(uiMode));
                    Criteria.checkThat(mAdapter.getItemCount(), is(itemCount));
                });
    }
}
