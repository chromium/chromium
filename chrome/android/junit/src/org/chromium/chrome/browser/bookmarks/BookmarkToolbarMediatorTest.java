// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

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
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.PropertyModel;

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

    BookmarkToolbarMediator mMediator;
    PropertyModel mModel;
    OneshotSupplierImpl<BookmarkDelegate> mBookmarkDelegateSupplier = new OneshotSupplierImpl<>();

    @Before
    public void setUp() {
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

        mMediator = new BookmarkToolbarMediator(
                mModel, mBookmarkItemsAdapter, mBookmarkDelegateSupplier, mSelectionDelegate);
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
}