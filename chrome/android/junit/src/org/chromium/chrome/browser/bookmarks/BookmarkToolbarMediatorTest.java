// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.bookmarks.BookmarkAddEditFolderActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderPickerActivity;
import org.chromium.chrome.browser.app.bookmarks.BookmarkFolderSelectActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;

/** Unit tests for {@link BookmarkToolbarMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
@EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
public class BookmarkToolbarMediatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock
    private BookmarkDelegate mBookmarkDelegate;
    @Mock
    private DragReorderableRecyclerViewAdapter mDragReorderableRecyclerViewAdapter;
    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private BookmarkOpener mBookmarkOpener;
    @Mock
    private SelectionDelegate mSelectionDelegate;
    @Mock
    private Runnable mNavigateBackRunnable;
    @Mock
    private BookmarkId mBookmarkId;
    @Mock
    private BookmarkItem mBookmarkItem;
    @Mock
    private BookmarkUiPrefs mBookmarkUiPrefs;
    @Mock
    private BookmarkAddNewFolderCoordinator mBookmarkAddNewFolderCoordinator;
    @Mock
    private PropertyObserver<PropertyKey> mPropertyObserver;
    @Mock
    private Runnable mEndSearchRunnable;

    @Spy
    private Context mContext;

    BookmarkToolbarMediator mMediator;
    PropertyModel mModel;
    OneshotSupplierImpl<BookmarkDelegate> mBookmarkDelegateSupplier;

    @Before
    public void setUp() {
        // Setup the context, we need a spy here because the context is used to launch activities.
        mContext = spy(new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight));
        doNothing().when(mContext).startActivity(any());

        // Setup the bookmark model ids/items.
        doReturn(mBookmarkItem).when(mBookmarkModel).getBookmarkById(any());
        doReturn(mBookmarkId).when(mBookmarkItem).getId();

        initModelAndMediator();
    }

    private void initModelAndMediator() {
        mModel = new PropertyModel.Builder(BookmarkToolbarProperties.ALL_KEYS)
                         .with(BookmarkToolbarProperties.BOOKMARK_MODEL, mBookmarkModel)
                         .with(BookmarkToolbarProperties.BOOKMARK_OPENER, mBookmarkOpener)
                         .with(BookmarkToolbarProperties.SELECTION_DELEGATE, mSelectionDelegate)
                         .with(BookmarkToolbarProperties.BOOKMARK_UI_MODE, BookmarkUiMode.LOADING)
                         .with(BookmarkToolbarProperties.IS_DIALOG_UI, false)
                         .with(BookmarkToolbarProperties.DRAG_ENABLED, false)
                         .with(BookmarkToolbarProperties.NAVIGATE_BACK_RUNNABLE,
                                 mNavigateBackRunnable)
                         .build();
        mBookmarkDelegateSupplier = new OneshotSupplierImpl<>();
        mMediator = new BookmarkToolbarMediator(mContext, mModel,
                mDragReorderableRecyclerViewAdapter, mBookmarkDelegateSupplier, mSelectionDelegate,
                mBookmarkModel, mBookmarkOpener, mBookmarkUiPrefs, mBookmarkAddNewFolderCoordinator,
                mEndSearchRunnable);
        mBookmarkDelegateSupplier.set(mBookmarkDelegate);
    }

    private boolean navigationButtonMatchesModel(@NavigationButton int navigationButton) {
        return navigationButton
                == (int) mModel.get(BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE);
    }

    private void dropCurrentSelection() {
        doReturn(Collections.emptyList()).when(mSelectionDelegate).getSelectedItemsAsList();
        doReturn(Collections.emptyList()).when(mSelectionDelegate).getSelectedItems();
        doReturn(false).when(mSelectionDelegate).isSelectionEnabled();
    }

    private void setCurrentSelection(BookmarkId... bookmarkIdArray) {
        List<BookmarkId> bookmarkIdList = Arrays.asList(bookmarkIdArray);
        doReturn(bookmarkIdList).when(mSelectionDelegate).getSelectedItemsAsList();
        doReturn(new HashSet<>(bookmarkIdList)).when(mSelectionDelegate).getSelectedItems();
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();
    }

    private void verifyActivityLaunched(Class clazz) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).startActivity(intentCaptor.capture());
        assertEquals(clazz.getName(), intentCaptor.getValue().getComponent().getClassName());

        mMediator = new BookmarkToolbarMediator(mContext, mModel,
                mDragReorderableRecyclerViewAdapter, mBookmarkDelegateSupplier, mSelectionDelegate,
                mBookmarkModel, mBookmarkOpener, mBookmarkUiPrefs, mBookmarkAddNewFolderCoordinator,
                mEndSearchRunnable);
    }

    @Test
    public void bookmarkDelegateAvailableSetsUpObserver() {
        verify(mBookmarkDelegate).addUiObserver(mMediator);
        verify(mSelectionDelegate).addObserver(mMediator);
    }

    @Test
    public void onStateChangedUpdatesModel() {
        mMediator.onUiModeChanged(BookmarkUiMode.LOADING);
        assertEquals(BookmarkUiMode.LOADING,
                mModel.get(BookmarkToolbarProperties.BOOKMARK_UI_MODE).intValue());

        mMediator.onUiModeChanged(BookmarkUiMode.SEARCHING);
        assertEquals(BookmarkUiMode.SEARCHING,
                mModel.get(BookmarkToolbarProperties.BOOKMARK_UI_MODE).intValue());

        mMediator.onUiModeChanged(BookmarkUiMode.FOLDER);
        assertEquals(BookmarkUiMode.FOLDER,
                mModel.get(BookmarkToolbarProperties.BOOKMARK_UI_MODE).intValue());
    }

    @Test
    public void destroyUnregistersObserver() {
        verify(mBookmarkDelegate).addUiObserver(mMediator);
        verify(mSelectionDelegate).addObserver(mMediator);

        mMediator.onDestroy();
        verify(mBookmarkDelegate).removeUiObserver(mMediator);
        verify(mSelectionDelegate).removeObserver(mMediator);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS})
    public void selectionStateChangeHidesKeyboard() {
        mMediator.onUiModeChanged(BookmarkUiMode.SEARCHING);
        assertEquals(true, mModel.get(BookmarkToolbarProperties.SOFT_KEYBOARD_VISIBLE));

        mMediator.onSelectionStateChange(null);
        assertEquals(false, mModel.get(BookmarkToolbarProperties.SOFT_KEYBOARD_VISIBLE));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS})
    public void selectionStateChangeHidesKeyboard_improvedBookmarks() {
        mModel.addObserver(mPropertyObserver);

        mMediator.onUiModeChanged(BookmarkUiMode.SEARCHING);
        verify(mPropertyObserver, never())
                .onPropertyChanged(any(), eq(BookmarkToolbarProperties.SOFT_KEYBOARD_VISIBLE));

        mMediator.onUiModeChanged(BookmarkUiMode.FOLDER);
        verify(mPropertyObserver, never())
                .onPropertyChanged(any(), eq(BookmarkToolbarProperties.SOFT_KEYBOARD_VISIBLE));

        mModel.removeObserver(mPropertyObserver);
    }

    @Test
    public void onFolderStateSet_CurrentFolderIsRoot() {
        doReturn(mBookmarkId).when(mBookmarkModel).getRootFolderId();
        doReturn(false).when(mBookmarkItem).isEditable();
        mMediator.onFolderStateSet(mBookmarkId);

        assertTrue(mModel.get(BookmarkToolbarProperties.SEARCH_BUTTON_VISIBLE));
        assertFalse(mModel.get(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE));
        assertEquals("Bookmarks", mModel.get(BookmarkToolbarProperties.TITLE));
        assertTrue(navigationButtonMatchesModel(NavigationButton.NONE));
    }

    @Test
    public void onFolderStateSet_CurrentFolderIsShopping() {
        doReturn(mBookmarkItem).when(mBookmarkModel).getBookmarkById(BookmarkId.SHOPPING_FOLDER);
        doReturn(false).when(mBookmarkItem).isEditable();
        mMediator.onFolderStateSet(BookmarkId.SHOPPING_FOLDER);

        assertTrue(mModel.get(BookmarkToolbarProperties.SEARCH_BUTTON_VISIBLE));
        assertFalse(mModel.get(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE));
        assertEquals("Tracked products", mModel.get(BookmarkToolbarProperties.TITLE));
        assertTrue(navigationButtonMatchesModel(NavigationButton.BACK));
    }

    @Test
    public void onFolderStateSet_EmptyTitleWhenChildOfRoot() {
        ArrayList<BookmarkId> topLevelFolders = new ArrayList<>();
        topLevelFolders.add(mBookmarkId);
        doReturn(topLevelFolders).when(mBookmarkModel).getTopLevelFolderIds();
        doReturn(mBookmarkId).when(mBookmarkItem).getParentId();
        doReturn(true).when(mBookmarkItem).isEditable();
        doReturn("").when(mBookmarkItem).getTitle();
        mMediator.onFolderStateSet(mBookmarkId);

        assertTrue(mModel.get(BookmarkToolbarProperties.SEARCH_BUTTON_VISIBLE));
        assertTrue(mModel.get(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE));
        assertEquals("Bookmarks", mModel.get(BookmarkToolbarProperties.TITLE));
        assertTrue(navigationButtonMatchesModel(NavigationButton.BACK));
    }

    @Test
    public void onFolderStateSet_RegularFolder() {
        doReturn(true).when(mBookmarkItem).isEditable();
        doReturn("test folder").when(mBookmarkItem).getTitle();
        mMediator.onFolderStateSet(mBookmarkId);

        assertTrue(mModel.get(BookmarkToolbarProperties.SEARCH_BUTTON_VISIBLE));
        assertTrue(mModel.get(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE));
        assertEquals("test folder", mModel.get(BookmarkToolbarProperties.TITLE));
        assertTrue(navigationButtonMatchesModel(NavigationButton.BACK));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS})
    public void testOnMenuItemClick_editMenu() {
        mMediator.onFolderStateSet(mBookmarkId);
        assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                           .apply(R.id.edit_menu_id));
        verifyActivityLaunched(BookmarkAddEditFolderActivity.class);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS})
    public void testOnMenuItemClick_editMenu_improvedBookmarks() {
        mMediator.onFolderStateSet(mBookmarkId);
        assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                           .apply(R.id.edit_menu_id));
        verifyActivityLaunched(BookmarkEditActivity.class);
    }

    @Test
    public void testOnMenuItemClick_closeMenu() {
        assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                           .apply(R.id.close_menu_id));
        // Difficult to verify that the activity has been finished, especially in a unit test.
    }

    @Test
    public void testOnMenuItemClick_searchMenu() {
        assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                           .apply(R.id.search_menu_id));
        verify(mBookmarkDelegate).openSearchUi();
    }

    @Test
    public void testOnMenuItemClick_selectionModeEditMenu() {
        setCurrentSelection(mBookmarkId);
        doReturn(false).when(mBookmarkItem).isFolder();
        assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                           .apply(R.id.selection_mode_edit_menu_id));
        verifyActivityLaunched(BookmarkEditActivity.class);
    }

    @Test
    public void testOnMenuItemClick_selectionModeEditMenuFolder() {
        setCurrentSelection(mBookmarkId);
        doReturn(true).when(mBookmarkItem).isFolder();
        assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                           .apply(R.id.selection_mode_edit_menu_id));
        verifyActivityLaunched(BookmarkEditActivity.class);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testOnMenuItemClick_selectionModeMoveMenu() {
        setCurrentSelection(mBookmarkId);
        assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                           .apply(R.id.selection_mode_move_menu_id));
        verifyActivityLaunched(BookmarkFolderSelectActivity.class);
    }

    @Test
    public void testOnMenuItemClick_selectionModeMoveMenu_improvedBookmarksEnabled() {
        setCurrentSelection(mBookmarkId);
        assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                           .apply(R.id.selection_mode_move_menu_id));
        verifyActivityLaunched(BookmarkFolderPickerActivity.class);
    }

    @Test
    public void testOnMenuItemClick_selectionModeDeleteMenu() {
        setCurrentSelection(mBookmarkId);
        assertTrue(mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                           .apply(R.id.selection_mode_delete_menu_id));
        verify(mBookmarkModel).deleteBookmarks(any());
    }

    @Test
    public void testOnMenuItemClick_selectionOpenInNewTab() {
        setCurrentSelection(mBookmarkId);
        assertTrue(mMediator.onMenuIdClick(R.id.selection_open_in_new_tab_id));
        verify(mBookmarkOpener).openBookmarksInNewTabs(any(), eq(false));
    }

    @Test
    public void testOnMenuItemClick_selectionOpenInIncognitoTab() {
        setCurrentSelection(mBookmarkId);
        assertTrue(mMediator.onMenuIdClick(R.id.selection_open_in_incognito_tab_id));
        verify(mBookmarkOpener).openBookmarksInNewTabs(any(), eq(true));
    }

    @Test
    public void testOnMenuItemClick_addNewFolder() {
        assertTrue(mMediator.onMenuIdClick(R.id.create_new_folder_menu_id));
        verify(mBookmarkAddNewFolderCoordinator).show(any());
    }

    @Test
    public void testAddNewFolder() {
        mMediator.onFolderStateSet(mBookmarkId);
        assertTrue(mModel.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_VISIBLE));
        assertTrue(mModel.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_ENABLED));
        assertTrue(mMediator.onMenuIdClick(R.id.create_new_folder_menu_id));
        verify(mBookmarkAddNewFolderCoordinator).show(any());

        doReturn(mBookmarkId).when(mBookmarkModel).getReadingListFolder();
        mMediator.onFolderStateSet(mBookmarkId);
        assertTrue(mModel.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_VISIBLE));
        assertFalse(mModel.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_ENABLED));

        doReturn(null).when(mBookmarkModel).getReadingListFolder();
        doReturn(mBookmarkId).when(mBookmarkModel).getPartnerFolderId();
        mMediator.onFolderStateSet(mBookmarkId);
        assertTrue(mModel.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_VISIBLE));
        assertFalse(mModel.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_ENABLED));
    }

    @Test
    public void testOnMenuItemClick_sortOptions() {
        assertTrue(mMediator.onMenuIdClick(R.id.sort_by_manual));
        assertEquals(
                R.id.sort_by_manual, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowSortOrder(BookmarkRowSortOrder.MANUAL);

        assertTrue(mMediator.onMenuIdClick(R.id.sort_by_newest));
        assertEquals(
                R.id.sort_by_newest, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs)
                .setBookmarkRowSortOrder(BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL);

        assertTrue(mMediator.onMenuIdClick(R.id.sort_by_oldest));
        assertEquals(
                R.id.sort_by_oldest, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowSortOrder(BookmarkRowSortOrder.CHRONOLOGICAL);

        assertTrue(mMediator.onMenuIdClick(R.id.sort_by_last_opened));
        assertEquals(R.id.sort_by_last_opened,
                mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowSortOrder(BookmarkRowSortOrder.RECENTLY_USED);

        assertTrue(mMediator.onMenuIdClick(R.id.sort_by_alpha));
        assertEquals(
                R.id.sort_by_alpha, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowSortOrder(BookmarkRowSortOrder.ALPHABETICAL);

        assertTrue(mMediator.onMenuIdClick(R.id.sort_by_reverse_alpha));
        assertEquals(R.id.sort_by_reverse_alpha,
                mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowSortOrder(BookmarkRowSortOrder.REVERSE_ALPHABETICAL);
    }

    @Test
    public void testOnMenuItemClick_viewOptions() {
        assertTrue(mMediator.onMenuIdClick(R.id.visual_view));
        assertEquals(R.id.visual_view, mModel.get(BookmarkToolbarProperties.CHECKED_VIEW_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowDisplayPref(BookmarkRowDisplayPref.VISUAL);

        assertTrue(mMediator.onMenuIdClick(R.id.compact_view));
        assertEquals(R.id.compact_view, mModel.get(BookmarkToolbarProperties.CHECKED_VIEW_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowDisplayPref(BookmarkRowDisplayPref.COMPACT);
    }

    @Test
    public void testInitialization_viewOptions() {
        doReturn(BookmarkRowDisplayPref.COMPACT).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        initModelAndMediator();
        assertEquals(R.id.compact_view, mModel.get(BookmarkToolbarProperties.CHECKED_VIEW_MENU_ID));

        doReturn(BookmarkRowDisplayPref.VISUAL).when(mBookmarkUiPrefs).getBookmarkRowDisplayPref();
        initModelAndMediator();
        assertEquals(R.id.visual_view, mModel.get(BookmarkToolbarProperties.CHECKED_VIEW_MENU_ID));
    }

    @Test
    public void testOnMenuItemClick_sortOrder() {
        assertTrue(mMediator.onMenuIdClick(R.id.sort_by_newest));
        assertEquals(
                R.id.sort_by_newest, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs)
                .setBookmarkRowSortOrder(BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL);

        assertTrue(mMediator.onMenuIdClick(R.id.sort_by_oldest));
        assertEquals(
                R.id.sort_by_oldest, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowSortOrder(BookmarkRowSortOrder.CHRONOLOGICAL);

        assertTrue(mMediator.onMenuIdClick(R.id.sort_by_alpha));
        assertEquals(
                R.id.sort_by_alpha, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowSortOrder(BookmarkRowSortOrder.ALPHABETICAL);

        assertTrue(mMediator.onMenuIdClick(R.id.sort_by_reverse_alpha));
        assertEquals(R.id.sort_by_reverse_alpha,
                mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowSortOrder(BookmarkRowSortOrder.REVERSE_ALPHABETICAL);
    }

    @Test
    public void testInitialization_sortOrder() {
        doReturn(BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();
        initModelAndMediator();
        assertEquals(
                R.id.sort_by_newest, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));

        doReturn(BookmarkRowSortOrder.CHRONOLOGICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();
        initModelAndMediator();
        assertEquals(
                R.id.sort_by_oldest, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));

        doReturn(BookmarkRowSortOrder.ALPHABETICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();
        initModelAndMediator();
        assertEquals(
                R.id.sort_by_alpha, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));

        doReturn(BookmarkRowSortOrder.REVERSE_ALPHABETICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();
        initModelAndMediator();
        assertEquals(R.id.sort_by_reverse_alpha,
                mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
    }

    @Test
    public void testTitleAndNavWhenSearching() {
        String folderName = "test folder";
        doReturn(folderName).when(mBookmarkItem).getTitle();
        mMediator.onFolderStateSet(mBookmarkId);
        assertEquals(folderName, mModel.get(BookmarkToolbarProperties.TITLE));

        mMediator.onUiModeChanged(BookmarkUiMode.SEARCHING);
        assertEquals("Search", mModel.get(BookmarkToolbarProperties.TITLE));
        assertEquals(NavigationButton.BACK,
                (long) mModel.get(BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE));
    }

    @Test
    public void testDisableSortOptionsInReadingList() {
        doReturn(BookmarkRowSortOrder.MANUAL).when(mBookmarkUiPrefs).getBookmarkRowSortOrder();
        mMediator.onFolderStateSet(mBookmarkId);
        assertEquals(
                R.id.sort_by_manual, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        assertTrue(mModel.get(BookmarkToolbarProperties.SORT_MENU_IDS_ENABLED));

        doReturn(mBookmarkId).when(mBookmarkModel).getReadingListFolder();
        mMediator.onFolderStateSet(mBookmarkId);
        assertFalse(mModel.get(BookmarkToolbarProperties.SORT_MENU_IDS_ENABLED));
        assertEquals(
                R.id.sort_by_newest, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs, times(0)).setBookmarkRowSortOrder(anyInt());

        // Verify  we go back to manual sort order and don't actually update the sorting prefs.
        doReturn(null).when(mBookmarkModel).getReadingListFolder();
        mMediator.onFolderStateSet(mBookmarkId);
        assertTrue(mModel.get(BookmarkToolbarProperties.SORT_MENU_IDS_ENABLED));
        assertEquals(
                R.id.sort_by_manual, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs, times(0)).setBookmarkRowSortOrder(anyInt());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS})
    public void testNavigateBackWhileSearching() {
        String folderName = "test folder";
        doReturn(folderName).when(mBookmarkItem).getTitle();
        mMediator.onFolderStateSet(mBookmarkId);
        assertEquals(folderName, mModel.get(BookmarkToolbarProperties.TITLE));

        mMediator.onUiModeChanged(BookmarkUiMode.SEARCHING);
        assertEquals("Search", mModel.get(BookmarkToolbarProperties.TITLE));

        // Pressing the back button should kick you out of search, but not navigate up in the tree.
        mModel.get(BookmarkToolbarProperties.NAVIGATE_BACK_RUNNABLE).run();
        verify(mEndSearchRunnable).run();
        verify(mBookmarkDelegate, never()).openFolder(any());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS})
    public void testSelectionWhileSorting() {
        String folderName = "test folder";
        doReturn(folderName).when(mBookmarkItem).getTitle();
        mMediator.onFolderStateSet(mBookmarkId);
        assertEquals(folderName, mModel.get(BookmarkToolbarProperties.TITLE));

        mMediator.onUiModeChanged(BookmarkUiMode.SEARCHING);
        assertEquals("Search", mModel.get(BookmarkToolbarProperties.TITLE));

        // Simulate the toolbar changing for selection.
        mModel.set(BookmarkToolbarProperties.TITLE, "test");
        mMediator.onSelectionStateChange(Collections.emptyList());
        assertEquals("Search", mModel.get(BookmarkToolbarProperties.TITLE));
    }
}
