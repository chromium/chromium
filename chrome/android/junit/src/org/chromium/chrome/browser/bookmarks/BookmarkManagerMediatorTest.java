// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doRunnable;

import android.app.Activity;
import android.content.Context;
import android.view.accessibility.AccessibilityManager;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.SyncService.SyncStateChangedListener;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DragListener;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DraggabilityProvider;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.Arrays;
import java.util.Collections;

/** Unit tests for {@link BookmarkManagerMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.BOOKMARKS_REFRESH, ChromeFeatureList.SHOPPING_LIST})
public class BookmarkManagerMediatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private BookmarkOpener mBookmarkOpener;
    @Mock
    private SelectableListLayout<BookmarkId> mSelectableListLayout;
    @Mock
    private SelectionDelegate<BookmarkId> mSelectionDelegate;
    @Mock
    private RecyclerView mRecyclerView;
    @Mock
    private LargeIconBridge mLargeIconBridge;
    @Mock
    private BookmarkUiObserver mBookmarkUiObserver;
    @Mock
    private Profile mProfile;
    @Mock
    private SyncService mSyncService;
    @Mock
    private IdentityServicesProvider mIdentityServicesProvider;
    @Mock
    private SigninManager mSigninManager;
    @Mock
    private IdentityManager mIdentityManager;
    @Mock
    private AccountManagerFacade mAccountManagerFacade;
    @Mock
    private BookmarkUndoController mBookmarkUndoController;
    @Mock
    AccessibilityManager mAccessibilityManager;
    @Mock
    private BookmarkUiPrefs mBookmarkUiPrefs;

    @Captor
    private ArgumentCaptor<BookmarkModelObserver> mBookmarkModelObserverArgumentCaptor;
    @Captor
    private ArgumentCaptor<DragListener> mDragListenerArgumentCaptor;
    @Captor
    private ArgumentCaptor<SyncStateChangedListener> mSyncStateChangedListenerCaptor;
    @Captor
    private ArgumentCaptor<Runnable> mFinishLoadingBookmarkModelCaptor;

    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean>
            mSelectableListLayoutHandleBackPressChangedSupplier = new ObservableSupplierImpl<>();
    private final BookmarkId mFolderId1 = new BookmarkId(/*id=*/1, BookmarkType.NORMAL);
    private final BookmarkId mFolderId2 = new BookmarkId(/*id=*/2, BookmarkType.NORMAL);
    private final BookmarkId mFolderId3 = new BookmarkId(/*id=*/3, BookmarkType.NORMAL);
    private final ModelList mModelList = new ModelList();

    private Activity mActivity;
    private BookmarkManagerMediator mMediator;
    private DragReorderableRecyclerViewAdapter mDragReorderableRecyclerViewAdapter;
    private final BookmarkItem mFolderItem1 =
            new BookmarkItem(mFolderId1, "Folder1", null, true, null, true, false, 0, false);
    private final BookmarkItem mFolderItem2 =
            new BookmarkItem(mFolderId2, "Folder2", null, true, mFolderId1, true, false, 0, false);
    private final BookmarkItem mFolderItem3 =
            new BookmarkItem(mFolderId3, "Folder3", null, true, mFolderId1, true, false, 0, false);

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> {
            mActivity = spy(activity);

            // Setup BookmarkModel.
            doReturn(true).when(mBookmarkModel).doesBookmarkExist(any());
            doReturn(Arrays.asList(mFolderId2, mFolderId3))
                    .when(mBookmarkModel)
                    .getChildIds(mFolderId1);
            doReturn(mFolderItem1).when(mBookmarkModel).getBookmarkById(mFolderId1);
            doReturn(mFolderItem2).when(mBookmarkModel).getBookmarkById(mFolderId2);
            doReturn(mFolderItem3).when(mBookmarkModel).getBookmarkById(mFolderId3);

            // Setup SelectableListLayout.
            doReturn(mActivity).when(mSelectableListLayout).getContext();
            doReturn(mSelectableListLayoutHandleBackPressChangedSupplier)
                    .when(mSelectableListLayout)
                    .getHandleBackPressChangedSupplier();
            doReturn(mAccessibilityManager)
                    .when(mActivity)
                    .getSystemService(Context.ACCESSIBILITY_SERVICE);

            // Setup BookmarkUIObserver.
            doRunnable(() -> mMediator.removeUiObserver(mBookmarkUiObserver))
                    .when(mBookmarkUiObserver)
                    .onDestroy();

            // Setup SharedPreferencesManager.
            doReturn(BookmarkRowDisplayPref.COMPACT)
                    .when(mBookmarkUiPrefs)
                    .getBookmarkRowDisplayPref();

            // Setup sync/identify mocks.
            SyncService.overrideForTests(mSyncService);
            IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
            doReturn(mSigninManager).when(mIdentityServicesProvider).getSigninManager(any());
            doReturn(mIdentityManager).when(mSigninManager).getIdentityManager();
            AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacade);

            mDragReorderableRecyclerViewAdapter =
                    spy(new DragReorderableRecyclerViewAdapter(mActivity, mModelList));
            mMediator = new BookmarkManagerMediator(mActivity, mBookmarkModel, mBookmarkOpener,
                    mSelectableListLayout, mSelectionDelegate, mRecyclerView,
                    mDragReorderableRecyclerViewAdapter, mLargeIconBridge, /*isDialogUi=*/true,
                    /*isIncognito=*/false, mBackPressStateSupplier, mProfile,
                    mBookmarkUndoController, mModelList, mBookmarkUiPrefs);
            mMediator.addUiObserver(mBookmarkUiObserver);
        });
    }

    void finishLoading() {
        when(mBookmarkModel.isBookmarkModelLoaded()).thenReturn(true);
        verify(mBookmarkModel, atLeast(0))
                .finishLoadingBookmarkModel(mFinishLoadingBookmarkModelCaptor.capture());
        for (Runnable finishLoadingBookmarkModel :
                mFinishLoadingBookmarkModelCaptor.getAllValues()) {
            finishLoadingBookmarkModel.run();
        }
    }

    @Test
    public void initAndLoadBookmarkModel() {
        finishLoading();
        assertEquals(BookmarkUiMode.LOADING, mMediator.getCurrentUiMode());
    }

    @Test
    public void setUrlBeforeModelLoaded() {
        // Setting a URL prior to the model loading should set the state for when it loads.
        mMediator.updateForUrl("chrome-native://bookmarks/folder/" + mFolderId1.getId());

        finishLoading();
        assertEquals(BookmarkUiMode.FOLDER, mMediator.getCurrentUiMode());
    }

    @Test
    public void syncStateChangedBeforeModelLoaded() {
        verify(mSyncService, atLeast(1))
                .addSyncStateChangedListener(mSyncStateChangedListenerCaptor.capture());
        for (SyncStateChangedListener syncStateChangedListener :
                mSyncStateChangedListenerCaptor.getAllValues()) {
            syncStateChangedListener.syncStateChanged();
        }

        verify(mBookmarkModel, times(0)).getDesktopFolderId();
        verify(mBookmarkModel, times(0)).getMobileFolderId();
        verify(mBookmarkModel, times(0)).getOtherFolderId();
        verify(mBookmarkModel, times(0)).getTopLevelFolderIds(true, false);

        finishLoading();
        verify(mBookmarkModel, times(1)).getDesktopFolderId();
        verify(mBookmarkModel, times(1)).getMobileFolderId();
        verify(mBookmarkModel, times(1)).getOtherFolderId();
        verify(mBookmarkModel, times(1)).getTopLevelFolderIds(true, false);
    }

    @Test
    public void testDestroy() {
        finishLoading();

        mMediator.onDestroy();
        verify(mBookmarkUiObserver).onDestroy();
        verify(mBookmarkUndoController).destroy();
        verify(mAccessibilityManager).removeAccessibilityStateChangeListener(any());
    }

    @Test
    public void onBackPressed_SelectableListLayoutIntercepts() {
        finishLoading();

        doReturn(true).when(mSelectableListLayout).onBackPressed();

        assertTrue(mMediator.onBackPressed());
    }

    @Test
    public void onBackPressed_EmptyStateStack() {
        finishLoading();

        mMediator.clearStateStackForTesting();
        assertFalse(mMediator.onBackPressed());
    }

    @Test
    public void onBackPressed_SingleStateStack() {
        finishLoading();

        assertFalse(mMediator.onBackPressed());
    }

    @Test
    public void onBackPressed_MultipleStateStack() {
        finishLoading();

        mMediator.openFolder(mFolderId1);
        mMediator.openFolder(mFolderId2);
        assertTrue(mMediator.onBackPressed());
    }

    @Test
    public void testMoveDownUp() {
        finishLoading();
        mMediator.openFolder(mFolderId1);

        mMediator.moveDownOne(mFolderId2);
        verify(mBookmarkModel)
                .reorderBookmarks(mFolderId1, new long[] {mFolderId3.getId(), mFolderId2.getId()});

        mMediator.moveUpOne(mFolderId2);
        verify(mBookmarkModel)
                .reorderBookmarks(mFolderId1, new long[] {mFolderId2.getId(), mFolderId3.getId()});
    }

    @Test
    public void testDrag() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        DraggabilityProvider draggabilityProvider = mMediator.getDraggabilityProvider();
        assertTrue(draggabilityProvider.isPassivelyDraggable(mModelList.get(0).model));
        assertFalse(draggabilityProvider.isActivelyDraggable(mModelList.get(0).model));

        when(mSelectionDelegate.isItemSelected(mFolderId2)).thenReturn(true);
        assertTrue(draggabilityProvider.isActivelyDraggable(mModelList.get(0).model));

        mModelList.move(0, 1);
        verify(mDragReorderableRecyclerViewAdapter)
                .addDragListener(mDragListenerArgumentCaptor.capture());
        mDragListenerArgumentCaptor.getValue().onSwap();
        verify(mBookmarkModel)
                .reorderBookmarks(mFolderId1, new long[] {mFolderId3.getId(), mFolderId2.getId()});
    }

    @Test
    public void testSearch() {
        when(mBookmarkModel.searchBookmarks(anyString(), anyInt()))
                .thenReturn(Collections.singletonList(mFolderId3));
        finishLoading();

        mMediator.openFolder(mFolderId1);
        assertEquals(2, mModelList.size());

        mMediator.search("3");
        assertEquals(1, mModelList.size());

        mMediator.closeSearchUi();
        assertEquals(2, mModelList.size());
    }

    @Test
    public void testBookmarkRemoved() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        assertEquals(2, mModelList.size());

        doReturn(Arrays.asList(mFolderId3)).when(mBookmarkModel).getChildIds(mFolderId1);
        verify(mBookmarkModel, times(2))
                .addObserver(mBookmarkModelObserverArgumentCaptor.capture());
        for (BookmarkModelObserver bookmarkModelObserver :
                mBookmarkModelObserverArgumentCaptor.getAllValues()) {
            bookmarkModelObserver.bookmarkNodeRemoved(
                    mFolderItem1, 0, mFolderItem2, /*isDoingExtensiveChanges*/ false);
        }

        assertEquals(1, mModelList.size());
    }

    @Test
    public void testAttachmentChanges() {
        mMediator.onAttachedToWindow();
        verify(mBookmarkUndoController).setEnabled(true);

        mMediator.onDetachedFromWindow();
        verify(mBookmarkUndoController).setEnabled(false);
    }

    @Test
    public void onPreferenceChanged_ViewPreferenceUpdated() {
        mMediator.onBookmarkRowDisplayPrefChanged();
        verify(mRecyclerView).setAdapter(mDragReorderableRecyclerViewAdapter);
    }
}