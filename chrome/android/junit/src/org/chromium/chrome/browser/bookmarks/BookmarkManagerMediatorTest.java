// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.MockitoHelper.doRunnable;

import android.app.Activity;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
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
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.TestActivity;

import java.util.Arrays;

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
    LargeIconBridge mLargeIconBridge;
    @Mock
    BookmarkUiObserver mBookmarkUiObserver;
    @Mock
    Profile mProfile;
    @Mock
    SyncService mSyncService;
    @Mock
    IdentityServicesProvider mIdentityServicesProvider;
    @Mock
    SigninManager mSigninManager;
    @Mock
    IdentityManager mIdentityManager;
    @Mock
    AccountManagerFacade mAccountManagerFacade;
    @Mock
    BookmarkUndoController mBookmarkUndoController;

    final ObservableSupplierImpl<Boolean> mBackPressStateSupplier = new ObservableSupplierImpl<>();
    final ObservableSupplierImpl<Boolean> mSelectableListLayoutHandleBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    final BookmarkId mFolderId = new BookmarkId(/*id=*/1, BookmarkType.NORMAL);
    final BookmarkId mFolder2Id = new BookmarkId(/*id=*/2, BookmarkType.NORMAL);

    private ActivityScenario<TestActivity> mActivityScenario;
    private Activity mActivity;
    private BookmarkManagerMediator mMediator;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> {
            mActivity = activity;

            // Setup BookmarkModel.
            doReturn(true).when(mBookmarkModel).doesBookmarkExist(any());
            doReturn(Arrays.asList(mFolder2Id)).when(mBookmarkModel).getChildIDs(mFolderId);
            BookmarkItem bookmarkItem =
                    new BookmarkItem(mFolderId, "Folder", null, true, null, true, false, 0, false);
            doReturn(bookmarkItem).when(mBookmarkModel).getBookmarkById(any());

            // Setup SelectableListLayout.
            doReturn(mActivity).when(mSelectableListLayout).getContext();
            doReturn(mSelectableListLayoutHandleBackPressChangedSupplier)
                    .when(mSelectableListLayout)
                    .getHandleBackPressChangedSupplier();

            // Setup BookmarkUIObserver.
            doRunnable(() -> mMediator.removeUiObserver(mBookmarkUiObserver))
                    .when(mBookmarkUiObserver)
                    .onDestroy();

            // Setup sync/identify mocks.
            SyncService.overrideForTests(mSyncService);
            IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
            doReturn(mSigninManager).when(mIdentityServicesProvider).getSigninManager(any());
            doReturn(mIdentityManager).when(mSigninManager).getIdentityManager();
            AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacade);

            BookmarkItemsAdapter bookmarkItemsAdapter =
                    new BookmarkItemsAdapter(mActivity, (a, b) -> null, (a, b, c) -> {});
            mMediator = new BookmarkManagerMediator(mActivity, mBookmarkModel, mBookmarkOpener,
                    mSelectableListLayout, mSelectionDelegate, mRecyclerView, bookmarkItemsAdapter,
                    mLargeIconBridge, /*isDialogUi=*/true, /*isIncognito=*/false,
                    mBackPressStateSupplier, mProfile, mBookmarkUndoController);
            mMediator.addUiObserver(mBookmarkUiObserver);
        });
    }

    void finishLoading() {
        mMediator.onBookmarkModelLoaded();
    }

    @Test
    public void initAndLoadBookmarkModel() {
        finishLoading();
        Assert.assertEquals(BookmarkUiMode.LOADING, mMediator.getCurrentUiMode());
    }

    @Test
    public void setUrlBeforeModelLoaded() {
        // Setting a URL prior to the model loading should set the state for when it loads.
        mMediator.updateForUrl("chrome-native://bookmarks/folder/" + mFolderId.getId());

        finishLoading();
        Assert.assertEquals(BookmarkUiMode.FOLDER, mMediator.getCurrentUiMode());
    }

    @Test
    public void syncStateChangedBeforeModelLoaded() {
        SyncStateChangedListener syncStateChangedListener =
                mMediator.getSyncStateChangedListenerForTesting();
        syncStateChangedListener.syncStateChanged();
        verify(mBookmarkModel, times(0)).getDesktopFolderId();
        verify(mBookmarkModel, times(0)).getMobileFolderId();
        verify(mBookmarkModel, times(0)).getOtherFolderId();
        verify(mBookmarkModel, times(0)).getTopLevelFolderIDs(true, false);

        finishLoading();
        verify(mBookmarkModel, times(1)).getDesktopFolderId();
        verify(mBookmarkModel, times(1)).getMobileFolderId();
        verify(mBookmarkModel, times(1)).getOtherFolderId();
        verify(mBookmarkModel, times(1)).getTopLevelFolderIDs(true, false);
    }

    @Test
    public void testDestroy() {
        finishLoading();

        mMediator.onDestroy();
        verify(mBookmarkUiObserver).onDestroy();
        verify(mBookmarkUndoController).destroy();
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

    @Test
    public void testAttachmentChanges() {
        mMediator.onAttachedToWindow();
        verify(mBookmarkUndoController).setEnabled(true);

        mMediator.onDetachedFromWindow();
        verify(mBookmarkUndoController).setEnabled(false);
    }
}