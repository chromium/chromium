// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.IdRes;
import androidx.appcompat.widget.ActionMenuView;
import androidx.core.view.ViewCompat;

import com.google.common.primitives.Ints;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.SearchDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.base.TestActivity;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Unit test for {@link BookmarkToolbar}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkToolbarTest {
    private static final List<Integer> SELECTION_MENU_IDS =
            Arrays.asList(
                    R.id.selection_mode_edit_menu_id,
                    R.id.selection_mode_move_menu_id,
                    R.id.selection_mode_delete_menu_id,
                    R.id.selection_open_in_new_tab_id,
                    R.id.selection_open_in_incognito_tab_id,
                    R.id.reading_list_mark_as_read_id,
                    R.id.reading_list_mark_as_unread_id,
                    R.id.selection_mode_copy_link);
    private static final BookmarkId BOOKMARK_ID_ONE = new BookmarkId(2, BookmarkType.NORMAL);
    private static final BookmarkId BOOKMARK_ID_TWO = new BookmarkId(3, BookmarkType.NORMAL);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SelectionDelegate<BookmarkId> mSelectionDelegate;
    @Mock private SearchDelegate mSearchDelegate;
    @Mock private Runnable mNavigateBackRunnable;
    private ViewGroup mContentView;
    private BookmarkToolbar mBookmarkToolbar;

    @Before
    public void setUp() throws Exception {

        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mContentView = new LinearLayout(activity);
        mContentView.setBackgroundColor(Color.WHITE);
        FrameLayout.LayoutParams params =
                new FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        activity.setContentView(mContentView, params);

        mBookmarkToolbar =
                activity.getLayoutInflater()
                        .inflate(R.layout.bookmark_toolbar, mContentView, true)
                        .findViewById(R.id.bookmark_toolbar);
        mBookmarkToolbar.setSortMenuIds(BookmarkToolbarMediator.SORT_MENU_IDS);
    }

    private void initializeNormal() {
        mBookmarkToolbar.initialize(
                mSelectionDelegate,
                /* titleResId= */ 0,
                R.id.normal_menu_group,
                R.id.selection_mode_menu_group,
                /* updateStatusBarColor= */ false);
        mBookmarkToolbar.initializeSearchView(
                mSearchDelegate, R.string.bookmark_toolbar_search, R.id.search_menu_id);
        mBookmarkToolbar.setSortMenuIds(BookmarkToolbarMediator.SORT_MENU_IDS);
        mBookmarkToolbar.setSelectionDelegate(mSelectionDelegate);
        mBookmarkToolbar.setBookmarkUiMode(BookmarkUiMode.FOLDER);
        mBookmarkToolbar.setIsDialogUi(true);
        mBookmarkToolbar.setNavigateBackRunnable(mNavigateBackRunnable);
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

    private void forceLayout() {
        mBookmarkToolbar.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(200, View.MeasureSpec.EXACTLY));
        mBookmarkToolbar.layout(0, 0, 1080, 200);
    }

    private ActionMenuView getActionMenuView() {
        for (int i = 0; i < mBookmarkToolbar.getChildCount(); i++) {
            View child = mBookmarkToolbar.getChildAt(i);
            if (child instanceof ActionMenuView) return (ActionMenuView) child;
        }
        return null;
    }

    private View getOverflowMenuButton() {
        ActionMenuView menuView = getActionMenuView();
        if (menuView == null) return null;

        for (int j = 0; j < menuView.getChildCount(); j++) {
            View c = menuView.getChildAt(j);
            if (c instanceof ImageView) return c;
        }
        return null;
    }

    @Test
    public void onNavigationBack() {
        initializeNormal();
        mBookmarkToolbar.onNavigationBack();
        verify(mNavigateBackRunnable).run();
    }

    @Test
    public void testOnMenuItemClick_closeMenu() {
        initializeNormal();

        MenuItem menuItem = mBookmarkToolbar.getMenu().findItem(R.id.close_menu_id);
        assertNotNull(menuItem);
    }

    @Test
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
    public void testSelectionShowEdit() {
        verifySelectionModeMenuItem(
                mBookmarkToolbar::setSelectionShowEdit, R.id.selection_mode_edit_menu_id);
    }

    @Test
    public void testSelectionShowOpenInNewTab() {
        verifySelectionModeMenuItem(
                mBookmarkToolbar::setSelectionShowOpenInNewTab, R.id.selection_open_in_new_tab_id);
    }

    @Test
    public void testSelectionShowOpenInIncognitoTab() {
        verifySelectionModeMenuItem(
                mBookmarkToolbar::setSelectionShowOpenInIncognito,
                R.id.selection_open_in_incognito_tab_id);
    }

    @Test
    public void testSelectionShowMove() {
        verifySelectionModeMenuItem(
                mBookmarkToolbar::setSelectionShowMove, R.id.selection_mode_move_menu_id);
    }

    @Test
    public void testSelectionShowMarkRead() {
        verifySelectionModeMenuItem(
                mBookmarkToolbar::setSelectionShowMarkRead, R.id.reading_list_mark_as_read_id);
    }

    @Test
    public void testSelectionShowMarkUnread() {
        verifySelectionModeMenuItem(
                mBookmarkToolbar::setSelectionShowMarkUnread, R.id.reading_list_mark_as_unread_id);
    }

    @Test
    public void testSelectionShowCopyLink() {
        verifySelectionModeMenuItem(
                mBookmarkToolbar::setSelectionShowCopyLink, R.id.selection_mode_copy_link);
    }

    @Test
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
    public void testSearching_improvedBookmarks() {
        initializeNormal();
        mBookmarkToolbar.setBookmarkUiMode(BookmarkUiMode.SEARCHING);
        assertFalse(mBookmarkToolbar.isSearching());
    }

    @Test
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
                R.id.reading_list_mark_as_unread_id,
                R.id.selection_mode_copy_link);

        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(false);
        mBookmarkToolbar.onSelectionStateChange(Collections.emptyList());

        // The filter button visibility should be carried over through a selection event.
        verifyMenuEnabled(
                BookmarkToolbarMediator.SORT_MENU_IDS, BookmarkToolbarMediator.SORT_MENU_IDS);
    }

    @Test
    public void testAccessibilityPaneDescription_beforeSelection() {
        initializeNormal();
        assertNull(ViewCompat.getAccessibilityPaneTitle(mBookmarkToolbar));
    }

    @Test
    public void testAccessibilityPaneDescription_afterSelectOne() {
        initializeNormal();
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);
        mBookmarkToolbar.onSelectionStateChange(List.of(BOOKMARK_ID_ONE));
        assertEquals(
                mBookmarkToolbar
                        .getContext()
                        .getString(R.string.accessibility_toolbar_screen_position, 1),
                ViewCompat.getAccessibilityPaneTitle(mBookmarkToolbar));
    }

    @Test
    public void testAccessibilityPaneDescription_afterSelectTwo() {
        initializeNormal();
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);
        mBookmarkToolbar.onSelectionStateChange(List.of(BOOKMARK_ID_ONE));
        mBookmarkToolbar.onSelectionStateChange(List.of(BOOKMARK_ID_ONE, BOOKMARK_ID_TWO));
        assertEquals(
                mBookmarkToolbar
                        .getContext()
                        .getString(R.string.accessibility_toolbar_multi_select, 2),
                ViewCompat.getAccessibilityPaneTitle(mBookmarkToolbar));
    }

    @Test
    public void testAccessibilityPaneDescription_afterSelectThenDeselect() {
        initializeNormal();
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);
        mBookmarkToolbar.onSelectionStateChange(List.of(BOOKMARK_ID_ONE, BOOKMARK_ID_TWO));
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(false);
        mBookmarkToolbar.onSelectionStateChange(List.of());
        assertEquals(
                mBookmarkToolbar.getContext().getString(R.string.accessibility_toolbar_exit_select),
                ViewCompat.getAccessibilityPaneTitle(mBookmarkToolbar));
    }

    @Test
    public void testSelection_preservesSelectionBack_noneState() {
        initializeNormal();
        mBookmarkToolbar.setNavigationButtonState(NavigationButton.NONE);
        mBookmarkToolbar.setChromeIconVisible(true);
        assertNotNull(mBookmarkToolbar.getNavigationIcon());

        // Enter selection mode.
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);
        mBookmarkToolbar.onSelectionStateChange(List.of(BOOKMARK_ID_ONE));
        mBookmarkToolbar.setChromeIconVisible(false);

        assertEquals(
                NavigationButton.SELECTION_BACK, mBookmarkToolbar.getNavigationButtonForTests());
        assertNotNull(mBookmarkToolbar.getNavigationIcon());

        // Update navigation button state while in selection mode (e.g. during drag/folder update).
        mBookmarkToolbar.setNavigationButtonState(NavigationButton.NONE);
        assertEquals(
                NavigationButton.SELECTION_BACK, mBookmarkToolbar.getNavigationButtonForTests());
        assertNotNull(mBookmarkToolbar.getNavigationIcon());

        // Exit selection mode.
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(false);
        mBookmarkToolbar.onSelectionStateChange(List.of());

        assertEquals(NavigationButton.NONE, mBookmarkToolbar.getNavigationButtonForTests());
    }

    @Test
    public void testSelection_preservesSelectionBack_normalViewBackState() {
        initializeNormal();
        mBookmarkToolbar.setNavigationButtonState(NavigationButton.NORMAL_VIEW_BACK);
        assertEquals(
                NavigationButton.NORMAL_VIEW_BACK, mBookmarkToolbar.getNavigationButtonForTests());
        assertNotNull(mBookmarkToolbar.getNavigationIcon());

        // Enter selection mode.
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);
        mBookmarkToolbar.onSelectionStateChange(List.of(BOOKMARK_ID_ONE));
        mBookmarkToolbar.setChromeIconVisible(false);

        assertEquals(
                NavigationButton.SELECTION_BACK, mBookmarkToolbar.getNavigationButtonForTests());
        assertNotNull(mBookmarkToolbar.getNavigationIcon());

        // Update navigation button state while in selection mode (e.g. during drag/folder update).
        mBookmarkToolbar.setNavigationButtonState(NavigationButton.NORMAL_VIEW_BACK);
        assertEquals(
                NavigationButton.SELECTION_BACK, mBookmarkToolbar.getNavigationButtonForTests());
        assertNotNull(mBookmarkToolbar.getNavigationIcon());

        // Exit selection mode.
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(false);
        mBookmarkToolbar.onSelectionStateChange(List.of());

        assertEquals(
                NavigationButton.NORMAL_VIEW_BACK, mBookmarkToolbar.getNavigationButtonForTests());
        assertNotNull(mBookmarkToolbar.getNavigationIcon());
    }

    @Test
    public void testSetCheckedSortMenuId_mutuallyExclusive() {
        initializeNormal();
        for (@IdRes int targetId : BookmarkToolbar.SORT_MENU_IDS) {
            mBookmarkToolbar.setCheckedSortMenuId(targetId);
            for (@IdRes int sortId : BookmarkToolbar.SORT_MENU_IDS) {
                MenuItem item = mBookmarkToolbar.getMenu().findItem(sortId);
                assertNotNull(item);
                assertEquals(
                        "Sort item " + sortId + " checked state mismatch for target " + targetId,
                        sortId == targetId,
                        item.isChecked());
            }
        }

        // Passing an unhandled ID should uncheck all items.
        mBookmarkToolbar.setCheckedSortMenuId(View.NO_ID);
        for (@IdRes int sortId : BookmarkToolbar.SORT_MENU_IDS) {
            assertFalse(mBookmarkToolbar.getMenu().findItem(sortId).isChecked());
        }
    }

    @Test
    public void testSetCheckedSortMenuId_nullSortMenuIdsFallback() {
        initializeNormal();
        // Clear mSortMenuIds to test fallback to SORT_MENU_IDS.
        mBookmarkToolbar.setSortMenuIds(null);
        mBookmarkToolbar.setCheckedSortMenuId(R.id.sort_by_newest);
        for (@IdRes int sortId : BookmarkToolbar.SORT_MENU_IDS) {
            assertEquals(
                    sortId == R.id.sort_by_newest,
                    mBookmarkToolbar.getMenu().findItem(sortId).isChecked());
        }
    }

    @Test
    public void testSetCheckedViewMenuId_mutuallyExclusive() {
        initializeNormal();
        for (@IdRes int targetId : BookmarkToolbar.VIEW_MENU_IDS) {
            mBookmarkToolbar.setCheckedViewMenuId(targetId);
            for (@IdRes int viewId : BookmarkToolbar.VIEW_MENU_IDS) {
                MenuItem item = mBookmarkToolbar.getMenu().findItem(viewId);
                assertNotNull(item);
                assertEquals(
                        "View item " + viewId + " checked state mismatch for target " + targetId,
                        viewId == targetId,
                        item.isChecked());
            }
        }

        // Passing an unhandled ID should uncheck all items.
        mBookmarkToolbar.setCheckedViewMenuId(View.NO_ID);
        for (@IdRes int viewId : BookmarkToolbar.VIEW_MENU_IDS) {
            assertFalse(mBookmarkToolbar.getMenu().findItem(viewId).isChecked());
        }
    }

    /**
     * Verifies that the 3-dot overflow menu button is correctly re-enabled when re-entering
     * Selection Mode after a deselection race condition (crbug.com/552272450).
     */
    @Test
    public void testSelectionMode_OverflowMenuState_RestoredOnReentry() {
        initializeNormal();

        // 1. Enter Selection Mode.
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);
        mBookmarkToolbar.onSelectionStateChange(List.of(BOOKMARK_ID_ONE));
        // Ensure we show menu items that are configured as showAsAction="never"
        // so that the 3-dot overflow menu button is actually generated by AppCompat.
        mBookmarkToolbar.setSelectionShowOpenInNewTab(true);
        mBookmarkToolbar.setSelectionShowOpenInIncognito(true);
        forceLayout();

        // Verify selection view is active and we have an overflow menu button that is enabled by
        // default.
        View overflowButton = getOverflowMenuButton();
        assertNotNull(overflowButton);
        assertTrue(overflowButton.isEnabled());

        // 2. Drag starts -> Disable the overflow button.
        mBookmarkToolbar.setDragEnabled(true);
        assertFalse(overflowButton.isEnabled());

        // 3. Exit Selection Mode BEFORE drag ends (simulating Step 2's deselection race).
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(false);
        mBookmarkToolbar.onSelectionStateChange(List.of());
        // Manually remove the overflow button from ActionMenuView to simulate the layout pruning
        // of un-needed views when transitioning back to NORMAL_VIEW.
        ActionMenuView menuView = getActionMenuView();
        if (menuView != null && overflowButton != null) {
            menuView.removeView(overflowButton);
        }
        forceLayout();

        // 4. Drag ends -> This attempts to re-enable the overflow menu.
        // But since the overflow button is physically removed from the ActionMenuView,
        // this will fail to find the overflow button and do nothing (leaving it disabled).
        mBookmarkToolbar.setDragEnabled(false);

        // 5. Re-enter Selection Mode (without starting a drag).
        when(mSelectionDelegate.isSelectionEnabled()).thenReturn(true);
        mBookmarkToolbar.onSelectionStateChange(List.of(BOOKMARK_ID_ONE));
        mBookmarkToolbar.setSelectionShowOpenInNewTab(true);
        mBookmarkToolbar.setSelectionShowOpenInIncognito(true);
        forceLayout();

        // The overflow button is visible again in Selection Mode.
        overflowButton = getOverflowMenuButton();
        assertNotNull(overflowButton);

        // Verify that the overflow button's enabled state is successfully restored on reentry.
        // This prevents a regression where cached AppCompat views retained the disabled state.
        assertTrue(overflowButton.isEnabled());
    }
}
