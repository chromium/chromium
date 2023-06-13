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
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doRunnable;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.util.Pair;

import androidx.annotation.StringRes;
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

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.chrome.browser.bookmarks.BookmarkRow.Location;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.StartImageVisibility;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.SyncService.SyncStateChangedListener;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DragListener;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DraggabilityProvider;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.CurrencyFormatterJni;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ProductPrice;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Collections;

/** Unit tests for {@link BookmarkManagerMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.BOOKMARKS_REFRESH, ChromeFeatureList.SHOPPING_LIST,
        ChromeFeatureList.EMPTY_STATES})
public class BookmarkManagerMediatorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

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
    private Runnable mHideKeyboardRunnable;
    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock
    private CurrencyFormatter.Natives mCurrencyFormatterJniMock;
    @Mock
    private Tracker mTracker;
    @Mock
    private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock
    private Drawable mDrawable;

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
    private final BookmarkId mRootFolderId = new BookmarkId(/*id=*/1, BookmarkType.NORMAL);
    private final BookmarkId mFolderId1 = new BookmarkId(/*id=*/2, BookmarkType.NORMAL);
    private final BookmarkId mFolderId2 = new BookmarkId(/*id=*/3, BookmarkType.NORMAL);
    private final BookmarkId mFolderId3 = new BookmarkId(/*id=*/4, BookmarkType.NORMAL);
    private final BookmarkId mBookmarkId21 = new BookmarkId(/*id=*/5, BookmarkType.NORMAL);
    private final BookmarkId mReadingListFolderId =
            new BookmarkId(/*id=*/5, BookmarkType.READING_LIST);
    private final BookmarkId mReadingListId = new BookmarkId(/*id=*/6, BookmarkType.READING_LIST);
    private final BookmarkId mDesktopFolderId = new BookmarkId(/*id=*/7, BookmarkType.NORMAL);

    private final BookmarkItem mFolderItem1 = new BookmarkItem(
            mFolderId1, "Folder1", null, true, mRootFolderId, true, false, 0, false);
    private final BookmarkItem mFolderItem2 =
            new BookmarkItem(mFolderId2, "Folder2", null, true, mFolderId1, true, false, 0, false);
    private final BookmarkItem mFolderItem3 =
            new BookmarkItem(mFolderId3, "Folder3", null, true, mFolderId1, true, false, 0, false);
    private final BookmarkItem mBookmarkItem21 = new BookmarkItem(mBookmarkId21, "Bookmark21",
            JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), false, mFolderId2, true, false, 0,
            false);
    private final BookmarkItem mReadingListFolderItem = new BookmarkItem(mReadingListFolderId,
            "Reading List", null, true, mRootFolderId, false, false, 0, false);
    private final BookmarkItem mReadingListItem = new BookmarkItem(mReadingListId,
            JUnitTestGURLs.EXAMPLE_URL, JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), false,
            mReadingListFolderId, true, false, 0, false);
    private final BookmarkItem mDesktopFolderItem = new BookmarkItem(
            mDesktopFolderId, "Desktop", null, true, mRootFolderId, false, false, 0, false);
    private final ModelList mModelList = new ModelList();
    private final Bitmap mBitmap = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
    private BookmarkUiPrefs mBookmarkUiPrefs =
            new BookmarkUiPrefs(SharedPreferencesManager.getInstance());

    private Activity mActivity;
    private BookmarkManagerMediator mMediator;
    private DragReorderableRecyclerViewAdapter mDragReorderableRecyclerViewAdapter;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> {
            mActivity = spy(activity);

            // Setup UrlFormatter.
            mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
            doAnswer(invocation -> {
                GURL url = invocation.getArgument(0);
                return url.getSpec();
            })
                    .when(mUrlFormatterJniMock)
                    .formatUrlForSecurityDisplay(any(), anyInt());

            // Setup CurrencyFormatter.
            mJniMocker.mock(CurrencyFormatterJni.TEST_HOOKS, mCurrencyFormatterJniMock);

            // Setup TrackerFactory.
            TrackerFactory.setTrackerForTests(mTracker);

            // Setup BookmarkModel.
            doReturn(mRootFolderId).when(mBookmarkModel).getRootFolderId();
            doReturn(mReadingListFolderId).when(mBookmarkModel).getReadingListFolder();
            doReturn(mReadingListFolderItem)
                    .when(mBookmarkModel)
                    .getBookmarkById(mReadingListFolderId);
            doReturn(mDesktopFolderId).when(mBookmarkModel).getDesktopFolderId();
            doReturn(mDesktopFolderItem).when(mBookmarkModel).getBookmarkById(mDesktopFolderId);
            doReturn(true).when(mBookmarkModel).doesBookmarkExist(any());
            doReturn(Arrays.asList(mFolderId2, mFolderId3))
                    .when(mBookmarkModel)
                    .getChildIds(mFolderId1);
            doReturn(mFolderItem1).when(mBookmarkModel).getBookmarkById(mFolderId1);
            doReturn(mFolderItem2).when(mBookmarkModel).getBookmarkById(mFolderId2);
            doReturn(mBookmarkItem21).when(mBookmarkModel).getBookmarkById(mBookmarkId21);
            doReturn(Arrays.asList(mBookmarkId21)).when(mBookmarkModel).getChildIds(mFolderId2);
            doReturn(1).when(mBookmarkModel).getTotalBookmarkCount(mFolderId2);
            doReturn(mFolderItem3).when(mBookmarkModel).getBookmarkById(mFolderId3);
            doReturn(Arrays.asList(mReadingListId))
                    .when(mBookmarkModel)
                    .getChildIds(mReadingListFolderId);
            doReturn(mReadingListFolderItem)
                    .when(mBookmarkModel)
                    .getBookmarkById(mReadingListFolderId);
            doReturn(mReadingListItem).when(mBookmarkModel).getBookmarkById(mReadingListId);

            // Setup SelectableListLayout.
            doReturn(mActivity).when(mSelectableListLayout).getContext();
            doReturn(mSelectableListLayoutHandleBackPressChangedSupplier)
                    .when(mSelectableListLayout)
                    .getHandleBackPressChangedSupplier();

            // Setup BookmarkUIObserver.
            doRunnable(() -> mMediator.removeUiObserver(mBookmarkUiObserver))
                    .when(mBookmarkUiObserver)
                    .onDestroy();

            // Setup LargeIconBridge.
            doAnswer(invocation -> {
                LargeIconCallback cb = invocation.getArgument(3);
                cb.onLargeIconAvailable(mBitmap, Color.GREEN, false, IconType.FAVICON);
                return null;
            })
                    .when(mLargeIconBridge)
                    .getLargeIconForUrl(any(), anyInt(), anyInt(), any());

            // Setup BookmarkUiPrefs.
            mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.COMPACT);

            // Setup sync/identify mocks.
            SyncService.overrideForTests(mSyncService);
            IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
            doReturn(mSigninManager).when(mIdentityServicesProvider).getSigninManager(any());
            doReturn(mIdentityManager).when(mSigninManager).getIdentityManager();
            AccountManagerFacadeProvider.setInstanceForTests(mAccountManagerFacade);

            // Setup image fetching.
            doAnswer((invocation) -> {
                Callback<Pair<Drawable, Drawable>> callback = invocation.getArgument(1);
                callback.onResult(new Pair<>(mDrawable, mDrawable));
                return null;
            })
                    .when(mBookmarkImageFetcher)
                    .fetchFirstTwoImagesForFolder(any(), any());
            doAnswer((invocation) -> {
                Callback<Drawable> callback = invocation.getArgument(1);
                callback.onResult(mDrawable);
                return null;
            })
                    .when(mBookmarkImageFetcher)
                    .fetchImageForBookmarkWithFaviconFallback(any(), any());
            doAnswer((invocation) -> {
                Callback<Drawable> callback = invocation.getArgument(1);
                callback.onResult(mDrawable);
                return null;
            })
                    .when(mBookmarkImageFetcher)
                    .fetchFaviconForBookmark(any(), any());

            mDragReorderableRecyclerViewAdapter =
                    spy(new DragReorderableRecyclerViewAdapter(mActivity, mModelList));
            mMediator = new BookmarkManagerMediator(mActivity, mBookmarkModel, mBookmarkOpener,
                    mSelectableListLayout, mSelectionDelegate, mRecyclerView,
                    mDragReorderableRecyclerViewAdapter, mLargeIconBridge, /*isDialogUi=*/true,
                    /*isIncognito=*/false, mBackPressStateSupplier, mProfile,
                    mBookmarkUndoController, mModelList, mBookmarkUiPrefs, mHideKeyboardRunnable,
                    mBookmarkImageFetcher);
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

    void verifyBookmarkListMenuItem(ListItem item, @StringRes int titleId, boolean enabled) {
        assertEquals(item.model.get(ListMenuItemProperties.TITLE_ID), titleId);
        assertEquals(item.model.get(ListMenuItemProperties.ENABLED), enabled);
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
    @Features.DisableFeatures({ChromeFeatureList.EMPTY_STATES})
    public void testEmptyView_Bookmark() {
        // Setup and open Bookmark folder.
        finishLoading();
        assertEquals(BookmarkUiMode.LOADING, mMediator.getCurrentUiMode());
        mMediator.openFolder(mFolderId1);

        // Verify empty view initialized.
        verify(mSelectableListLayout).setEmptyViewText(R.string.bookmarks_folder_empty);
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.EMPTY_STATES})
    public void testEmptyView_ReadingList() {
        // Setup and open Reading list folder.
        finishLoading();
        assertEquals(BookmarkUiMode.LOADING, mMediator.getCurrentUiMode());
        mMediator.openFolder(mReadingListFolderId);

        // Verify empty view initialized.
        verify(mSelectableListLayout).setEmptyViewText(R.string.reading_list_empty_list_title);
    }

    @Test
    public void testEmptyView_EmptyState_Bookmark() {
        // Setup and open Bookmark folder.
        finishLoading();
        assertEquals(BookmarkUiMode.LOADING, mMediator.getCurrentUiMode());
        mMediator.openFolder(mFolderId1);

        // Verify empty view initialized.
        verify(mSelectableListLayout)
                .setEmptyStateImageRes(R.drawable.bookmark_empty_state_illustration);
        verify(mSelectableListLayout)
                .setEmptyStateViewText(R.string.bookmark_manager_empty_state,
                        R.string.bookmark_manager_back_to_page_by_adding_bookmark);
    }

    @Test
    public void testEmptyView_EmptyState_ReadingList() {
        // Setup and open Reading list folder.
        finishLoading();
        assertEquals(BookmarkUiMode.LOADING, mMediator.getCurrentUiMode());
        mMediator.openFolder(mReadingListFolderId);

        // Verify empty view initialized.
        verify(mSelectableListLayout)
                .setEmptyStateImageRes(R.drawable.reading_list_empty_state_illustration);
        verify(mSelectableListLayout)
                .setEmptyStateViewText(R.string.reading_list_manager_empty_state,
                        R.string.reading_list_manager_save_page_to_read_later);
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
    public void onBackPressed_AndThenModelEvent() {
        initAndLoadBookmarkModel();
        assertFalse(mMediator.onBackPressed());

        verify(mBookmarkModel).addObserver(mBookmarkModelObserverArgumentCaptor.capture());
        mBookmarkModelObserverArgumentCaptor.getValue().bookmarkModelChanged();
        // This test is verifying the observer event doesn't crash.
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

        mMediator.openSearchUi();
        mMediator.search("3");
        assertEquals(1, mModelList.size());

        mMediator.onEndSearch();
        assertEquals(2, mModelList.size());
    }

    @Test
    public void testBookmarkRemoved() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        assertEquals(2, mModelList.size());

        doReturn(Arrays.asList(mFolderId3)).when(mBookmarkModel).getChildIds(mFolderId1);
        verify(mBookmarkModel).addObserver(mBookmarkModelObserverArgumentCaptor.capture());
        mBookmarkModelObserverArgumentCaptor.getValue().bookmarkNodeRemoved(
                mFolderItem1, 0, mFolderItem2, /*isDoingExtensiveChanges*/ false);
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
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void onPreferenceChanged_ViewPreferenceUpdated() {
        mMediator.openFolder(mFolderId1);
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.VISUAL);
        assertEquals(ViewType.IMPROVED_BOOKMARK_VISUAL, mModelList.get(1).type);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testBuildImprovedBookmarkRow() {
        finishLoading();
        mMediator.openFolder(mFolderId2);
        assertEquals(2, mModelList.size());

        ListItem item = mModelList.get(1);
        assertEquals(ViewType.IMPROVED_BOOKMARK_COMPACT, item.type);

        PropertyModel model = item.model;
        assertNotNull(model);
        assertEquals(mBookmarkItem21,
                model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY).getBookmarkItem());
        assertEquals(mBookmarkId21, model.get(BookmarkManagerProperties.BOOKMARK_ID));
        assertEquals(mBookmarkItem21.getTitle(), model.get(ImprovedBookmarkRowProperties.TITLE));
        assertEquals(
                "https://www.example.com/", model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        assertNull(model.get(ImprovedBookmarkRowProperties.START_ICON_TINT));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.POPUP_LISTENER));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.DRAG_ENABLED));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE));
        assertEquals(true, model.get(ImprovedBookmarkRowProperties.EDITABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.OPEN_BOOKMARK_CALLBACK));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testBuildImprovedBookmarkRow_ReadingList() {
        finishLoading();
        mMediator.openFolder(mReadingListFolderId);
        assertEquals(4, mModelList.size());

        ListItem item = mModelList.get(2);
        assertEquals(ViewType.IMPROVED_BOOKMARK_COMPACT, item.type);

        PropertyModel model = item.model;
        assertNotNull(model);
        assertEquals(mReadingListItem,
                model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY).getBookmarkItem());
        assertEquals(mReadingListId, model.get(BookmarkManagerProperties.BOOKMARK_ID));
        assertEquals(mReadingListItem.getTitle(), model.get(ImprovedBookmarkRowProperties.TITLE));
        assertEquals(
                "https://www.example.com/", model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testBuildImprovedBookmarkRow_Visual() {
        finishLoading();
        mMediator.openFolder(mFolderId2);
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.VISUAL);
        assertEquals(2, mModelList.size());

        ListItem item = mMediator.buildImprovedBookmarkRow(BookmarkListEntry.createBookmarkEntry(
                mBookmarkItem21, null, mBookmarkUiPrefs.getBookmarkRowDisplayPref()));
        assertEquals(ViewType.IMPROVED_BOOKMARK_VISUAL, item.type);

        PropertyModel model = item.model;
        assertNotNull(model);
        assertEquals(mBookmarkItem21,
                model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY).getBookmarkItem());
        assertEquals(mBookmarkId21, model.get(BookmarkManagerProperties.BOOKMARK_ID));
        assertEquals(mBookmarkItem21.getTitle(), model.get(ImprovedBookmarkRowProperties.TITLE));
        assertEquals(
                "https://www.example.com/", model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        assertNull(model.get(ImprovedBookmarkRowProperties.START_ICON_TINT));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.POPUP_LISTENER));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.DRAG_ENABLED));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE));
        assertEquals(true, model.get(ImprovedBookmarkRowProperties.EDITABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.OPEN_BOOKMARK_CALLBACK));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testBuildImprovedBookmarkRow_Shopping() {
        ShoppingSpecifics specifics = ShoppingSpecifics.newBuilder()
                                              .setCurrentPrice(ProductPrice.newBuilder()
                                                                       .setCurrencyCode("USD")
                                                                       .setAmountMicros(100)
                                                                       .build())
                                              .setPreviousPrice(ProductPrice.newBuilder()
                                                                        .setCurrencyCode("USD")
                                                                        .setAmountMicros(100)
                                                                        .build())
                                              .setIsPriceTracked(true)
                                              .build();
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder().setShoppingSpecifics(specifics).build();
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(mBookmarkId21);

        finishLoading();
        mMediator.openFolder(mFolderId2);
        assertEquals(2, mModelList.size());

        ListItem item = mModelList.get(1);
        assertEquals(ViewType.IMPROVED_BOOKMARK_COMPACT, item.type);

        PropertyModel model = item.model;
        assertNotNull(model);
        assertNotNull(model.get(ImprovedBookmarkRowProperties.ACCESSORY_VIEW));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testBuildImprovedBookmarkRow_Folder() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        assertEquals(3, mModelList.size());

        ListItem item = mModelList.get(1);
        assertEquals(ViewType.IMPROVED_BOOKMARK_COMPACT, item.type);

        PropertyModel model = item.model;
        assertNotNull(model);
        assertEquals(mFolderItem2.getId(),
                model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY).getBookmarkItem().getId());
        assertEquals(mFolderId2, model.get(BookmarkManagerProperties.BOOKMARK_ID));
        assertEquals(mFolderItem2.getTitle(), model.get(ImprovedBookmarkRowProperties.TITLE));
        assertEquals("1 bookmark", model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertEquals(StartImageVisibility.DRAWABLE,
                model.get(ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_TINT));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.POPUP_LISTENER));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.DRAG_ENABLED));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE));
        assertEquals(true, model.get(ImprovedBookmarkRowProperties.EDITABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.OPEN_BOOKMARK_CALLBACK));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testBuildImprovedBookmarkRow_Folder_Visual() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.VISUAL);
        assertEquals(3, mModelList.size());

        ListItem item = mModelList.get(1);
        assertEquals(ViewType.IMPROVED_BOOKMARK_VISUAL, item.type);

        PropertyModel model = item.model;
        assertNotNull(model);
        assertEquals(mFolderItem2.getId(),
                model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY).getBookmarkItem().getId());
        assertEquals(mFolderId2, model.get(BookmarkManagerProperties.BOOKMARK_ID));
        assertEquals(mFolderItem2.getTitle(), model.get(ImprovedBookmarkRowProperties.TITLE));
        assertEquals("1 bookmark", model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertEquals(StartImageVisibility.FOLDER_DRAWABLE,
                model.get(ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_TINT));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.POPUP_LISTENER));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.DRAG_ENABLED));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE));
        assertEquals(true, model.get(ImprovedBookmarkRowProperties.EDITABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.OPEN_BOOKMARK_CALLBACK));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testBuildImprovedBookmarkRow_readingListFolder() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        assertEquals(3, mModelList.size());

        ListItem item = mModelList.get(1);
        assertEquals(ViewType.IMPROVED_BOOKMARK_COMPACT, item.type);

        PropertyModel model = item.model;
        assertNotNull(model);
        assertEquals(mFolderItem2.getId(),
                model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY).getBookmarkItem().getId());
        assertEquals(mFolderId2, model.get(BookmarkManagerProperties.BOOKMARK_ID));
        assertEquals(mFolderItem2.getTitle(), model.get(ImprovedBookmarkRowProperties.TITLE));
        assertEquals("1 bookmark", model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_TINT));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.POPUP_LISTENER));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.DRAG_ENABLED));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE));
        assertEquals(true, model.get(ImprovedBookmarkRowProperties.EDITABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.OPEN_BOOKMARK_CALLBACK));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testBuildImprovedBookmarkRow_FolderVisual() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.VISUAL);
        assertEquals(3, mModelList.size());

        ListItem item = mModelList.get(1);
        assertEquals(ViewType.IMPROVED_BOOKMARK_VISUAL, item.type);

        PropertyModel model = item.model;
        assertNotNull(model);
        assertEquals(mFolderItem2,
                model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY).getBookmarkItem());
        assertEquals(mFolderId2, model.get(BookmarkManagerProperties.BOOKMARK_ID));
        assertEquals(mFolderItem2.getTitle(), model.get(ImprovedBookmarkRowProperties.TITLE));
        assertEquals("1 bookmark", model.get(ImprovedBookmarkRowProperties.DESCRIPTION));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_IMAGE_FOLDER_DRAWABLES));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.POPUP_LISTENER));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.DRAG_ENABLED));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE));
        assertEquals(true, model.get(ImprovedBookmarkRowProperties.EDITABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.OPEN_BOOKMARK_CALLBACK));
    }

    @Test
    public void testCreateListMenuModelList() {
        finishLoading();
        mMediator.openFolder(mFolderId2);

        ModelList modelList = mMediator.createListMenuModelList(mBookmarkId21, Location.MIDDLE);
        assertEquals(6, modelList.size());
        verifyBookmarkListMenuItem(modelList.get(0), R.string.bookmark_item_select, true);
        verifyBookmarkListMenuItem(modelList.get(1), R.string.bookmark_item_edit, true);
        verifyBookmarkListMenuItem(modelList.get(2), R.string.bookmark_item_move, true);
        verifyBookmarkListMenuItem(modelList.get(3), R.string.bookmark_item_delete, true);
        verifyBookmarkListMenuItem(modelList.get(4), R.string.menu_item_move_up, true);
        verifyBookmarkListMenuItem(modelList.get(5), R.string.menu_item_move_down, true);

        modelList = mMediator.createListMenuModelList(mBookmarkId21, Location.TOP);
        assertEquals(5, modelList.size());
        verifyBookmarkListMenuItem(modelList.get(4), R.string.menu_item_move_down, true);

        modelList = mMediator.createListMenuModelList(mBookmarkId21, Location.BOTTOM);
        assertEquals(5, modelList.size());
        verifyBookmarkListMenuItem(modelList.get(4), R.string.menu_item_move_up, true);

        mMediator.openFolder(mRootFolderId);
        modelList = mMediator.createListMenuModelList(mBookmarkId21, Location.MIDDLE);
        assertEquals("neither move option should be visible", 4, modelList.size());

        mMediator.openSearchUi();
        modelList = mMediator.createListMenuModelList(mBookmarkId21, Location.MIDDLE);
        assertEquals(5, modelList.size());
        verifyBookmarkListMenuItem(modelList.get(4), R.string.bookmark_show_in_folder, true);
    }

    @Test
    public void testCreateListMenuModelList_ReadingList() {
        finishLoading();
        mMediator.openFolder(mReadingListFolderId);

        ModelList modelList = mMediator.createListMenuModelList(mReadingListId, Location.MIDDLE);
        assertEquals(5, modelList.size());
        verifyBookmarkListMenuItem(modelList.get(0), R.string.reading_list_mark_as_read, true);
        verifyBookmarkListMenuItem(modelList.get(1), R.string.bookmark_item_select, true);
        verifyBookmarkListMenuItem(modelList.get(2), R.string.bookmark_item_edit, true);
        verifyBookmarkListMenuItem(modelList.get(3), R.string.bookmark_item_move, true);
        verifyBookmarkListMenuItem(modelList.get(4), R.string.bookmark_item_delete, true);
    }

    @Test
    public void testCreateListMenuForBookmark() {
        finishLoading();
        mMediator.openFolder(mFolderId2);

        // This is the first item mFolderId2.
        BasicListMenu menu =
                (BasicListMenu) mMediator.createListMenuForBookmark(mModelList.get(0).model);
        assertNotNull(menu);

        // select
        menu.onItemClick(null, null, 0, 0);
        verify(mSelectionDelegate).toggleSelectionForItem(mBookmarkId21);

        // edit
        // TODO(crbug.com/1444544): This doesn't actually open the activity yet.
        menu.onItemClick(null, null, 1, 0);

        // move
        // TODO(crbug.com/1444544): This doesn't actually open the activity yet.
        menu.onItemClick(null, null, 2, 0);

        // delete.
        menu.onItemClick(null, null, 3, 0);
        verify(mBookmarkModel).deleteBookmarks(mBookmarkId21);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testParentFolderUpdatedWhenChildDeleted() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.VISUAL);
        assertEquals(3, mModelList.size());
        assertEquals(
                1, mModelList.get(1).model.get(ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT));

        doReturn(0).when(mBookmarkModel).getTotalBookmarkCount(mFolderId2);
        verify(mBookmarkModel).addObserver(mBookmarkModelObserverArgumentCaptor.capture());
        mBookmarkModelObserverArgumentCaptor.getValue().bookmarkNodeRemoved(
                mFolderItem2, 0, mBookmarkItem21, false);

        assertEquals(
                0, mModelList.get(1).model.get(ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT));
    }
}
