// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
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

import org.hamcrest.MatcherAssert;
import org.hamcrest.Matchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
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
import org.chromium.chrome.browser.app.bookmarks.BookmarkEditActivity;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.function.BooleanSupplier;

/** Unit tests for {@link BookmarkToolbarMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class BookmarkToolbarMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarios =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BookmarkDelegate mBookmarkDelegate;
    @Mock private DragReorderableRecyclerViewAdapter mDragReorderableRecyclerViewAdapter;
    @Mock private BookmarkOpener mBookmarkOpener;
    @Mock private SelectionDelegate mSelectionDelegate;
    @Mock private Runnable mNavigateBackRunnable;
    @Mock private BookmarkUiPrefs mBookmarkUiPrefs;
    @Mock private BookmarkAddNewFolderCoordinator mBookmarkAddNewFolderCoordinator;
    @Mock private PropertyObserver<PropertyKey> mPropertyObserver;
    @Mock private Runnable mEndSearchRunnable;
    @Mock private BookmarkMoveSnackbarManager mBookmarkMoveSnackbarManager;
    @Mock private Profile mProfile;

    @Spy private Context mContext;

    FakeBookmarkModel mBookmarkModel;
    BookmarkToolbarMediator mMediator;
    PropertyModel mModel;
    OneshotSupplierImpl<BookmarkDelegate> mBookmarkDelegateSupplier;
    BooleanSupplier mIncognitoEnabledSupplier;
    boolean mIncognitoEnabled = true;

    @Before
    public void setUp() {
        // Setup the context, we need a spy here because the context is used to launch activities.
        mContext =
                spy(
                        new ContextThemeWrapper(
                                ApplicationProvider.getApplicationContext(),
                                R.style.Theme_BrowserUI_DayNight));
        doNothing().when(mContext).startActivity(any());

        // Setup the bookmark model ids/items.
        mBookmarkModel = FakeBookmarkModel.createModel();

        mIncognitoEnabledSupplier = () -> mIncognitoEnabled;

        initModelAndMediator();
    }

    private void initModelAndMediator() {
        mModel =
                new PropertyModel.Builder(BookmarkToolbarProperties.ALL_KEYS)
                        .with(BookmarkToolbarProperties.BOOKMARK_OPENER, mBookmarkOpener)
                        .with(BookmarkToolbarProperties.SELECTION_DELEGATE, mSelectionDelegate)
                        .with(BookmarkToolbarProperties.BOOKMARK_UI_MODE, BookmarkUiMode.LOADING)
                        .with(BookmarkToolbarProperties.IS_DIALOG_UI, false)
                        .with(BookmarkToolbarProperties.DRAG_ENABLED, false)
                        .with(
                                BookmarkToolbarProperties.NAVIGATE_BACK_RUNNABLE,
                                mNavigateBackRunnable)
                        .build();
        mBookmarkDelegateSupplier = new OneshotSupplierImpl<>();
        mMediator =
                new BookmarkToolbarMediator(
                        mContext,
                        mModel,
                        mDragReorderableRecyclerViewAdapter,
                        mBookmarkDelegateSupplier,
                        mSelectionDelegate,
                        mBookmarkModel,
                        mBookmarkOpener,
                        mBookmarkUiPrefs,
                        mBookmarkAddNewFolderCoordinator,
                        mEndSearchRunnable,
                        mBookmarkMoveSnackbarManager,
                        mIncognitoEnabledSupplier);
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

        mMediator =
                new BookmarkToolbarMediator(
                        mContext,
                        mModel,
                        mDragReorderableRecyclerViewAdapter,
                        mBookmarkDelegateSupplier,
                        mSelectionDelegate,
                        mBookmarkModel,
                        mBookmarkOpener,
                        mBookmarkUiPrefs,
                        mBookmarkAddNewFolderCoordinator,
                        mEndSearchRunnable,
                        mBookmarkMoveSnackbarManager,
                        mIncognitoEnabledSupplier);
    }

    @Test
    public void bookmarkDelegateAvailableSetsUpObserver() {
        verify(mBookmarkDelegate).addUiObserver(mMediator);
        verify(mSelectionDelegate).addObserver(mMediator);
    }

    @Test
    public void onStateChangedUpdatesModel() {
        mMediator.onUiModeChanged(BookmarkUiMode.LOADING);
        assertEquals(
                BookmarkUiMode.LOADING,
                mModel.get(BookmarkToolbarProperties.BOOKMARK_UI_MODE).intValue());

        mMediator.onUiModeChanged(BookmarkUiMode.SEARCHING);
        assertEquals(
                BookmarkUiMode.SEARCHING,
                mModel.get(BookmarkToolbarProperties.BOOKMARK_UI_MODE).intValue());

        mMediator.onUiModeChanged(BookmarkUiMode.FOLDER);
        assertEquals(
                BookmarkUiMode.FOLDER,
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
    public void selectionStateChangeHidesKeyboard() {
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
        mMediator.onFolderStateSet(mBookmarkModel.getRootFolderId());

        assertFalse(mModel.get(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE));
        assertEquals("Bookmarks", mModel.get(BookmarkToolbarProperties.TITLE));
        assertTrue(navigationButtonMatchesModel(NavigationButton.NONE));
    }

    @Test
    public void onFolderStateSet_EmptyTitleWhenChildOfRoot() {
        BookmarkId emptyTitleFolderId =
                mBookmarkModel.addFolder(mBookmarkModel.getTopLevelFolderIds().get(0), 0, "");
        mMediator.onFolderStateSet(emptyTitleFolderId);

        assertTrue(mModel.get(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE));
        assertEquals("Bookmarks", mModel.get(BookmarkToolbarProperties.TITLE));
        assertTrue(navigationButtonMatchesModel(NavigationButton.NORMAL_VIEW_BACK));
    }

    @Test
    public void onFolderStateSet_RegularFolder() {
        BookmarkId folderId =
                mBookmarkModel.addFolder(new BookmarkId(7, BookmarkType.NORMAL), 0, "test folder");
        mMediator.onFolderStateSet(folderId);

        assertTrue(mModel.get(BookmarkToolbarProperties.EDIT_BUTTON_VISIBLE));
        assertEquals("test folder", mModel.get(BookmarkToolbarProperties.TITLE));
        assertTrue(navigationButtonMatchesModel(NavigationButton.NORMAL_VIEW_BACK));
    }

    @Test
    public void testOnMenuItemClick_editMenu() {
        mMediator.onFolderStateSet(
                mBookmarkModel.addFolder(new BookmarkId(7, BookmarkType.NORMAL), 0, ""));
        assertTrue(
                mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                        .apply(R.id.edit_menu_id));
        verifyActivityLaunched(BookmarkEditActivity.class);
    }

    @Test
    public void testOnMenuItemClick_closeMenu() {
        assertTrue(
                mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                        .apply(R.id.close_menu_id));
        // Difficult to verify that the activity has been finished, especially in a unit test.
    }

    @Test
    public void testOnMenuItemClick_selectionModeEditMenu() {
        setCurrentSelection(
                mBookmarkModel.addBookmark(
                        new BookmarkId(7, BookmarkType.NORMAL),
                        0,
                        "Test",
                        JUnitTestGURLs.EXAMPLE_URL));
        assertTrue(
                mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                        .apply(R.id.selection_mode_edit_menu_id));
        verifyActivityLaunched(BookmarkEditActivity.class);
    }

    @Test
    public void testOnMenuItemClick_selectionModeEditMenuFolder() {
        setCurrentSelection(
                mBookmarkModel.addFolder(new BookmarkId(7, BookmarkType.NORMAL), 0, ""));
        assertTrue(
                mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                        .apply(R.id.selection_mode_edit_menu_id));
        verifyActivityLaunched(BookmarkEditActivity.class);
    }

    @Test
    public void testOnMenuItemClick_selectionModeMoveMenu() {
        BookmarkId bookmarkId = new BookmarkId(7, BookmarkType.NORMAL);
        setCurrentSelection(bookmarkId);
        assertTrue(
                mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                        .apply(R.id.selection_mode_move_menu_id));
        verify(mBookmarkMoveSnackbarManager).startFolderPickerAndObserveResult(bookmarkId);
    }

    @Test
    public void testOnMenuItemClick_selectionModeDeleteMenu() {
        BookmarkId bookmarkId =
                mBookmarkModel.addBookmark(
                        new BookmarkId(7, BookmarkType.NORMAL),
                        0,
                        "Test",
                        JUnitTestGURLs.EXAMPLE_URL);
        setCurrentSelection(bookmarkId);

        assertNotNull(mBookmarkModel.getBookmarkById(bookmarkId));
        assertTrue(
                mModel.get(BookmarkToolbarProperties.MENU_ID_CLICKED_FUNCTION)
                        .apply(R.id.selection_mode_delete_menu_id));
        assertNull(mBookmarkModel.getBookmarkById(bookmarkId));
    }

    @Test
    public void testOnMenuItemClick_selectionOpenInNewTab() {
        setCurrentSelection(new BookmarkId(7, BookmarkType.NORMAL));
        assertTrue(mMediator.onMenuIdClick(R.id.selection_open_in_new_tab_id));
        verify(mBookmarkOpener).openBookmarksInNewTabs(any(), eq(false));
    }

    @Test
    public void testOnMenuItemClick_selectionOpenInIncognitoTab() {
        setCurrentSelection(new BookmarkId(7, BookmarkType.NORMAL));
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
        mMediator.onFolderStateSet(
                mBookmarkModel.addFolder(new BookmarkId(7, BookmarkType.NORMAL), 0, ""));
        assertTrue(mModel.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_VISIBLE));
        assertTrue(mModel.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_ENABLED));
        assertTrue(mMediator.onMenuIdClick(R.id.create_new_folder_menu_id));
        verify(mBookmarkAddNewFolderCoordinator).show(any());

        mMediator.onFolderStateSet(mBookmarkModel.getLocalOrSyncableReadingListFolder());
        assertTrue(mModel.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_VISIBLE));
        assertFalse(mModel.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_ENABLED));

        mMediator.onFolderStateSet(mBookmarkModel.getAccountReadingListFolder());
        assertTrue(mModel.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_VISIBLE));
        assertFalse(mModel.get(BookmarkToolbarProperties.NEW_FOLDER_BUTTON_ENABLED));

        mMediator.onFolderStateSet(mBookmarkModel.getPartnerFolderId());
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
        assertEquals(
                R.id.sort_by_last_opened,
                mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowSortOrder(BookmarkRowSortOrder.RECENTLY_USED);

        assertTrue(mMediator.onMenuIdClick(R.id.sort_by_alpha));
        assertEquals(
                R.id.sort_by_alpha, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs).setBookmarkRowSortOrder(BookmarkRowSortOrder.ALPHABETICAL);

        assertTrue(mMediator.onMenuIdClick(R.id.sort_by_reverse_alpha));
        assertEquals(
                R.id.sort_by_reverse_alpha,
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
        assertEquals(
                R.id.sort_by_reverse_alpha,
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
        assertEquals(
                R.id.sort_by_reverse_alpha,
                mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
    }

    @Test
    public void testTitleAndNavWhenSearching() {
        String folderName = "test folder";
        mMediator.onFolderStateSet(
                mBookmarkModel.addFolder(new BookmarkId(7, BookmarkType.NORMAL), 0, folderName));
        assertEquals(folderName, mModel.get(BookmarkToolbarProperties.TITLE));

        mMediator.onUiModeChanged(BookmarkUiMode.SEARCHING);
        assertEquals("Search", mModel.get(BookmarkToolbarProperties.TITLE));
        assertEquals(
                NavigationButton.NORMAL_VIEW_BACK,
                (long) mModel.get(BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE));
    }

    @Test
    public void testDisableSortOptionsInReadingList() {
        doReturn(BookmarkRowSortOrder.MANUAL).when(mBookmarkUiPrefs).getBookmarkRowSortOrder();
        BookmarkId folderId =
                mBookmarkModel.addFolder(new BookmarkId(7, BookmarkType.NORMAL), 0, "");
        mMediator.onFolderStateSet(folderId);
        assertEquals(
                R.id.sort_by_manual, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        assertTrue(mModel.get(BookmarkToolbarProperties.SORT_MENU_IDS_ENABLED));

        mMediator.onFolderStateSet(mBookmarkModel.getLocalOrSyncableReadingListFolder());
        assertFalse(mModel.get(BookmarkToolbarProperties.SORT_MENU_IDS_ENABLED));
        assertEquals(
                R.id.sort_by_newest, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs, times(0)).setBookmarkRowSortOrder(anyInt());

        mMediator.onFolderStateSet(mBookmarkModel.getAccountReadingListFolder());
        assertFalse(mModel.get(BookmarkToolbarProperties.SORT_MENU_IDS_ENABLED));
        assertEquals(
                R.id.sort_by_newest, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs, times(0)).setBookmarkRowSortOrder(anyInt());

        // Verify  we go back to manual sort order and don't actually update the sorting prefs.
        mMediator.onFolderStateSet(folderId);
        assertTrue(mModel.get(BookmarkToolbarProperties.SORT_MENU_IDS_ENABLED));
        assertEquals(
                R.id.sort_by_manual, mModel.get(BookmarkToolbarProperties.CHECKED_SORT_MENU_ID));
        verify(mBookmarkUiPrefs, times(0)).setBookmarkRowSortOrder(anyInt());
    }

    @Test
    public void testNavigateBackWhileSearching() {
        String folderName = "test folder";
        mMediator.onFolderStateSet(
                mBookmarkModel.addFolder(new BookmarkId(7, BookmarkType.NORMAL), 0, folderName));
        assertEquals(folderName, mModel.get(BookmarkToolbarProperties.TITLE));

        mMediator.onUiModeChanged(BookmarkUiMode.SEARCHING);
        assertEquals("Search", mModel.get(BookmarkToolbarProperties.TITLE));

        // Pressing the back button should kick you out of search, but not navigate up in the tree.
        mModel.get(BookmarkToolbarProperties.NAVIGATE_BACK_RUNNABLE).run();
        verify(mEndSearchRunnable).run();
        verify(mBookmarkDelegate, never()).openFolder(any());
    }

    @Test
    public void testSelectionWhileSorting() {
        String folderName = "test folder";
        mMediator.onFolderStateSet(
                mBookmarkModel.addFolder(new BookmarkId(7, BookmarkType.NORMAL), 0, folderName));
        assertEquals(folderName, mModel.get(BookmarkToolbarProperties.TITLE));

        mMediator.onUiModeChanged(BookmarkUiMode.SEARCHING);
        assertEquals("Search", mModel.get(BookmarkToolbarProperties.TITLE));

        // Simulate the toolbar changing for selection.
        mModel.set(BookmarkToolbarProperties.TITLE, "test");
        mMediator.onSelectionStateChange(Collections.emptyList());
        assertEquals("Search", mModel.get(BookmarkToolbarProperties.TITLE));
    }

    @Test
    public void testSelection_EmptyList() {
        MatcherAssert.assertThat(
                mModel.getAllSetProperties(),
                Matchers.not(
                        Matchers.contains(
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD)));

        mMediator.onSelectionStateChange(Collections.emptyList());

        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD));
    }

    @Test
    public void testSelection_SingleBookmark() {
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();
        MatcherAssert.assertThat(
                mModel.getAllSetProperties(),
                Matchers.not(
                        Matchers.contains(
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD)));

        mMediator.onSelectionStateChange(
                Collections.singletonList(
                        mBookmarkModel.addBookmark(
                                new BookmarkId(7, BookmarkType.NORMAL),
                                0,
                                "Test",
                                JUnitTestGURLs.EXAMPLE_URL)));

        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD));
    }

    @Test
    public void testSelection_SingleFolder() {
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();
        MatcherAssert.assertThat(
                mModel.getAllSetProperties(),
                Matchers.not(
                        Matchers.contains(
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD)));

        mMediator.onSelectionStateChange(
                Collections.singletonList(
                        mBookmarkModel.addFolder(
                                new BookmarkId(7, BookmarkType.NORMAL), 0, "Test")));

        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD));
    }

    @Test
    public void testSelection_MultipleBookmark() {
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();
        MatcherAssert.assertThat(
                mModel.getAllSetProperties(),
                Matchers.not(
                        Matchers.contains(
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD)));

        mMediator.onSelectionStateChange(
                Arrays.asList(
                        mBookmarkModel.addBookmark(
                                new BookmarkId(7, BookmarkType.NORMAL),
                                0,
                                "Test",
                                JUnitTestGURLs.EXAMPLE_URL),
                        mBookmarkModel.addBookmark(
                                new BookmarkId(7, BookmarkType.NORMAL),
                                1,
                                "Test2",
                                JUnitTestGURLs.EXAMPLE_URL),
                        mBookmarkModel.addBookmark(
                                new BookmarkId(7, BookmarkType.NORMAL),
                                2,
                                "Test3",
                                JUnitTestGURLs.EXAMPLE_URL)));

        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD));
    }

    @Test
    public void testSelection_MultipleFolder() {
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();
        MatcherAssert.assertThat(
                mModel.getAllSetProperties(),
                Matchers.not(
                        Matchers.contains(
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD)));

        mMediator.onSelectionStateChange(
                Arrays.asList(
                        mBookmarkModel.addFolder(
                                new BookmarkId(7, BookmarkType.NORMAL), 0, "Test1"),
                        mBookmarkModel.addFolder(
                                new BookmarkId(7, BookmarkType.NORMAL), 1, "Test2"),
                        mBookmarkModel.addFolder(
                                new BookmarkId(7, BookmarkType.NORMAL), 2, "Test3")));

        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD));
    }

    @Test
    public void testSelection_MixedBookmarkAndFolders() {
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();
        MatcherAssert.assertThat(
                mModel.getAllSetProperties(),
                Matchers.not(
                        Matchers.contains(
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD)));

        mMediator.onSelectionStateChange(
                Arrays.asList(
                        mBookmarkModel.addBookmark(
                                new BookmarkId(7, BookmarkType.NORMAL),
                                0,
                                "Test",
                                JUnitTestGURLs.EXAMPLE_URL),
                        mBookmarkModel.addFolder(
                                new BookmarkId(7, BookmarkType.NORMAL), 1, "Test2")));

        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD));
    }

    @Test
    public void testSelection_PartnerBookmark() {
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();
        MatcherAssert.assertThat(
                mModel.getAllSetProperties(),
                Matchers.not(
                        Matchers.contains(
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD)));

        mMediator.onSelectionStateChange(
                Collections.singletonList(
                        mBookmarkModel.addPartnerBookmarkItem("Test", JUnitTestGURLs.EXAMPLE_URL)));

        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD));
    }

    @Test
    public void testSelection_ReadingList_AllReadItems() {
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();
        MatcherAssert.assertThat(
                mModel.getAllSetProperties(),
                Matchers.not(
                        Matchers.contains(
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD)));

        BookmarkId readingListItem1 =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getDefaultReadingListFolder(),
                        "Test1",
                        JUnitTestGURLs.EXAMPLE_URL);
        BookmarkId readingListItem2 =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getDefaultReadingListFolder(),
                        "Test2",
                        JUnitTestGURLs.EXAMPLE_URL);
        BookmarkId readingListItem3 =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getDefaultReadingListFolder(),
                        "Test2",
                        JUnitTestGURLs.EXAMPLE_URL);

        mBookmarkModel.setReadStatusForReadingList(readingListItem1, true);
        mBookmarkModel.setReadStatusForReadingList(readingListItem2, true);
        mBookmarkModel.setReadStatusForReadingList(readingListItem3, true);

        mMediator.onSelectionStateChange(
                Arrays.asList(readingListItem1, readingListItem2, readingListItem3));

        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD));
    }

    @Test
    public void testSelection_ReadingList_AllUnreadItems() {
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();
        MatcherAssert.assertThat(
                mModel.getAllSetProperties(),
                Matchers.not(
                        Matchers.contains(
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD)));

        BookmarkId readingListItem1 =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getDefaultReadingListFolder(),
                        "Test1",
                        JUnitTestGURLs.EXAMPLE_URL);
        BookmarkId readingListItem2 =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getDefaultReadingListFolder(),
                        "Test2",
                        JUnitTestGURLs.EXAMPLE_URL);
        BookmarkId readingListItem3 =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getDefaultReadingListFolder(),
                        "Test2",
                        JUnitTestGURLs.EXAMPLE_URL);

        mBookmarkModel.setReadStatusForReadingList(readingListItem1, false);
        mBookmarkModel.setReadStatusForReadingList(readingListItem2, false);
        mBookmarkModel.setReadStatusForReadingList(readingListItem3, false);

        mMediator.onSelectionStateChange(
                Arrays.asList(readingListItem1, readingListItem2, readingListItem3));

        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD));
    }

    @Test
    public void testSelection_ReadingList_MixedReadAndUnreadItems() {
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();
        MatcherAssert.assertThat(
                mModel.getAllSetProperties(),
                Matchers.not(
                        Matchers.contains(
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD)));

        BookmarkId readingListItem1 =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getDefaultReadingListFolder(),
                        "Test1",
                        JUnitTestGURLs.EXAMPLE_URL);
        BookmarkId readingListItem2 =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getDefaultReadingListFolder(),
                        "Test2",
                        JUnitTestGURLs.EXAMPLE_URL);
        BookmarkId readingListItem3 =
                mBookmarkModel.addToReadingList(
                        mBookmarkModel.getDefaultReadingListFolder(),
                        "Test2",
                        JUnitTestGURLs.EXAMPLE_URL);

        mBookmarkModel.setReadStatusForReadingList(readingListItem1, false);
        mBookmarkModel.setReadStatusForReadingList(readingListItem2, true);
        mBookmarkModel.setReadStatusForReadingList(readingListItem3, false);

        mMediator.onSelectionStateChange(
                Arrays.asList(readingListItem1, readingListItem2, readingListItem3));

        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD));
    }

    @Test
    public void testSelection_IncognitoEnabledMode() {
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();
        MatcherAssert.assertThat(
                mModel.getAllSetProperties(),
                Matchers.not(
                        Matchers.contains(
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_EDIT,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_NEW_TAB,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MOVE,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_READ,
                                BookmarkToolbarProperties.SELECTION_MODE_SHOW_MARK_UNREAD)));

        BookmarkId bookmark =
                mBookmarkModel.addBookmark(
                        new BookmarkId(7, BookmarkType.NORMAL),
                        0,
                        "Test",
                        JUnitTestGURLs.EXAMPLE_URL);

        mIncognitoEnabled = true;
        mMediator.onSelectionStateChange(Collections.singletonList(bookmark));
        assertTrue(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO));

        mIncognitoEnabled = false;
        mMediator.onSelectionStateChange(Collections.singletonList(bookmark));
        assertFalse(mModel.get(BookmarkToolbarProperties.SELECTION_MODE_SHOW_OPEN_IN_INCOGNITO));
    }
}
