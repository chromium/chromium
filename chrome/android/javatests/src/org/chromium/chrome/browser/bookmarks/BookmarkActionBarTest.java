// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.Instrumentation.ActivityMonitor;
import android.graphics.Color;
import android.support.test.InstrumentationRegistry;
import android.view.MenuItem;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.espresso.core.deps.guava.primitives.Ints;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.bookmarks.BookmarkAddEditFolderActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderSelectActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** On device unit test for {@link BookmarkActionBar}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class BookmarkActionBarTest extends BlankUiTestActivityTestCase {
    private static final List<Integer> SELECTION_MENU_IDS =
            Arrays.asList(R.id.selection_mode_edit_menu_id, R.id.selection_mode_move_menu_id,
                    R.id.selection_mode_delete_menu_id, R.id.selection_open_in_new_tab_id,
                    R.id.selection_open_in_incognito_tab_id);
    private static final BookmarkId BOOKMARK_ID_ROOT = new BookmarkId(0, BookmarkType.NORMAL);
    private static final BookmarkId BOOKMARK_ID_FOLDER = new BookmarkId(1, BookmarkType.NORMAL);
    private static final BookmarkId BOOKMARK_ID_ONE = new BookmarkId(2, BookmarkType.NORMAL);
    private static final BookmarkId BOOKMARK_ID_TWO = new BookmarkId(3, BookmarkType.NORMAL);
    private static final BookmarkId BOOKMARK_ID_PARTNER = new BookmarkId(4, BookmarkType.PARTNER);
    private static final BookmarkId BOOKMARK_ID_READING_LIST =
            new BookmarkId(5, BookmarkType.READING_LIST);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    BookmarkDelegate mBookmarkDelegate;
    @Mock
    SelectionDelegate<BookmarkId> mSelectionDelegate;
    @Mock
    SearchDelegate mSearchDelegate;
    @Mock
    BookmarkModel mBookmarkModel;

    private Activity mActivity;
    private WindowAndroid mWindowAndroid;
    private ViewGroup mContentView;
    private BookmarkActionBar mBookmarkActionBar;
    private final List<ActivityMonitor> mActivityMonitorList = new ArrayList<>();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mBookmarkDelegate.getModel()).thenReturn(mBookmarkModel);
        when(mBookmarkDelegate.getSelectionDelegate()).thenReturn(mSelectionDelegate);

        IncognitoUtils.setEnabledForTesting(true);

        mActivity = getActivity();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mWindowAndroid = new WindowAndroid(mActivity);
            mContentView = new LinearLayout(mActivity);
            mContentView.setBackgroundColor(Color.WHITE);
            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            mActivity.setContentView(mContentView, params);

            mBookmarkActionBar = mActivity.getLayoutInflater()
                                         .inflate(R.layout.bookmark_action_bar, mContentView, true)
                                         .findViewById(R.id.bookmark_action_bar);

            when(mBookmarkModel.getRootFolderId()).thenReturn(BOOKMARK_ID_ROOT);
            when(mBookmarkModel.getTopLevelFolderParentIDs())
                    .thenReturn(Collections.singletonList(BOOKMARK_ID_ROOT));

            mockBookmarkItem(BOOKMARK_ID_FOLDER, "folder", null, true, BOOKMARK_ID_ROOT);
            mockBookmarkItem(
                    BOOKMARK_ID_ONE, "one", JUnitTestGURLs.URL_1, false, BOOKMARK_ID_FOLDER);
            mockBookmarkItem(
                    BOOKMARK_ID_TWO, "two", JUnitTestGURLs.URL_2, false, BOOKMARK_ID_FOLDER);
            mockBookmarkItem(BOOKMARK_ID_PARTNER, "partner", JUnitTestGURLs.RED_1, false,
                    BOOKMARK_ID_FOLDER);
            mockBookmarkItem(BOOKMARK_ID_READING_LIST, "reading list", JUnitTestGURLs.BLUE_1, false,
                    BOOKMARK_ID_FOLDER);
        });
    }

    @After
    public void tearDown() {
        IncognitoUtils.setEnabledForTesting(null);

        // Since these monitors block the creation of activities, it is crucial that they're removed
        // so that when batching tests the subsequent cases actually see their activities.
        for (ActivityMonitor activityMonitor : mActivityMonitorList) {
            InstrumentationRegistry.getInstrumentation().removeMonitor(activityMonitor);
        }
        mActivityMonitorList.clear();

        TestThreadUtils.runOnUiThreadBlocking(() -> { mWindowAndroid.destroy(); });
    }

    private void initializeNormal() {
        mBookmarkActionBar.initialize(mSelectionDelegate, 0, R.id.normal_menu_group,
                R.id.selection_mode_menu_group, false);
        mBookmarkActionBar.onBookmarkDelegateInitialized(mBookmarkDelegate);
        mBookmarkActionBar.initializeSearchView(
                mSearchDelegate, R.string.bookmark_action_bar_search, R.id.search_menu_id);
    }

    private void initializeAsDialog() {
        when(mBookmarkDelegate.isDialogUi()).thenReturn(true);
        initializeNormal();
    }

    private void mockBookmarkItem(
            BookmarkId bookmarkId, String title, String url, boolean isFolder, BookmarkId parent) {
        BookmarkItem bookmarkItem = new BookmarkItem(
                bookmarkId, title, new GURL(url), isFolder, parent, false, false, 0, false);
        when(mBookmarkModel.getBookmarkById(bookmarkId)).thenReturn(bookmarkItem);
    }

    /**
     * {@link SelectionDelegate} has two accessors to get the currently selected {@link BookmarkId}
     * objects. Instead of each test case trying to set mocks for the specific calls that are
     * invoked, this helper sets up both every time.
     */
    private void setCurrentSelection(BookmarkId... bookmarkIdArray) {
        List<BookmarkId> bookmarkIdList = Arrays.asList(bookmarkIdArray);
        when(mSelectionDelegate.getSelectedItemsAsList()).thenReturn(bookmarkIdList);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(new HashSet<>(bookmarkIdList));
    }

    private ActivityMonitor addBlockingActivityMonitor(Class clazz) {
        ActivityMonitor activityMonitor = new ActivityMonitor(clazz.getName(), null, true);
        InstrumentationRegistry.getInstrumentation().addMonitor(activityMonitor);
        mActivityMonitorList.add(activityMonitor);
        return activityMonitor;
    }

    private void verifySelectionMenuVisibility(int... hiddenMenuIds) {
        verifyMenuVisibility(SELECTION_MENU_IDS, hiddenMenuIds);
    }

    private void verifyMenuVisibility(List<Integer> applicableMenuIds, int... hiddenMenuIds) {
        Set<Integer> hiddenIdSet = new HashSet<>(Ints.asList(hiddenMenuIds));
        for (int menuId : applicableMenuIds) {
            boolean isVisible = !hiddenIdSet.contains(menuId);
            MenuItem menuItem = mBookmarkActionBar.getMenu().findItem(menuId);
            Assert.assertNotNull(menuId);
            Assert.assertEquals("Mismatched visibility for menu item " + menuItem, isVisible,
                    menuItem.isVisible());
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void onNavigationBack() {
        initializeNormal();
        mBookmarkActionBar.onFolderStateSet(BOOKMARK_ID_FOLDER);
        mBookmarkActionBar.onNavigationBack();
        Mockito.verify(mBookmarkDelegate, Mockito.times(1)).openFolder(BOOKMARK_ID_ROOT);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void onNavigationBack_searching() {
        initializeNormal();
        mBookmarkActionBar.showSearchView(false);
        mBookmarkActionBar.onNavigationBack();
        Assert.assertFalse(mBookmarkActionBar.isSearching());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnMenuItemClick_editMenu() {
        ActivityMonitor activityMonitor =
                addBlockingActivityMonitor(BookmarkAddEditFolderActivity.class);
        initializeNormal();

        mBookmarkActionBar.onFolderStateSet(BOOKMARK_ID_FOLDER);
        Assert.assertEquals(0, activityMonitor.getHits());

        Assert.assertTrue(mBookmarkActionBar.onMenuItemClick(
                mBookmarkActionBar.getMenu().findItem(R.id.edit_menu_id)));
        Assert.assertEquals(1, activityMonitor.getHits());
    }
    @Test
    @SmallTest
    @UiThreadTest
    public void testOnMenuItemClick_closeMenu() {
        initializeAsDialog();

        MenuItem menuItem = mBookmarkActionBar.getMenu().findItem(R.id.close_menu_id);
        Assert.assertNotNull(menuItem);
        Assert.assertTrue(mBookmarkActionBar.onMenuItemClick(menuItem));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnMenuItemClick_searchMenu() {
        initializeNormal();
        Assert.assertTrue(mBookmarkActionBar.onMenuItemClick(
                mBookmarkActionBar.getMenu().findItem(R.id.search_menu_id)));
        Mockito.verify(mBookmarkDelegate, Mockito.times(1)).openSearchUI();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnMenuItemClick_selectionModeEditMenu() {
        ActivityMonitor activityMonitor = addBlockingActivityMonitor(BookmarkEditActivity.class);
        initializeNormal();
        setCurrentSelection(BOOKMARK_ID_ONE);

        Assert.assertTrue(mBookmarkActionBar.onMenuItemClick(
                mBookmarkActionBar.getMenu().findItem(R.id.selection_mode_edit_menu_id)));
        Assert.assertEquals(1, activityMonitor.getHits());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnMenuItemClick_selectionModeEditMenuFolder() {
        ActivityMonitor activityMonitor =
                addBlockingActivityMonitor(BookmarkAddEditFolderActivity.class);
        initializeNormal();
        setCurrentSelection(BOOKMARK_ID_FOLDER);

        Assert.assertTrue(mBookmarkActionBar.onMenuItemClick(
                mBookmarkActionBar.getMenu().findItem(R.id.selection_mode_edit_menu_id)));
        Assert.assertEquals(1, activityMonitor.getHits());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnMenuItemClick_selectionModeMoveMenu() {
        ActivityMonitor activityMonitor =
                addBlockingActivityMonitor(BookmarkFolderSelectActivity.class);
        initializeNormal();
        setCurrentSelection(BOOKMARK_ID_ONE, BOOKMARK_ID_TWO);

        Assert.assertTrue(mBookmarkActionBar.onMenuItemClick(
                mBookmarkActionBar.getMenu().findItem(R.id.selection_mode_move_menu_id)));
        Assert.assertEquals(1, activityMonitor.getHits());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnMenuItemClick_selectionModeDeleteMenu() {
        initializeNormal();
        setCurrentSelection(BOOKMARK_ID_ONE, BOOKMARK_ID_TWO);

        Assert.assertTrue(mBookmarkActionBar.onMenuItemClick(
                mBookmarkActionBar.getMenu().findItem(R.id.selection_mode_delete_menu_id)));
        verify(mBookmarkModel, Mockito.times(1)).deleteBookmarks(Mockito.any());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnBookmarkDelegateInitialized() {
        mBookmarkActionBar.onBookmarkDelegateInitialized(mBookmarkDelegate);
        Assert.assertNull(mBookmarkActionBar.getMenu().findItem(R.id.close_menu_id));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnBookmarkDelegateInitialized_isDialog() {
        when(mBookmarkDelegate.isDialogUi()).thenReturn(true);
        mBookmarkActionBar.onBookmarkDelegateInitialized(mBookmarkDelegate);
        Assert.assertNotNull(mBookmarkActionBar.getMenu().findItem(R.id.close_menu_id));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnDestroy() {
        mBookmarkActionBar.onBookmarkDelegateInitialized(mBookmarkDelegate);
        mBookmarkActionBar.onDestroy();
        Mockito.verify(mBookmarkDelegate, Mockito.times(1)).removeUIObserver(mBookmarkActionBar);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnDestroy_nullDelegate() {
        mBookmarkActionBar.onDestroy();
        Mockito.verify(mBookmarkDelegate, Mockito.never()).removeUIObserver(mBookmarkActionBar);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnSearchStateSet() {
        mBookmarkActionBar.onSearchStateSet();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnSelectionStateChange_nullBookmarkDelegate() {
        mBookmarkActionBar.initialize(mSelectionDelegate, 0, R.id.normal_menu_group,
                R.id.selection_mode_menu_group, false);
        mBookmarkActionBar.initializeSearchView(
                mSearchDelegate, R.string.bookmark_action_bar_search, R.id.search_menu_id);
        mBookmarkActionBar.onSelectionStateChange(Collections.singletonList(BOOKMARK_ID_ONE));

        verifySelectionMenuVisibility(R.id.selection_mode_edit_menu_id,
                R.id.selection_mode_move_menu_id, R.id.selection_mode_delete_menu_id,
                R.id.selection_open_in_new_tab_id, R.id.selection_open_in_incognito_tab_id);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnSelectionStateChange_selectionNotEnabled() {
        initializeNormal();
        mBookmarkActionBar.onSelectionStateChange(Collections.emptyList());

        verifySelectionMenuVisibility(R.id.selection_mode_edit_menu_id,
                R.id.selection_mode_move_menu_id, R.id.selection_mode_delete_menu_id,
                R.id.selection_open_in_new_tab_id, R.id.selection_open_in_incognito_tab_id);
        Mockito.verify(mBookmarkDelegate, Mockito.times(1)).notifyStateChange(mBookmarkActionBar);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnSelectionStateChange_multiple() {
        initializeNormal();
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);

        mBookmarkActionBar.onSelectionStateChange(Arrays.asList(BOOKMARK_ID_ONE, BOOKMARK_ID_TWO));
        verifySelectionMenuVisibility(R.id.selection_mode_edit_menu_id);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnSelectionStateChange_incognitoDisabled() {
        IncognitoUtils.setEnabledForTesting(false);
        initializeNormal();
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);

        mBookmarkActionBar.onSelectionStateChange(Collections.singletonList(BOOKMARK_ID_ONE));
        verifySelectionMenuVisibility(R.id.selection_open_in_incognito_tab_id);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnSelectionStateChange_folder() {
        initializeNormal();
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);

        mBookmarkActionBar.onSelectionStateChange(Collections.singletonList(BOOKMARK_ID_FOLDER));
        verifySelectionMenuVisibility(
                R.id.selection_open_in_new_tab_id, R.id.selection_open_in_incognito_tab_id);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnSelectionStateChange_partner() {
        initializeNormal();
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);

        mBookmarkActionBar.onSelectionStateChange(Collections.singletonList(BOOKMARK_ID_PARTNER));
        verifySelectionMenuVisibility(R.id.selection_mode_move_menu_id);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnSelectionStateChange_readingList() {
        initializeNormal();
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);

        mBookmarkActionBar.onSelectionStateChange(
                Collections.singletonList(BOOKMARK_ID_READING_LIST));
        verifySelectionMenuVisibility();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnDragStateChange() {
        initializeNormal();

        mBookmarkActionBar.onDragStateChange(true);
        Assert.assertFalse(mBookmarkActionBar.getMenu()
                                   .findItem(R.id.selection_mode_edit_menu_id)
                                   .isEnabled());

        mBookmarkActionBar.onDragStateChange(false);
        Assert.assertTrue(mBookmarkActionBar.getMenu()
                                  .findItem(R.id.selection_mode_edit_menu_id)
                                  .isEnabled());
    }
}