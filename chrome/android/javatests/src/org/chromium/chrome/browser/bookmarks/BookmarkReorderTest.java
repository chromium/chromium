// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.chrome.browser.ui.widget.highlight.ViewHighlighterTestUtils.checkHighlightOff;
import static org.chromium.chrome.browser.ui.widget.highlight.ViewHighlighterTestUtils.checkHighlightPulse;

import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.support.v7.widget.RecyclerView.AdapterDataObserver;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.View;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge.BookmarkModelObserver;
import org.chromium.chrome.browser.bookmarks.BookmarkPromoHeader.PromoState;
import org.chromium.chrome.browser.night_mode.NightModeTestUtils;
import org.chromium.chrome.browser.ui.widget.ListMenuButton;
import org.chromium.chrome.browser.widget.selection.SelectableListToolbar.ViewType;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Tests for the bookmark manager.
 */
// clang-format off
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.REORDER_BOOKMARKS)
@RetryOnFailure
public class BookmarkReorderTest extends BookmarkTest {
    // clang-format on

    // This class extends BookmarkTest because some new features are being added, but all of the
    // old functionality should remain; thus, we want to also run all of the old tests.
    // This seemed the most elegant and efficient way to accomplish this.
    // TODO(crbug.com/160194): Clean up after bookmark reordering launches.

    private static final String TEST_TITLE_A = "a";
    private static final String TEST_URL_A = "http://a.com";
    private static final String TAG = "BookmarkReorderTest";

    @Override
    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Test
    @MediumTest
    public void testEndIconVisibilityInSelectionMode() throws Exception {
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);
        addBookmark(TEST_TITLE_A, TEST_URL_A);

        forceSyncHeaderState();
        openBookmarkManager();

        BookmarkRow test =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        View testMoreButton = test.findViewById(R.id.more);
        View testDragHandle = test.findViewById(R.id.drag_handle);

        View testFolderA = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        View aMoreButton = testFolderA.findViewById(R.id.more);
        View aDragHandle = testFolderA.findViewById(R.id.drag_handle);

        toggleSelectionAndEndAnimation(testId, test);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(test::isChecked, "Expected item \"test\" to become selected");

        Assert.assertEquals("Expected bookmark toolbar to be selection mode",
                mManager.getToolbarForTests().getCurrentViewType(), ViewType.SELECTION_VIEW);
        Assert.assertEquals("Expected more button of selected item to be gone when drag is active.",
                View.GONE, testMoreButton.getVisibility());
        Assert.assertEquals(
                "Expected drag handle of selected item to be visible when drag is active.",
                View.VISIBLE, testDragHandle.getVisibility());
        Assert.assertTrue("Expected drag handle to be enabled when drag is active.",
                testDragHandle.isEnabled());

