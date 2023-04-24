// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.bookmarks.BookmarkAddEditFolderActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderSelectActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/** Unit tests for {@link BookmarkToolbarMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class BookmarkToolbarMediatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);
    @Mock
    BookmarkDelegate mBookmarkDelegate;
    @Mock
    DragReorderableRecyclerViewAdapter mDragReorderableRecyclerViewAdapter;
    @Mock
    BookmarkModel mBookmarkModel;
    @Mock
    BookmarkOpener mBookmarkOpener;
    @Mock
    SelectionDelegate mSelectionDelegate;
    @Mock
    Runnable mOpenSearchUiRunnable;
    @Mock
    Callback mOpenFolderCallback;
    @Mock
    BookmarkId mBookmarkId;
    @Mock
    BookmarkItem mBookmarkItem;
    @Mock
    BookmarkUiPrefs mBookmarkUiPrefs;
    @Spy
    Context mContext;

    BookmarkToolbarMediator mMediator;
    PropertyModel mModel;
    OneshotSupplierImpl<BookmarkDelegate> mBookmarkDelegateSupplier = new OneshotSupplierImpl<>();

    @Before
    public void setUp() {
        // Setup the context, we need a spy here because the context is used to launch activities.
        mContext = Mockito.spy(new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight));
        doNothing().when(mContext).startActivity(any());

        // Setup the bookmark model ids/items.
        doReturn(mBookmarkItem).when(mBookmarkModel).getBookmarkById(any());
        doReturn(mBookmarkId).when(mBookmarkItem).getId();

        mModel = new PropertyModel.Builder(BookmarkToolbarProperties.ALL_KEYS)
                         .with(BookmarkToolbarProperties.BOOKMARK_MODEL, mBookmarkModel)
                         .with(BookmarkToolbarProperties.BOOKMARK_OPENER, mBookmarkOpener)
                         .with(BookmarkToolbarProperties.SELECTION_DELEGATE, mSelectionDelegate)
                         .with(BookmarkToolbarProperties.BOOKMARK_UI_MODE, BookmarkUiMode.LOADING)
                         .with(BookmarkToolbarProperties.IS_DIALOG_UI, false)
                         .with(BookmarkToolbarProperties.DRAG_ENABLED, false)
                         .with(BookmarkToolbarProperties.OPEN_SEARCH_UI_RUNNABLE,
                                 mOpenSearchUiRunnable)
                         .with(BookmarkToolbarProperties.OPEN_FOLDER_CALLBACK, mOpenFolderCallback)
                         .build();

        mMediator = new BookmarkToolbarMediator(mContext, mModel,
                mDragReorderableRecyclerViewAdapter, mBookmarkDelegateSupplier, mSelectionDelegate,
                mBookmarkModel, mBookmarkOpener, mBookmarkUiPrefs);
        mBookmarkDelegateSupplier.set(mBookmarkDelegate);
    }

    private boolean navigationButtonMatchesModel(@NavigationButton int navigationButton) {
        return navigationButton
                == (int) mModel.get(BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE);
    }

    private void setCurrentSelection(BookmarkId... bookmarkIdArray) {
        List<BookmarkId> bookmarkIdList = Arrays.asList(bookmarkIdArray);
        doReturn(bookmarkIdList).when(mSelectionDelegate).getSelectedItemsAsList();
        doReturn(new HashSet<>(bookmarkIdList)).when(mSelectionDelegate).getSelectedItems();
    }

    private void verifyActivityLaunched(Class clazz) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).startActivity(intentCaptor.capture());
        Assert.assertEquals(clazz.getName(), intentCaptor.getValue().getComponent().getClassName());

        mMediator = new BookmarkToolbarMediator(mContext, mModel,
                mDragReorderableRecyclerViewAdapter, mBookmarkDelegateSupplier, mSelectionDelegate,
                mBookmarkModel, mBookmarkOpener, mBookmarkUiPrefs);
    }

    @Test
    public void bookmarkDelegateAvailableSetsUpObserver() {
        Mockito.verify(mBookmarkDelegate).addUiObserver(mMediator);
        Mockito.verify(mSelectionDelegate).addObserver(mMediator);
    }

    @Test
    public void onStateChangedUpdatesModel() {
        mMediator.onUiModeChanged(BookmarkUiMode.LOADING);
        Assert.assertEquals(BookmarkUiMode.LOADING,
                mModel.get(BookmarkToolbarProperties.BOOKMARK_UI_MODE).intValue());

        mMediator.onUiModeChanged(BookmarkUiMode.SEARCHING);
        Assert.assertEquals(BookmarkUiMode.SEARCHING,
                mModel.get(BookmarkToolbarProperties.BOOKMARK_UI_MODE).intValue());

        mMediator.onUiModeChanged(BookmarkUiMode.FOLDER);
        Assert.assertEquals(BookmarkUiMode.FOLDER,
                mModel.get(BookmarkToolbarProperties.BOOKMARK_UI_MODE).intValue());
    }

    @Test
    public void destroyUnregistersObserver() {
        Mockito.verify(mBookmarkDelegate).addUiObserver(mMediator);
        Mockito.verify(mSelectionDelegate).addObserver(mMediator);

        mMediator.onDestroy();
        Mockito.verify(mBookmarkDelegate).removeUiObserver(mMediator);
        Mockito.verify(mSelectionDelegate).removeObserver(mMediator);
    }

    @Test
    public void selectionStateChangeHidesKeyboard() {
        mMediator.onUiModeChanged(BookmarkUiMode.SEARCHING);
        Assert.assertEquals(true, mModel.get(BookmarkToolbarProperties.SOFT_KEYBOARD_VISIBLE));

        mMediator.onSelectionStateChange(null);
        Assert.assertEquals(false, mModel.get(BookmarkToolbarProperties.SOFT_KEYBOARD_VISIBLE));
    }

    @Test
    public void onFolderStateSet_CurrentFolderIsRoot() {
        doReturn(mBookmarkId).when(mBookmarkModel).getRootFolderId();
        doReturn(false).when(mBookmarkItem).isEditable();
        mMediator.onFolderStateSet(mBookmarkId);

        Assert.assertTrue(mModel.get(BookmarkToolbarProperties.SEARCH_BUTTON_VISIBLE));
        Assert.assertFalse(mModel.get(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE));
        Assert.assertEquals("Bookmarks", mModel.get(BookmarkToolbarProperties.TITLE));
        Assert.assertTrue(navigationButtonMatchesModel(NavigationButton.NONE));
    }

    @Test
    public void onFolderStateSet_CurrentFolderIsShopping() {
        doReturn(mBookmarkItem).when(mBookmarkModel).getBookmarkById(BookmarkId.SHOPPING_FOLDER);
        doReturn(false).when(mBookmarkItem).isEditable();
        mMediator.onFolderStateSet(BookmarkId.SHOPPING_FOLDER);

        Assert.assertTrue(mModel.get(BookmarkToolbarProperties.SEARCH_BUTTON_VISIBLE));
        Assert.assertFalse(mModel.get(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE));
        Assert.assertEquals("Tracked products", mModel.get(BookmarkToolbarProperties.TITLE));
        Assert.assertTrue(navigationButtonMatchesModel(NavigationButton.BACK));
    }

    @Test
    public void onFolderStateSet_EmptyTitleWhenChildOfRoot() {
        ArrayList<BookmarkId> topLevelFolders = new ArrayList<>();
        topLevelFolders.add(mBookmarkId);
        doReturn(topLevelFolders).when(mBookmarkModel).getTopLevelFolderParentIds();
        doReturn(mBookmarkId).when(mBookmarkItem).getParentId();
        doReturn(true).when(mBookmarkItem).isEditable();
        doReturn("").when(mBookmarkItem).getTitle();
        mMediator.onFolderStateSet(mBookmarkId);

        Assert.assertTrue(mModel.get(BookmarkToolbarProperties.SEARCH_BUTTON_VISIBLE));
        Assert.assertTrue(mModel.get(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE));
        Assert.assertEquals("Bookmarks", mModel.get(BookmarkToolbarProperties.TITLE));
        Assert.assertTrue(navigationButtonMatchesModel(NavigationButton.BACK));
    }

    @Test
    public void onFolderStateSet_RegularFolder() {
        doReturn(true).when(mBookmarkItem).isEditable();
        doReturn("test folder").when(mBookmarkItem).getTitle();
        mMediator.onFolderStateSet(mBookmarkId);

        Assert.assertTrue(mModel.get(BookmarkToolbarProperties.SEARCH_BUTTON_VISIBLE));
        Assert.assertTrue(mModel.get(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE));
        Assert.assertEquals("test folder", mModel.get(BookmarkToolbarProperties.TITLE));
        Assert.assertTrue(navigationButtonMatchesModel(NavigationButton.BACK));
    }

    @Test
    public void testOnMenuItemClick_editMenu() {
        mMediator.onFolderStateSet(mBookmarkId);
        Assert.assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                                  .apply(R.id.edit_menu_id));
        verifyActivityLaunched(BookmarkAddEditFolderActivity.class);
    }

    @Test
    public void testOnMenuItemClick_closeMenu() {
        Assert.assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                                  .apply(R.id.close_menu_id));
        // Difficult to verify that the activity has been finished, especially in a unit test.
    }

    @Test
    public void testOnMenuItemClick_searchMenu() {
        Assert.assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                                  .apply(R.id.search_menu_id));
        Mockito.verify(mBookmarkDelegate).openSearchUi();
    }

    @Test
    public void testOnMenuItemClick_selectionModeEditMenu() {
        setCurrentSelection(mBookmarkId);
        doReturn(false).when(mBookmarkItem).isFolder();
        Assert.assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                                  .apply(R.id.selection_mode_edit_menu_id));
        verifyActivityLaunched(BookmarkEditActivity.class);
    }

    @Test
    public void testOnMenuItemClick_selectionModeEditMenuFolder() {
        setCurrentSelection(mBookmarkId);
        doReturn(true).when(mBookmarkItem).isFolder();
        Assert.assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                                  .apply(R.id.selection_mode_edit_menu_id));
        verifyActivityLaunched(BookmarkAddEditFolderActivity.class);
    }

    @Test
    public void testOnMenuItemClick_selectionModeMoveMenu() {
        setCurrentSelection(mBookmarkId);
        Assert.assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                                  .apply(R.id.selection_mode_move_menu_id));
        verifyActivityLaunched(BookmarkFolderSelectActivity.class);
    }

    @Test
    public void testOnMenuItemClick_selectionModeDeleteMenu() {
        setCurrentSelection(mBookmarkId);
        Assert.assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                                  .apply(R.id.selection_mode_delete_menu_id));
        verify(mBookmarkModel).deleteBookmarks(Mockito.any());
    }

    @Test
    public void testOnMenuItemClick_selectionOpenInNewTab() {
        setCurrentSelection(mBookmarkId);
        Assert.assertTrue(mMediator.onMenuIdClick(R.id.selection_open_in_new_tab_id));
        verify(mBookmarkOpener).openBookmarksInNewTabs(any(), eq(false));
    }

    @Test
    public void testOnMenuItemClick_selectionOpenInIncognitoTab() {
        setCurrentSelection(mBookmarkId);
        Assert.assertTrue(mMediator.onMenuIdClick(R.id.selection_open_in_incognito_tab_id));
        verify(mBookmarkOpener).openBookmarksInNewTabs(any(), eq(true));
    }

    @Test
    public void testOnMenuItemClick_sortOptions() {
        Assert.assertTrue(mMediator.onMenuIdClick(R.id.sort_by_newest));
        Assert.assertEquals(
                R.id.sort_by_newest, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));

        Assert.assertTrue(mMediator.onMenuIdClick(R.id.sort_by_oldest));
        Assert.assertEquals(
                R.id.sort_by_oldest, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));

        Assert.assertTrue(mMediator.onMenuIdClick(R.id.sort_by_alpha));
        Assert.assertEquals(
                R.id.sort_by_alpha, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));

        Assert.assertTrue(mMediator.onMenuIdClick(R.id.sort_by_reverse_alpha));
        Assert.assertEquals(R.id.sort_by_reverse_alpha,
                mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
    }

    @Test
    public void testOnMenuItemClick_viewOptions() {
        Assert.assertTrue(mMediator.onMenuIdClick(R.id.visual_view));
        Assert.assertEquals(
                R.id.visual_view, mModel.get(BookmarkToolbarProperties.CHECKED_VIEW_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowDisplayPref(BookmarkRowDisplayPref.VISUAL);

        Assert.assertTrue(mMediator.onMenuIdClick(R.id.compact_view));
        Assert.assertEquals(
                R.id.compact_view, mModel.get(BookmarkToolbarProperties.CHECKED_VIEW_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowDisplayPref(BookmarkRowDisplayPref.COMPACT);
    }
}
