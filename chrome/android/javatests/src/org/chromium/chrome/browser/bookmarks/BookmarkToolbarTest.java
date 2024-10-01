// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.Instrumentation.ActivityMonitor;
import android.graphics.Color;
import android.view.MenuItem;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.annotation.IdRes;
import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import com.google.common.primitives.Ints;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
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

/** On device unit test for {@link BookmarkToolbar}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class BookmarkToolbarTest extends BlankUiTestActivityTestCase {
    private static final List<Integer> SELECTION_MENU_IDS =
            Arrays.asList(
                    R.id.selection_mode_edit_menu_id,
                    R.id.selection_mode_move_menu_id,
                    R.id.selection_mode_delete_menu_id,
                    R.id.selection_open_in_new_tab_id,
                    R.id.selection_open_in_incognito_tab_id,
                    R.id.reading_list_mark_as_read_id,
                    R.id.reading_list_mark_as_unread_id);
    private static final BookmarkId BOOKMARK_ID_ROOT = new BookmarkId(0, BookmarkType.NORMAL);
    private static final BookmarkId BOOKMARK_ID_FOLDER = new BookmarkId(1, BookmarkType.NORMAL);
    private static final BookmarkId BOOKMARK_ID_ONE = new BookmarkId(2, BookmarkType.NORMAL);
    private static final BookmarkId BOOKMARK_ID_TWO = new BookmarkId(3, BookmarkType.NORMAL);
    private static final BookmarkId BOOKMARK_ID_PARTNER = new BookmarkId(4, BookmarkType.PARTNER);
    private static final BookmarkId BOOKMARK_ID_READING_LIST =
            new BookmarkId(5, BookmarkType.READING_LIST);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock BookmarkDelegate mBookmarkDelegate;
    @Mock SelectionDelegate<BookmarkId> mSelectionDelegate;
    @Mock SearchDelegate mSearchDelegate;
    @Mock BookmarkModel mBookmarkModel;
    @Mock BookmarkOpener mBookmarkOpener;
    @Mock Runnable mNavigateBackRunnable;
    @Mock Profile mProfile;

    private Activity mActivity;
    private WindowAndroid mWindowAndroid;
    private ViewGroup mContentView;
    private BookmarkToolbar mBookmarkToolbar;

    private final List<ActivityMonitor> mActivityMonitorList = new ArrayList<>();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mBookmarkDelegate.getModel()).thenReturn(mBookmarkModel);
        when(mBookmarkDelegate.getSelectionDelegate()).thenReturn(mSelectionDelegate);

        ProfileManager.setLastUsedProfileForTesting(mProfile);
        IncognitoUtils.setEnabledForTesting(true);

        mActivity = getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWindowAndroid = new WindowAndroid(mActivity);
                    mContentView = new LinearLayout(mActivity);
                    mContentView.setBackgroundColor(Color.WHITE);
                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);
                    mActivity.setContentView(mContentView, params);

                    mBookmarkToolbar =
                            mActivity
                                    .getLayoutInflater()
                                    .inflate(R.layout.bookmark_toolbar, mContentView, true)
                                    .findViewById(R.id.bookmark_toolbar);
                    mBookmarkToolbar.setSortMenuIds(BookmarkToolbarMediator.SORT_MENU_IDS);

                    when(mBookmarkModel.getRootFolderId()).thenReturn(BOOKMARK_ID_ROOT);

                    BookmarkItem rootBookmarkItem =
                            new BookmarkItem(
                                    BOOKMARK_ID_ROOT,
                                    "root",
                                    null,
                                    false,
                                    null,
                                    false,
                                    false,
                                    0,
                                    false,
                                    0,
                                    false);
                    when(mBookmarkModel.getBookmarkById(BOOKMARK_ID_ROOT))
                            .thenReturn(rootBookmarkItem);

                    mockBookmarkItem(
                            BOOKMARK_ID_FOLDER, "folder", null, true, BOOKMARK_ID_ROOT, true);
                    mockBookmarkItem(
                            BOOKMARK_ID_ONE,
                            "one",
                            JUnitTestGURLs.URL_1.getSpec(),
                            false,
                            BOOKMARK_ID_FOLDER,
                            true);
                    mockBookmarkItem(
                            BOOKMARK_ID_TWO,
                            "two",
                            JUnitTestGURLs.URL_2.getSpec(),
                            false,
                            BOOKMARK_ID_FOLDER,
                            true);
                    mockBookmarkItem(
                            BOOKMARK_ID_PARTNER,
                            "partner",
                            JUnitTestGURLs.RED_1.getSpec(),
                            false,
                            BOOKMARK_ID_FOLDER,
                            false);
                    mockBookmarkItem(
                            BOOKMARK_ID_READING_LIST,
                            "reading list",
                            JUnitTestGURLs.BLUE_1.getSpec(),
                            false,
                            BOOKMARK_ID_FOLDER,
                            true);
                });
    }

    @After
    public void tearDown() {
        // Since these monitors block the creation of activities, it is crucial that they're removed
        // so that when batching tests the subsequent cases actually see their activities.
        for (ActivityMonitor activityMonitor : mActivityMonitorList) {
            InstrumentationRegistry.getInstrumentation().removeMonitor(activityMonitor);
        }
        mActivityMonitorList.clear();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWindowAndroid.destroy();
                });
    }

    private void initializeNormal() {
        mBookmarkToolbar.initialize(
                mSelectionDelegate,
                0,
                R.id.normal_menu_group,
                R.id.selection_mode_menu_group,
                false);
        mBookmarkToolbar.initializeSearchView(
                mSearchDelegate, R.string.bookmark_toolbar_search, R.id.search_menu_id);
        mBookmarkToolbar.setSortMenuIds(BookmarkToolbarMediator.SORT_MENU_IDS);
        mBookmarkToolbar.setBookmarkOpener(mBookmarkOpener);
        mBookmarkToolbar.setSelectionDelegate(mSelectionDelegate);
        mBookmarkToolbar.setBookmarkUiMode(BookmarkUiMode.FOLDER);
        mBookmarkToolbar.setIsDialogUi(true);
        mBookmarkToolbar.setNavigateBackRunnable(mNavigateBackRunnable);
    }

    private void mockBookmarkItem(
            BookmarkId bookmarkId,
            String title,
            String url,
            boolean isFolder,
            BookmarkId parent,
            boolean isEditable) {
        BookmarkItem bookmarkItem =
                new BookmarkItem(
                        bookmarkId,
                        title,
                        new GURL(url),
                        isFolder,
                        parent,
                        isEditable,
                        false,
                        0,
                        false,
                        0,
                        false);
        when(mBookmarkModel.getBookmarkById(bookmarkId)).thenReturn(bookmarkItem);
    }

    private void verifySelectionMenuVisibility(int... hiddenMenuIds) {
        verifyMenuVisibility(SELECTION_MENU_IDS, hiddenMenuIds);
    }

    private void verifyMenuVisibility(List<Integer> applicableMenuIds, int... hiddenMenuIds) {
        Set<Integer> hiddenIdSet = new HashSet<>(Ints.asList(hiddenMenuIds));
        for (int menuId : applicableMenuIds) {
            boolean isVisible = !hiddenIdSet.contains(menuId);
            MenuItem menuItem = mBookmarkToolbar.getMenu().findItem(menuId);
            assertEquals(
                    "Mismatched visibility for menu item " + menuItem,
                    isVisible,
                    menuItem.isVisible());
        }
    }

    private void verifyMenuEnabled(List<Integer> applicableMenuIds, List<Integer> disabledIds) {
        for (int menuId : applicableMenuIds) {
            boolean isEnabled = !disabledIds.contains(menuId);
            MenuItem menuItem = mBookmarkToolbar.getMenu().findItem(menuId);
            assertEquals(
                    "Mismatched enabled state for menu item " + menuItem,
                    isEnabled,
                    menuItem.isEnabled());
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void onNavigationBack() {
        initializeNormal();
        mBookmarkToolbar.onNavigationBack();
        Mockito.verify(mNavigateBackRunnable).run();
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnMenuItemClick_closeMenu() {
        initializeNormal();

        MenuItem menuItem = mBookmarkToolbar.getMenu().findItem(R.id.close_menu_id);
        assertNotNull(menuItem);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnMenuItemClick_closeMenu_goneWhenNotDialogUi() {
        initializeNormal();
        mBookmarkToolbar.setIsDialogUi(false);

        MenuItem menuItem = mBookmarkToolbar.getMenu().findItem(R.id.close_menu_id);
        assertNull(menuItem);
    }

    private void verifySelectionModeMenuItem(
            Callback<Boolean> visibilityFunction, @IdRes int menuId) {
        initializeNormal();
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);
        mBookmarkToolbar.onSelectionStateChange(Collections.singletonList(BOOKMARK_ID_ONE));

        Set<Integer> mutableMenuItems = new HashSet<>(SELECTION_MENU_IDS);
        assertTrue(
                "Delete should have existed.",
                mutableMenuItems.remove(R.id.selection_mode_delete_menu_id));

        // Initially, all mutable items should be hidden.
        verifySelectionMenuVisibility(Ints.toArray(mutableMenuItems));

        // Trigger the new menu item to show.
        visibilityFunction.onResult(true);

        // Ensure the new item is showing.
        Set<Integer> expectedMeunuItems = new HashSet<>(mutableMenuItems);
        assertTrue(expectedMeunuItems.remove(menuId));
        verifySelectionMenuVisibility(Ints.toArray(expectedMeunuItems));

        // Ensure a subsequent selection event keeps the item visible.
        mBookmarkToolbar.onSelectionStateChange(Collections.singletonList(BOOKMARK_ID_ONE));
        verifySelectionMenuVisibility(Ints.toArray(expectedMeunuItems));

        // Trigger the new menu item to hide.
        visibilityFunction.onResult(false);
        verifySelectionMenuVisibility(Ints.toArray(mutableMenuItems));

        // Ensure a subsequent selection event keeps the item hidden.
        mBookmarkToolbar.onSelectionStateChange(Collections.singletonList(BOOKMARK_ID_ONE));
        verifySelectionMenuVisibility(Ints.toArray(mutableMenuItems));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSelectionShowEdit() {
        verifySelectionModeMenuItem(
                mBookmarkToolbar::setSelectionShowEdit, R.id.selection_mode_edit_menu_id);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSelectionShowOpenInNewTab() {
        verifySelectionModeMenuItem(
                mBookmarkToolbar::setSelectionShowOpenInNewTab, R.id.selection_open_in_new_tab_id);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSelectionShowOpenInIncognitoTab() {
        verifySelectionModeMenuItem(
                mBookmarkToolbar::setSelectionShowOpenInIncognito,
                R.id.selection_open_in_incognito_tab_id);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSelectionShowMove() {
        verifySelectionModeMenuItem(
                mBookmarkToolbar::setSelectionShowMove, R.id.selection_mode_move_menu_id);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSelectionShowMarkRead() {
        verifySelectionModeMenuItem(
                mBookmarkToolbar::setSelectionShowMarkRead, R.id.reading_list_mark_as_read_id);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSelectionShowMarkUnread() {
        verifySelectionModeMenuItem(
                mBookmarkToolbar::setSelectionShowMarkUnread, R.id.reading_list_mark_as_unread_id);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testOnDragStateChange() {
        initializeNormal();

        mBookmarkToolbar.setDragEnabled(true);
        assertFalse(
                mBookmarkToolbar.getMenu().findItem(R.id.selection_mode_edit_menu_id).isEnabled());

        mBookmarkToolbar.setDragEnabled(false);
        assertTrue(
                mBookmarkToolbar.getMenu().findItem(R.id.selection_mode_edit_menu_id).isEnabled());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSearching_improvedBookmarks() {
        initializeNormal();
        mBookmarkToolbar.setBookmarkUiMode(BookmarkUiMode.SEARCHING);
        assertFalse(mBookmarkToolbar.isSearching());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSortButtonsDisabled_throughSelection() {
        initializeNormal();
        mBookmarkToolbar.setSortMenuIdsEnabled(false);
        verifyMenuEnabled(
                BookmarkToolbarMediator.SORT_MENU_IDS, BookmarkToolbarMediator.SORT_MENU_IDS);

        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);
        mBookmarkToolbar.onSelectionStateChange(Collections.singletonList(BOOKMARK_ID_ONE));

        verifySelectionMenuVisibility(
                R.id.selection_mode_edit_menu_id,
                R.id.selection_mode_move_menu_id,
                R.id.selection_open_in_new_tab_id,
                R.id.selection_open_in_incognito_tab_id,
                R.id.reading_list_mark_as_read_id,
                R.id.reading_list_mark_as_unread_id);

        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(false);
        mBookmarkToolbar.onSelectionStateChange(Collections.emptyList());

        // The filter button visibility should be carried over through a selection event.
        verifyMenuEnabled(
                BookmarkToolbarMediator.SORT_MENU_IDS, BookmarkToolbarMediator.SORT_MENU_IDS);
    }
}