        Assert.assertEquals(
                "Expected more button of unselected item to be gone when drag is active.",
                View.GONE, aMoreButton.getVisibility());
        Assert.assertEquals(
                "Expected drag handle of unselected item to be visible when drag is active.",
                View.VISIBLE, aDragHandle.getVisibility());
        Assert.assertFalse(
                "Expected drag handle of unselected item to be disabled when drag is active.",
                aDragHandle.isEnabled());
    }

    @Test
    @MediumTest
    public void testEndIconVisiblityInSearchMode() throws Exception {
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);
        addFolder(TEST_TITLE_A);

        forceSyncHeaderState();
        openBookmarkManager();

        View searchButton = mManager.getToolbarForTests().findViewById(R.id.search_menu_id);

        BookmarkRow test =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        View testMoreButton = test.findViewById(R.id.more);
        View testDragHandle = test.findViewById(R.id.drag_handle);

        View a = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        View aMoreButton = a.findViewById(R.id.more);
        View aDragHandle = a.findViewById(R.id.drag_handle);

        TestThreadUtils.runOnUiThreadBlocking(searchButton::performClick);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(
                () -> mManager.getToolbarForTests().isSearching(), "Expected to enter search mode");

        toggleSelectionAndEndAnimation(testId, test);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(test::isChecked, "Expected item \"test\" to become selected");

        Assert.assertEquals("Expected drag handle of selected item to be gone "
                        + "when selection mode is activated from search.",
                View.GONE, testDragHandle.getVisibility());
        Assert.assertEquals("Expected more button of selected item to be visible "
                        + "when selection mode is activated from search.",
                View.VISIBLE, testMoreButton.getVisibility());
        Assert.assertFalse("Expected more button of selected item to be disabled "
                        + "when selection mode is activated from search.",
                testMoreButton.isEnabled());

        Assert.assertEquals("Expected drag handle of unselected item to be gone "
                        + "when selection mode is activated from search.",
                View.GONE, aDragHandle.getVisibility());
        Assert.assertEquals("Expected more button of unselected item to be visible "
                        + "when selection mode is activated from search.",
                View.VISIBLE, aMoreButton.getVisibility());
        Assert.assertFalse("Expected more button of unselected item to be disabled "
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
        BookmarkId aId = addBookmark(TEST_TITLE_A, TEST_URL_A);

        // When bookmarks are added, they are added to the top of the list.
        // The current bookmark order is the reverse of the order in which they were added.
        initial.add(aId);
        initial.add(googleId);
        initial.add(fooId);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Bookmarks were not added in the expected order.", initial,
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder(), true, true)
                            .subList(0, 3));
        });

        expected.add(fooId);
        expected.add(aId);
        expected.add(googleId);

        forceSyncHeaderState();
        openBookmarkManager();

        // Callback occurs upon changes inside of the bookmark model.
        CallbackHelper modelReorderHelper = new CallbackHelper();
        BookmarkBridge.BookmarkModelObserver bookmarkModelObserver =
                new BookmarkBridge.BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        modelReorderHelper.notifyCalled();
                    }
                };

        // Perform registration to make callbacks work.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel.addObserver(bookmarkModelObserver);
        });

        BookmarkRow foo =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(3).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_PAGE_TITLE_FOO, foo.getTitle());
        toggleSelectionAndEndAnimation(fooId, foo);

        // Starts as last bookmark (2nd index) and ends as 0th bookmark (promo header not included).
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((ReorderBookmarkItemsAdapter) mItemsContainer.getAdapter()).simulateDragForTests(3, 1);
        });

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<BookmarkId> observed =
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder(), true, true);
            // Exclude partner bookmarks folder
            Assert.assertEquals(expected, observed.subList(0, 3));
            Assert.assertTrue("The selected item should stay selected", foo.isItemSelected());
        });
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

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Bookmarks were not added in the expected order.", initial,
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder(), true, true)
                            .subList(0, 4));
        });

        expected.add(cId);
        expected.add(bId);
        expected.add(aId);
        expected.add(testId);

        forceSyncHeaderState();
        openBookmarkManager();

        // Callback occurs upon changes inside of the bookmark model.
        CallbackHelper modelReorderHelper = new CallbackHelper();
        BookmarkBridge.BookmarkModelObserver bookmarkModelObserver =
                new BookmarkBridge.BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        modelReorderHelper.notifyCalled();
                    }
                };

        // Perform registration to make callbacks work.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel.addObserver(bookmarkModelObserver);
        });

        BookmarkFolderRow test =
                (BookmarkFolderRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE, test.getTitle());

        toggleSelectionAndEndAnimation(testId, test);

        // Starts as 0th bookmark (not counting promo header) and ends as last (index 3).
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((ReorderBookmarkItemsAdapter) mItemsContainer.getAdapter()).simulateDragForTests(1, 4);
        });

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<BookmarkId> observed =
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder(), true, true);
            // Exclude partner bookmarks folder
            Assert.assertEquals(expected, observed.subList(0, 4));
            Assert.assertTrue("The selected item should stay selected", test.isItemSelected());
        });
    }

    @Test
    @MediumTest
    public void testSmallDrag_Down_MixedFoldersAndBookmarks() throws Exception {
        List<BookmarkId> initial = new ArrayList<>();
        List<BookmarkId> expected = new ArrayList<>();
        BookmarkId aId = addFolder("a");
        BookmarkId bId = addBookmark("b", "http://b.com");
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);

        initial.add(testId);
        initial.add(bId);
        initial.add(aId);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals("Bookmarks were not added in the expected order.", initial,
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder(), true, true)
                            .subList(0, 3));
        });

        expected.add(bId);
        expected.add(testId);
        expected.add(aId);

        forceSyncHeaderState();
        openBookmarkManager();

        // Callback occurs upon changes inside of the bookmark model.
        CallbackHelper modelReorderHelper = new CallbackHelper();
        BookmarkBridge.BookmarkModelObserver bookmarkModelObserver =
                new BookmarkBridge.BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        modelReorderHelper.notifyCalled();
                    }
                };
        // Perform registration to make callbacks work.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel.addObserver(bookmarkModelObserver);
        });

        BookmarkFolderRow test =
                (BookmarkFolderRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE, test.getTitle());

        toggleSelectionAndEndAnimation(testId, test);

        // Starts as 0th bookmark (not counting promo header) and ends at the 1st index.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ((ReorderBookmarkItemsAdapter) mItemsContainer.getAdapter()).simulateDragForTests(1, 2);
        });

        modelReorderHelper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<BookmarkId> observed =
                    mBookmarkModel.getChildIDs(mBookmarkModel.getDefaultFolder(), true, true);
            // Exclude partner bookmarks folder
            Assert.assertEquals(expected, observed.subList(0, 3));
            Assert.assertTrue("The selected item should stay selected", test.isItemSelected());
        });
    }

    @Test
    @MediumTest
    public void testPromoDraggability() throws Exception {
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);

        forceSyncHeaderState();
        openBookmarkManager();

        ViewHolder promo = mItemsContainer.findViewHolderForAdapterPosition(0);

        toggleSelectionAndEndAnimation(
                testId, (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView);

        ReorderBookmarkItemsAdapter adapter =
                ((ReorderBookmarkItemsAdapter) mItemsContainer.getAdapter());
        Assert.assertFalse("Promo header should not be passively draggable",
                adapter.isPassivelyDraggable(promo));
        Assert.assertFalse("Promo header should not be actively draggable",
                adapter.isActivelyDraggable(promo));
    }

    @Test
    @MediumTest
    public void testPartnerFolderDraggability() throws Exception {
        BookmarkId testId = addFolderWithPartner(TEST_FOLDER_TITLE);
        forceSyncHeaderState();
        openBookmarkManager();

        ViewHolder partner = mItemsContainer.findViewHolderForAdapterPosition(2);

        toggleSelectionAndEndAnimation(
                testId, (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView);

        ReorderBookmarkItemsAdapter adapter =
                ((ReorderBookmarkItemsAdapter) mItemsContainer.getAdapter());
        Assert.assertFalse("Partner bookmarks folder should not be passively draggable",
                adapter.isPassivelyDraggable(partner));
        Assert.assertFalse("Partner bookmarks folder should not be actively draggable",
                adapter.isActivelyDraggable(partner));
    }

    @Test
    @MediumTest
    public void testUnselectedItemDraggability() throws Exception {
        BookmarkId aId = addBookmark("a", "http://a.com");
        addFolder(TEST_FOLDER_TITLE);

        forceSyncHeaderState();
        openBookmarkManager();

        ViewHolder test = mItemsContainer.findViewHolderForAdapterPosition(1);
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) test.itemView).getTitle());

        toggleSelectionAndEndAnimation(
                aId, (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView);

        ReorderBookmarkItemsAdapter adapter =
                ((ReorderBookmarkItemsAdapter) mItemsContainer.getAdapter());
        Assert.assertTrue("Unselected rows should be passively draggable",
                adapter.isPassivelyDraggable(test));
        Assert.assertFalse("Unselected rows should not be actively draggable",
                adapter.isActivelyDraggable(test));
    }

    @Test
    @MediumTest
    public void testCannotSelectPromo() throws Exception {
        addFolder(TEST_FOLDER_TITLE);

        forceSyncHeaderState();
        openBookmarkManager();

        View promo = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        TouchCommon.longPressView(promo);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        Assert.assertFalse("Expected that we would not be in selection mode "
                        + "after long pressing on promo view.",
                mManager.getSelectionDelegate().isSelectionEnabled());
    }

    @Test
    @MediumTest
    public void testCannotSelectPartner() throws Exception {
        addFolderWithPartner(TEST_FOLDER_TITLE);
        forceSyncHeaderState();
        openBookmarkManager();

        View partner = mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        TouchCommon.longPressView(partner);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        Assert.assertFalse("Expected that we would not be in selection mode "
                        + "after long pressing on partner bookmark.",
                mManager.getSelectionDelegate().isSelectionEnabled());
    }

    @Test
    @MediumTest
    public void testMoveUpMenuItem() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, TEST_URL_A);
        addFolder(TEST_FOLDER_TITLE);
        forceSyncHeaderState();
        openBookmarkManager();

        View google = mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_PAGE_TITLE_GOOGLE,
                ((BookmarkItemRow) google).getTitle());
        View more = google.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move up")).perform(click());

        // Confirm that the "Google" bookmark is now on top, and that the "test" folder is 2nd
        Assert.assertTrue(
                ((BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView)
                        .getTitle()
                        .equals(TEST_PAGE_TITLE_GOOGLE));
        Assert.assertTrue(
                ((BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView)
                        .getTitle()
                        .equals(TEST_FOLDER_TITLE));
    }

    @Test
    @MediumTest
    public void testMoveDownMenuItem() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, TEST_URL_A);
        addFolder(TEST_FOLDER_TITLE);
        forceSyncHeaderState();
        openBookmarkManager();

        View testFolder = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolder).getTitle());
        ListMenuButton more = testFolder.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move down")).perform(click());

        // Confirm that the "Google" bookmark is now on top, and that the "test" folder is 2nd
        Assert.assertTrue(
                ((BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView)
                        .getTitle()
                        .equals(TEST_PAGE_TITLE_GOOGLE));
        Assert.assertTrue(
                ((BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(2).itemView)
                        .getTitle()
                        .equals(TEST_FOLDER_TITLE));
    }

    @Test
    @MediumTest
    public void testMoveDownGoneForBottomElement() throws Exception {
        addBookmarkWithPartner(TEST_PAGE_TITLE_GOOGLE, TEST_URL_A);
        addFolderWithPartner(TEST_FOLDER_TITLE);
        forceSyncHeaderState();
        openBookmarkManager();

        View google = mItemsContainer.findViewHolderForAdapterPosition(2).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_PAGE_TITLE_GOOGLE,
                ((BookmarkItemRow) google).getTitle());
        View more = google.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveUpGoneForTopElement() throws Exception {
        addBookmark(TEST_PAGE_TITLE_GOOGLE, TEST_URL_A);
        addFolder(TEST_FOLDER_TITLE);
        forceSyncHeaderState();
        openBookmarkManager();

        View testFolder = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolder).getTitle());
        ListMenuButton more = testFolder.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);
        onView(withText("Move up")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveButtonsGoneInSearchMode() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        openBookmarkManager();

        View searchButton = mManager.getToolbarForTests().findViewById(R.id.search_menu_id);
        TestThreadUtils.runOnUiThreadBlocking(searchButton::performClick);

        // Callback occurs when Item "test" is selected.
        CriteriaHelper.pollUiThread(
                () -> mManager.getToolbarForTests().isSearching(), "Expected to enter search mode");

        View testFolder = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolder).getTitle());
        View more = testFolder.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);

        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveButtonsGoneWithOneBookmark() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        forceSyncHeaderState();
        openBookmarkManager();

        View testFolder = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolder).getTitle());
        View more = testFolder.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::callOnClick);

        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testMoveButtonsGoneForPartnerBookmarks() throws Exception {
        loadFakePartnerBookmarkShimForTesting();
        BookmarkPromoHeader.forcePromoStateForTests(PromoState.PROMO_NONE);
        openBookmarkManager();

        // Open partner bookmarks folder.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.openFolder(mBookmarkModel.getPartnerFolderId()));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        Assert.assertEquals("Wrong number of items in partner bookmark folder.", 2,
                getAdapter().getItemCount());

        // Verify that bookmark 1 is editable (so more button can be triggered) but not movable.
        BookmarkId partnerBookmarkId1 = getReorderAdapter().getIdByPosition(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkBridge.BookmarkItem partnerBookmarkItem1 =
                    mBookmarkModel.getBookmarkById(partnerBookmarkId1);
            partnerBookmarkItem1.forceEditableForTesting();
            Assert.assertEquals("Incorrect bookmark type for item 1", BookmarkType.PARTNER,
                    partnerBookmarkId1.getType());
            Assert.assertFalse(
                    "Partner item 1 should not be movable", partnerBookmarkItem1.isMovable());
            Assert.assertTrue(
                    "Partner item 1 should be editable", partnerBookmarkItem1.isEditable());
        });

        // Verify that bookmark 2 is editable (so more button can be triggered) but not movable.
        View partnerBookmarkView1 = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        View more1 = partnerBookmarkView1.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more1::callOnClick);
        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());

        // Verify that bookmark 2 is not movable.
        BookmarkId partnerBookmarkId2 = getReorderAdapter().getIdByPosition(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BookmarkBridge.BookmarkItem partnerBookmarkItem2 =
                    mBookmarkModel.getBookmarkById(partnerBookmarkId2);
            partnerBookmarkItem2.forceEditableForTesting();
            Assert.assertEquals("Incorrect bookmark type for item 2", BookmarkType.PARTNER,
                    partnerBookmarkId2.getType());
            Assert.assertFalse(
                    "Partner item 2 should not be movable", partnerBookmarkItem2.isMovable());
            Assert.assertTrue(
                    "Partner item 2 should be editable", partnerBookmarkItem2.isEditable());
        });

        // Verify that bookmark 2 does not have move up/down items.
        View partnerBookmarkView2 = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        View more2 = partnerBookmarkView2.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more2::callOnClick);
        onView(withText("Move up")).check(doesNotExist());
        onView(withText("Move down")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testTopLevelFolderUpdateAfterSync() throws Exception {
        // Set up the test and open the bookmark manager to the Mobile Bookmarks folder.
        MockSyncContentResolverDelegate syncDelegate = new MockSyncContentResolverDelegate();
        syncDelegate.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(syncDelegate, null);
        readPartnerBookmarks();
        openBookmarkManager();
        ReorderBookmarkItemsAdapter adapter = getReorderAdapter();

        // Open the root folder.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.openFolder(mBookmarkModel.getRootFolderId()));

        // Add a bookmark to the Other Bookmarks folder.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel.addBookmark(
                    mBookmarkModel.getOtherFolderId(), 0, TEST_TITLE_A, TEST_URL_A);
        });

        // Dismiss promo header and simulate a sign in.
        syncDelegate.setMasterSyncAutomatically(false);
        TestThreadUtils.runOnUiThreadBlocking(adapter::simulateSignInForTests);
        Assert.assertEquals(
                "Expected \"Other Bookmarks\" folder to appear!", 2, adapter.getItemCount());
    }

    @Test
    @MediumTest
    @Features.DisableFeatures(ChromeFeatureList.BOOKMARKS_SHOW_IN_FOLDER)
    public void testShowInFolderDisabled() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        forceSyncHeaderState();
        openBookmarkManager();
        enterSearch();
        clickMoreButtonOnFirstItem(TEST_FOLDER_TITLE);

        onView(withText("Show in folder")).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void testShowInFolder_NoScroll() throws Exception {
        addFolder(TEST_FOLDER_TITLE);
        forceSyncHeaderState();
        openBookmarkManager();

        // Enter search mode.
        enterSearch();

        // Click "Show in folder".
        View testFolder = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        clickMoreButtonOnFirstItem(TEST_FOLDER_TITLE);
        onView(withText("Show in folder")).perform(click());

        // Assert that the view pulses.
        Assert.assertTrue("Expected bookmark row to pulse after clicking \"show in folder\"!",
                checkHighlightPulse(testFolder));

        // Enter search mode again.
        enterSearch();

        Assert.assertTrue("Expected bookmark row to not be highlighted "
                        + "after entering search mode",
                checkHighlightOff(testFolder));

        // Click "Show in folder" again.
        clickMoreButtonOnFirstItem(TEST_FOLDER_TITLE);
        onView(withText("Show in folder")).perform(click());
        Assert.assertTrue(
                "Expected bookmark row to pulse after clicking \"show in folder\" a 2nd time!",
                checkHighlightPulse(testFolder));
    }

    @Test
    @MediumTest
    public void testShowInFolder_Scroll() throws Exception {
        addFolder(TEST_FOLDER_TITLE); // Index 8
        addBookmark(TEST_TITLE_A, TEST_URL_A);
        addBookmark(TEST_PAGE_TITLE_FOO, "http://foo.com");
        addFolder(TEST_PAGE_TITLE_GOOGLE2);
        addFolder("B");
        addFolder("C");
        addFolder("D");
        addFolder("E"); // Index 1
        forceSyncHeaderState();
        openBookmarkManager();

        // Enter search mode.
        enterSearch();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mManager.onSearchTextChanged(TEST_FOLDER_TITLE));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // This should be the only (& therefore 0-indexed) item.
        clickMoreButtonOnFirstItem(TEST_FOLDER_TITLE);

        // Show in folder.
        onView(withText("Show in folder")).perform(click());

        // This should be in the 8th position now.
        ViewHolder testFolderInList = mItemsContainer.findViewHolderForAdapterPosition(8);
        Assert.assertFalse(
                "Expected list to scroll bookmark item into view", testFolderInList == null);
        Assert.assertEquals("Wrong bookmark item selected.", TEST_FOLDER_TITLE,
                ((BookmarkFolderRow) testFolderInList.itemView).getTitle());
        Assert.assertTrue("Expected highlight to pulse on after scrolling to the item!",
                checkHighlightPulse(testFolderInList.itemView));
    }

    @Test
    @MediumTest
    public void testShowInFolder_OpenOtherFolder() throws Exception {
        BookmarkId testId = addFolder(TEST_FOLDER_TITLE);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addBookmark(testId, 0, TEST_TITLE_A, TEST_URL_A));
        forceSyncHeaderState();
        openBookmarkManager();

        // Enter search mode.
        enterSearch();
        TestThreadUtils.runOnUiThreadBlocking(() -> mManager.onSearchTextChanged(TEST_URL_A));
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // This should be the only (& therefore 0-indexed) item.
        clickMoreButtonOnFirstItem(TEST_TITLE_A);

        // Show in folder.
        onView(withText("Show in folder")).perform(click());
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        // Make sure that we're in the right folder (index 1 because of promo).
        View itemA = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_TITLE_A,
                ((BookmarkItemRow) itemA).getTitle());

        Assert.assertTrue("Expected highlight to pulse after opening an item in another folder!",
                checkHighlightPulse(itemA));

        // Open mobile bookmarks folder, then go back to the subfolder.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mManager.openFolder(mBookmarkModel.getMobileFolderId());
            mManager.openFolder(testId);
        });
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);

        View itemASecondView = mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", TEST_TITLE_A,
                ((BookmarkItemRow) itemASecondView).getTitle());
        Assert.assertTrue(
                "Expected highlight to not be highlighted after exiting and re-entering folder!",
                checkHighlightOff(itemASecondView));
    }

    @Test
    @SmallTest
    public void testAddBookmarkInBackgroundWithSelection() throws Exception {
        BookmarkId id = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo);
        BookmarkPromoHeader.forcePromoStateForTests(PromoState.PROMO_NONE);
        // Force empty partner bookmark folder to keep set of bookmark items consistent across
        // devices.
        loadEmptyPartnerBookmarksForTesting();
        openBookmarkManager();
        Assert.assertEquals(1, getAdapter().getItemCount());
        BookmarkRow row =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        toggleSelectionAndEndAnimation(id, row);
        CallbackHelper helper = new CallbackHelper();
        getAdapter().registerAdapterDataObserver(new AdapterDataObserver() {
            @Override
            public void onChanged() {
                helper.notifyCalled();
            }
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mBookmarkModel.addBookmark(
                    mBookmarkModel.getDefaultFolder(), 1, TEST_PAGE_TITLE_GOOGLE, mTestPage);
        });

        helper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(isItemPresentInBookmarkList(TEST_PAGE_TITLE_FOO));
            Assert.assertTrue(isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
            Assert.assertEquals(2, getAdapter().getItemCount());
            Assert.assertTrue("The selected row should be kept selected", row.isItemSelected());
        });
    }

    @Test
    @SmallTest
    public void testDeleteAllSelectedBookmarksInBackground() throws Exception {
        // selected on bookmark and then remove that in background
        // in the meantime, the toolbar changes from selection mode to normal mode
        BookmarkId fooId = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo);
        BookmarkId googleId = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        BookmarkId aId = addBookmark(TEST_TITLE_A, TEST_URL_A);
        BookmarkPromoHeader.forcePromoStateForTests(PromoState.PROMO_NONE);
        // Force empty partner bookmark folder to keep set of bookmark items consistent across
        // devices.
        loadEmptyPartnerBookmarksForTesting();
        openBookmarkManager();
        Assert.assertEquals(3, getAdapter().getItemCount());
        BookmarkRow row =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        toggleSelectionAndEndAnimation(googleId, row);
        CallbackHelper helper = new CallbackHelper();
        mManager.getSelectionDelegate().addObserver((x) -> { helper.notifyCalled(); });

        removeBookmark(googleId);

        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        helper.waitForCallback(0, 1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(
                    "Item is not deleted", isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
            Assert.assertEquals(2, getReorderAdapter().getItemCount());
            Assert.assertEquals("Bookmark View should be back to normal view",
                    mManager.getToolbarForTests().getCurrentViewType(), ViewType.NORMAL_VIEW);
        });
    }

    @Test
    @SmallTest
    public void testDeleteSomeSelectedBookmarksInBackground() throws Exception {
        // selected on bookmarks and then remove one of them in background
        // in the meantime, the toolbar stays in selection mode
        BookmarkId fooId = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo);
        BookmarkId googleId = addBookmark(TEST_PAGE_TITLE_GOOGLE, mTestPage);
        BookmarkId aId = addBookmark(TEST_TITLE_A, TEST_URL_A);
        BookmarkPromoHeader.forcePromoStateForTests(PromoState.PROMO_NONE);
        // Force empty partner bookmark folder to keep set of bookmark items consistent across
        // devices.
        loadEmptyPartnerBookmarksForTesting();
        openBookmarkManager();
        Assert.assertEquals(3, getAdapter().getItemCount());
        BookmarkRow row =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(1).itemView;
        toggleSelectionAndEndAnimation(googleId, row);
        BookmarkRow aRow =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        toggleSelectionAndEndAnimation(aId, aRow);
        CallbackHelper helper = new CallbackHelper();
        mManager.getSelectionDelegate().addObserver((x) -> { helper.notifyCalled(); });

        removeBookmark(googleId);

        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        helper.waitForCallback(0, 1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(
                    "Item is not deleted", isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
            Assert.assertEquals(2, getReorderAdapter().getItemCount());
            Assert.assertTrue("Item selected should not be cleared", aRow.isItemSelected());
            Assert.assertEquals("Should stay in selection mode because there is one selected",
                    mManager.getToolbarForTests().getCurrentViewType(), ViewType.SELECTION_VIEW);
        });
    }

    @Test
    @SmallTest
    public void testUpdateSelectedBookmarkInBackground() throws Exception {
        BookmarkId id = addBookmark(TEST_PAGE_TITLE_FOO, mTestPageFoo);
        BookmarkPromoHeader.forcePromoStateForTests(PromoState.PROMO_NONE);
        // Force empty partner bookmark folder to keep set of bookmark items consistent across
        // devices.
        loadEmptyPartnerBookmarksForTesting();
        openBookmarkManager();
        Assert.assertEquals(1, getAdapter().getItemCount());
        BookmarkRow row =
                (BookmarkRow) mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        toggleSelectionAndEndAnimation(id, row);
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addObserver(new BookmarkModelObserver() {
                    @Override
                    public void bookmarkModelChanged() {
                        helper.notifyCalled();
                    }
                }));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.setBookmarkTitle(id, TEST_PAGE_TITLE_GOOGLE));

        helper.waitForCallback(0, 1);
        RecyclerViewTestUtils.waitForStableRecyclerView(mItemsContainer);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(isItemPresentInBookmarkList(TEST_PAGE_TITLE_FOO));
            Assert.assertTrue(isItemPresentInBookmarkList(TEST_PAGE_TITLE_GOOGLE));
            Assert.assertEquals(1, getAdapter().getItemCount());
            Assert.assertTrue("The selected row should stay selected", row.isItemSelected());
        });
    }

    @Override
    protected void openBookmarkManager() throws InterruptedException {
        super.openBookmarkManager();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mManager.getDragStateDelegate().setA11yStateForTesting(false);
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
    private BookmarkId addBookmarkWithPartner(String title, String url) throws ExecutionException {
        loadEmptyPartnerBookmarksForTesting();
        return TestThreadUtils.runOnUiThreadBlocking(
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
        loadEmptyPartnerBookmarksForTesting();
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> mBookmarkModel.addFolder(mBookmarkModel.getDefaultFolder(), 0, title));
    }

    /**
     * Ignores the Android sync settings, and forces a sync header for tests.
     */
    private void forceSyncHeaderState() {
        MockSyncContentResolverDelegate syncDelegate = new MockSyncContentResolverDelegate();
        syncDelegate.setMasterSyncAutomatically(true);
        AndroidSyncSettings.overrideForTests(syncDelegate, null);
        BookmarkPromoHeader.forcePromoStateForTests(BookmarkPromoHeader.PromoState.PROMO_SYNC);
    }

    private ReorderBookmarkItemsAdapter getReorderAdapter() {
        return (ReorderBookmarkItemsAdapter) getAdapter();
    }

    private void enterSearch() throws Exception {
        View searchButton = mManager.getToolbarForTests().findViewById(R.id.search_menu_id);
        TestThreadUtils.runOnUiThreadBlocking(searchButton::performClick);
        CriteriaHelper.pollUiThread(
                () -> mManager.getToolbarForTests().isSearching(), "Expected to enter search mode");
    }

    private void clickMoreButtonOnFirstItem(String expectedBookmarkItemTitle) throws Exception {
        View firstItem = mItemsContainer.findViewHolderForAdapterPosition(0).itemView;
        Assert.assertEquals("Wrong bookmark item selected.", expectedBookmarkItemTitle,
                firstItem instanceof BookmarkItemRow ? ((BookmarkItemRow) firstItem).getTitle()
                                                     : ((BookmarkFolderRow) firstItem).getTitle());
        View more = firstItem.findViewById(R.id.more);
        TestThreadUtils.runOnUiThreadBlocking(more::performClick);
    }

    @Override
    protected BookmarkManager getBookmarkManager() {
        return (BookmarkManager) getReorderAdapter().getDelegateForTesting();
    }

    @Override
    protected BookmarkId getIdByPosition(int pos) {
        return getReorderAdapter().getIdByPosition(pos);
    }

    @Override
    protected void searchBookmarks(final String query) {
        TestThreadUtils.runOnUiThreadBlocking(() -> getReorderAdapter().search(query));
    }
}