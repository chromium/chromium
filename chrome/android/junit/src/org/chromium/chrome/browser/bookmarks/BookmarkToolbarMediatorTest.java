// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListToolbar.NavigationButton;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;

/** Unit tests for {@link BookmarkToolbarMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class BookmarkToolbarMediatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    BookmarkDelegate mBookmarkDelegate;
    @Mock
    BookmarkItemsAdapter mBookmarkItemsAdapter;
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

    Context mContext;
    BookmarkToolbarMediator mMediator;
    PropertyModel mModel;
    OneshotSupplierImpl<BookmarkDelegate> mBookmarkDelegateSupplier = new OneshotSupplierImpl<>();

    @Before
    public void setUp() {
        mContext = new ContextThemeWrapper(
                ApplicationProvider.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        doReturn(mBookmarkItem).when(mBookmarkModel).getBookmarkById(mBookmarkId);

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

        mMediator = new BookmarkToolbarMediator(mContext, mModel, mBookmarkItemsAdapter,
                mBookmarkDelegateSupplier, mSelectionDelegate, mBookmarkModel);
    }

    public boolean navigationButtonMatchesModel(@NavigationButton int navigationButton) {
        return navigationButton
                == (int) mModel.get(BookmarkToolbarProperties.NAVIGATION_BUTTON_STATE);
    }

    @Test
    public void bookmarkDelegateAvailableSetsUpObserver() {
        mBookmarkDelegateSupplier.set(mBookmarkDelegate);
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
        mBookmarkDelegateSupplier.set(mBookmarkDelegate);
        Mockito.verify(mBookmarkDelegate).addUiObserver(mMediator);

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
        doReturn(topLevelFolders).when(mBookmarkModel).getTopLevelFolderParentIDs();
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
}