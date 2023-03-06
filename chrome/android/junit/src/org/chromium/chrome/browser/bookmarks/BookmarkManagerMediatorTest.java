// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.MockitoHelper.doRunnable;

import android.content.Context;
import android.view.accessibility.AccessibilityManager;

import androidx.recyclerview.widget.RecyclerView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.LargeIconBridge;

import java.util.Arrays;

/** Unit tests for {@link BookmarkManagerMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkManagerMediatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    Context mContext;
    @Mock
    BookmarkModel mBookmarkModel;
    @Mock
    BookmarkOpener mBookmarkOpener;
    @Mock
    SelectableListLayout<BookmarkId> mSelectableListLayout;
    @Mock
    SelectionDelegate<BookmarkId> mSelectionDelegate;
    @Mock
    RecyclerView mRecyclerView;
    @Mock
    BookmarkItemsAdapter mBookmarkItemsAdapter;
    @Mock
    LargeIconBridge mLargeIconBridge;
    @Mock
    AccessibilityManager mAccessibilityManager;
    @Mock
    BookmarkUiObserver mBookmarkUiObserver;

    final ObservableSupplierImpl<Boolean> mBackPressStateSupplier = new ObservableSupplierImpl<>();
    final ObservableSupplierImpl<Boolean> mSelectableListLayoutHandleBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    final BookmarkId mFolderId = new BookmarkId(/*id=*/1, BookmarkType.NORMAL);
    final BookmarkId mFolder2Id = new BookmarkId(/*id=*/2, BookmarkType.NORMAL);

    BookmarkManagerMediator mMediator;

    @Before
    public void setUp() {
        // Setup Context.
        doReturn(mAccessibilityManager)
                .when(mContext)
                .getSystemService(Context.ACCESSIBILITY_SERVICE);

        // Setup BookmarkModel.
        doReturn(true).when(mBookmarkModel).doesBookmarkExist(any());
        doReturn(Arrays.asList(mFolder2Id)).when(mBookmarkModel).getChildIDs(mFolderId);

        // Setup SelectableListLayout.
        doReturn(mContext).when(mSelectableListLayout).getContext();
        doReturn(mSelectableListLayoutHandleBackPressChangedSupplier)
                .when(mSelectableListLayout)
                .getHandleBackPressChangedSupplier();

        // Setup BookmarkUIObserver.
        doRunnable(() -> mMediator.removeUiObserver(mBookmarkUiObserver))
                .when(mBookmarkUiObserver)
                .onDestroy();

        mMediator = new BookmarkManagerMediator(mContext, mBookmarkModel, mBookmarkOpener,
                mSelectableListLayout, mSelectionDelegate, mRecyclerView, mBookmarkItemsAdapter,
                mLargeIconBridge, /*isDialogUi=*/true, /*isIncognito=*/false,
                mBackPressStateSupplier);
        mMediator.addUiObserver(mBookmarkUiObserver);
    }

    void finishLoading() {
        mMediator.onBookmarkModelLoaded();
    }

    @Test
    public void initAndLoadBookmarkModel() {
        finishLoading();
        Assert.assertEquals(BookmarkUiState.STATE_LOADING, mMediator.getCurrentState());
    }

    @Test
    public void setUrlBeforeModelLoaded() {
        // Setting a URL prior to the model loading should set the state for when it loads.
        mMediator.updateForUrl("chrome-native://bookmarks/folder/" + mFolderId.getId());

        finishLoading();
        Assert.assertEquals(BookmarkUiState.STATE_FOLDER, mMediator.getCurrentState());
    }

    @Test
    public void destroyUnregistersObservers() {
        finishLoading();

        mMediator.onDestroy();
        verify(mBookmarkUiObserver).onDestroy();
    }

    @Test
    public void onBackPressed_SelectableListLayoutIntercepts() {
        finishLoading();

        doReturn(true).when(mSelectableListLayout).onBackPressed();

        Assert.assertTrue(mMediator.onBackPressed());
    }

    @Test
    public void onBackPressed_EmptyStateStack() {
        finishLoading();

        mMediator.clearStateStackForTesting();
        Assert.assertFalse(mMediator.onBackPressed());
    }

    @Test
    public void onBackPressed_SingleStateStack() {
        finishLoading();

        Assert.assertFalse(mMediator.onBackPressed());
    }

    @Test
    public void onBackPressed_MultipleStateStack() {
        finishLoading();

        mMediator.openFolder(mFolderId);
        mMediator.openFolder(mFolder2Id);
        Assert.assertTrue(mMediator.onBackPressed());
    }
}