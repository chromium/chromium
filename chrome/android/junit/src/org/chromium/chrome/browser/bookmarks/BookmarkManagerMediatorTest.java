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
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;
import static org.chromium.ui.test.util.MockitoHelper.doRunnable;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.util.Pair;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;
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
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
import org.chromium.chrome.browser.bookmarks.BookmarkUiState.BookmarkUiMode;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.ImageVisibility;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.commerce.PriceTrackingUtilsJni;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DragListener;
import org.chromium.components.browser_ui.widget.dragreorder.DragReorderableRecyclerViewAdapter.DraggabilityProvider;
import org.chromium.components.browser_ui.widget.dragreorder.DragStateDelegate;
import org.chromium.components.browser_ui.widget.listmenu.BasicListMenu;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuItemProperties;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.components.commerce.core.ShoppingService;
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
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncService.SyncStateChangedListener;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.function.Consumer;

/** Unit tests for {@link BookmarkManagerMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.BOOKMARKS_REFRESH, ChromeFeatureList.SHOPPING_LIST,
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
    @Mock
    private ShoppingService mShoppingService;
    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private PriceTrackingUtils.Natives mPriceTrackingUtilsJniMock;
    @Mock
    private ListObservable.ListObserver<Void> mListObserver;
    @Mock
    private Consumer<OnScrollListener> mOnScrollListenerConsumer;

    @Captor
    private ArgumentCaptor<BookmarkModelObserver> mBookmarkModelObserverArgumentCaptor;
    @Captor
    private ArgumentCaptor<SelectionObserver> mSelectionObserver;
    @Captor
    private ArgumentCaptor<DragListener> mDragListenerArgumentCaptor;
    @Captor
    private ArgumentCaptor<SyncStateChangedListener> mSyncStateChangedListenerCaptor;
    @Captor
    private ArgumentCaptor<Runnable> mFinishLoadingBookmarkModelCaptor;
    @Captor
    private ArgumentCaptor<OnScrollListener> mOnScrollListenerCaptor;

    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Boolean>
            mSelectableListLayoutHandleBackPressChangedSupplier = new ObservableSupplierImpl<>();
    private final BookmarkId mRootFolderId = new BookmarkId(/*id=*/1, BookmarkType.NORMAL);
    private final BookmarkId mDesktopFolderId = new BookmarkId(/*id=*/2, BookmarkType.NORMAL);
    private final BookmarkId mMobileFolderId = new BookmarkId(/*id=*/3, BookmarkType.NORMAL);
    private final BookmarkId mOtherFolderId = new BookmarkId(/*id=*/4, BookmarkType.NORMAL);
    private final BookmarkId mFolderId1 = new BookmarkId(/*id=*/5, BookmarkType.NORMAL);
    private final BookmarkId mFolderId2 = new BookmarkId(/*id=*/6, BookmarkType.NORMAL);
    private final BookmarkId mFolderId3 = new BookmarkId(/*id=*/7, BookmarkType.NORMAL);
    private final BookmarkId mBookmarkId21 = new BookmarkId(/*id=*/8, BookmarkType.NORMAL);
    private final BookmarkId mReadingListFolderId =
            new BookmarkId(/*id=*/9, BookmarkType.READING_LIST);
    private final BookmarkId mReadingListId = new BookmarkId(/*id=*/10, BookmarkType.READING_LIST);
    private final BookmarkId mPriceTrackedBookmarkId =
            new BookmarkId(/*id=*/11, BookmarkType.NORMAL);

    private final BookmarkItem mDesktopFolderItem = new BookmarkItem(
            mDesktopFolderId, "Bookmarks bar", null, true, mRootFolderId, true, false, 0, false, 0);
    private final BookmarkItem mMobileFolderItem = new BookmarkItem(mMobileFolderId,
            "Mobile bookmarks", null, true, mRootFolderId, true, false, 0, false, 0);
    private final BookmarkItem mOtherFolderItem = new BookmarkItem(
            mOtherFolderId, "Other bookmarks", null, true, mRootFolderId, true, false, 0, false, 0);
    private final BookmarkItem mFolderItem1 = new BookmarkItem(
            mFolderId1, "Folder1", null, true, mRootFolderId, true, false, 0, false, 0);
    private final BookmarkItem mFolderItem2 = new BookmarkItem(
            mFolderId2, "Folder2", null, true, mFolderId1, true, false, 0, false, 0);
    private final BookmarkItem mFolderItem3 = new BookmarkItem(
            mFolderId3, "Folder3", null, true, mFolderId1, true, false, 0, false, 0);
    private final BookmarkItem mBookmarkItem21 = new BookmarkItem(mBookmarkId21, "Bookmark21",
            JUnitTestGURLs.EXAMPLE_URL, false, mFolderId2, true, false, 0, false, 0);
    private final BookmarkItem mReadingListFolderItem = new BookmarkItem(mReadingListFolderId,
            "Reading List", null, true, mRootFolderId, false, false, 0, false, 0);
    private final BookmarkItem mReadingListItem = new BookmarkItem(mReadingListId,
            JUnitTestGURLs.EXAMPLE_URL.getSpec(), JUnitTestGURLs.EXAMPLE_URL, false,
            mReadingListFolderId, true, false, 0, false, 0);
    private final BookmarkItem mPriceTrackedBookmarkItem =
            new BookmarkItem(mPriceTrackedBookmarkId, "Price tracked bookmark",
                    JUnitTestGURLs.EXAMPLE_URL, false, mMobileFolderId, true, false, 0, false, 0);

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

            // Setup Profile.
            Profile.setLastUsedProfileForTesting(mProfile);

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
            doReturn(mDesktopFolderId).when(mBookmarkModel).getDesktopFolderId();
            doReturn(mDesktopFolderItem).when(mBookmarkModel).getBookmarkById(mDesktopFolderId);
            doReturn(mMobileFolderId).when(mBookmarkModel).getMobileFolderId();
            doReturn(mMobileFolderItem).when(mBookmarkModel).getBookmarkById(mMobileFolderId);
            doReturn(mPriceTrackedBookmarkItem)
                    .when(mBookmarkModel)
                    .getBookmarkById(mPriceTrackedBookmarkId);
            doReturn(Arrays.asList(mPriceTrackedBookmarkId))
                    .when(mBookmarkModel)
                    .getChildIds(mMobileFolderId);
            doReturn(mOtherFolderId).when(mBookmarkModel).getOtherFolderId();
            doReturn(mOtherFolderItem).when(mBookmarkModel).getBookmarkById(mOtherFolderId);
            doReturn(mReadingListFolderId).when(mBookmarkModel).getReadingListFolder();
            doReturn(mReadingListFolderItem)
                    .when(mBookmarkModel)
                    .getBookmarkById(mReadingListFolderId);
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
            doReturn(true).when(mBookmarkModel).isFolderVisible(any());
            doReturn(Arrays.asList(mReadingListFolderId))
                    .when(mBookmarkModel)
                    .getTopLevelFolderIds(/*getSpecial=*/true, /*getNormal=*/false);

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
            SyncServiceFactory.setInstanceForTesting(mSyncService);
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

            // Setup price tracking utils.
            mJniMocker.mock(PriceTrackingUtilsJni.TEST_HOOKS, mPriceTrackingUtilsJniMock);
            doCallback(3, (Callback<Boolean> callback) -> callback.onResult(true))
                    .when(mPriceTrackingUtilsJniMock)
                    .setPriceTrackingStateForBookmark(
                            any(), anyLong(), anyBoolean(), any(), anyBoolean());
            doCallback(0, (Callback<List<BookmarkId>> callback) -> {
                callback.onResult(Arrays.asList(mPriceTrackedBookmarkId));
            }).when(mShoppingService).getAllPriceTrackedBookmarks(any());
            ShoppingSpecifics trackedShoppingSpecifics =
                    ShoppingSpecifics.newBuilder().setProductClusterId(1).build();
            PowerBookmarkMeta shoppingMetaTracked =
                    PowerBookmarkMeta.newBuilder()
                            .setShoppingSpecifics(trackedShoppingSpecifics)
                            .build();
            doReturn(true)
                    .when(mShoppingService)
                    .isSubscribedFromCache(
                            PowerBookmarkUtils.createCommerceSubscriptionForShoppingSpecifics(
                                    trackedShoppingSpecifics));
            doReturn(shoppingMetaTracked)
                    .when(mBookmarkModel)
                    .getPowerBookmarkMeta(mPriceTrackedBookmarkId);
            ShoppingFeatures.setShoppingListEligibleForTesting(true);

            mDragReorderableRecyclerViewAdapter =
                    spy(new DragReorderableRecyclerViewAdapter(mActivity, mModelList));
            mMediator = new BookmarkManagerMediator(mActivity, mBookmarkModel, mBookmarkOpener,
                    mSelectableListLayout, mSelectionDelegate, mRecyclerView,
                    mDragReorderableRecyclerViewAdapter, mLargeIconBridge, /*isDialogUi=*/true,
                    /*isIncognito=*/false, mBackPressStateSupplier, mProfile,
                    mBookmarkUndoController, mModelList, mBookmarkUiPrefs, mHideKeyboardRunnable,
                    mBookmarkImageFetcher, mShoppingService, mSnackbarManager,
                    mOnScrollListenerConsumer);
            mMediator.addUiObserver(mBookmarkUiObserver);
        });
    }

    private void finishLoading() {
        when(mBookmarkModel.isBookmarkModelLoaded()).thenReturn(true);
        verify(mBookmarkModel, atLeast(0))
                .finishLoadingBookmarkModel(mFinishLoadingBookmarkModelCaptor.capture());
        for (Runnable finishLoadingBookmarkModel :
                mFinishLoadingBookmarkModelCaptor.getAllValues()) {
            finishLoadingBookmarkModel.run();
        }
    }

    private void verifyBookmarkListMenuItem(
            ListItem item, @StringRes int titleId, boolean enabled) {
        assertEquals(item.model.get(ListMenuItemProperties.TITLE_ID), titleId);
        assertEquals(item.model.get(ListMenuItemProperties.ENABLED), enabled);
    }

    private void verifyCurrentViewTypes(int... expectedViewTypes) {
        verifyModelListHaViewTypes(mModelList, expectedViewTypes);
    }

    private static void verifyModelListHaViewTypes(ModelList modelList, int... expectedViewTypes) {
        assertEquals(expectedViewTypes.length, modelList.size());
        for (int i = 0; i < expectedViewTypes.length; ++i) {
            assertEquals("ViewType did not match at index " + i, expectedViewTypes[i],
                    modelList.get(i).type);
        }
    }

    private void verifyCurrentBookmarkIds(BookmarkId... expectedBookmarkIds) {
        verifyModelListHasBookmarkIds(mModelList, expectedBookmarkIds);
    }

    private static void verifyModelListHasBookmarkIds(
            ModelList modelList, BookmarkId... expectedBookmarkIds) {
        assertEquals(expectedBookmarkIds.length, modelList.size());
        for (int i = 0; i < expectedBookmarkIds.length; ++i) {
            BookmarkId bookmarkId = getBookmarkIdFromModel(modelList.get(i).model);
            assertEquals(
                    "BookmarkId did not match at index " + i, expectedBookmarkIds[i], bookmarkId);
        }
    }

    private static @Nullable BookmarkId getBookmarkIdFromModel(PropertyModel propertyModel) {
        BookmarkListEntry bookmarkListEntry =
                propertyModel.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY);
        if (bookmarkListEntry == null) {
            return null;
        }
        BookmarkItem bookmarkItem = bookmarkListEntry.getBookmarkItem();
        if (bookmarkItem == null) {
            return null;
        }
        return bookmarkItem.getId();
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
    @DisableFeatures({ChromeFeatureList.EMPTY_STATES})
    public void testEmptyView_Bookmark() {
        // Setup and open Bookmark folder.
        finishLoading();
        assertEquals(BookmarkUiMode.LOADING, mMediator.getCurrentUiMode());
        mMediator.openFolder(mFolderId1);

        // Verify empty view initialized.
        verify(mSelectableListLayout).setEmptyViewText(R.string.bookmarks_folder_empty);
    }

    @Test
    @DisableFeatures({ChromeFeatureList.EMPTY_STATES})
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
        verify(mBookmarkImageFetcher).destroy();
        verify(mLargeIconBridge).destroy();
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
    @DisableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
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
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testDrag_improvedBookmarks() {
        mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.MANUAL);

        finishLoading();
        mMediator.openFolder(mFolderId1);
        DraggabilityProvider draggabilityProvider = mMediator.getDraggabilityProvider();
        assertTrue(draggabilityProvider.isPassivelyDraggable(mModelList.get(1).model));
        assertTrue(draggabilityProvider.isActivelyDraggable(mModelList.get(1).model));
        DragStateDelegate dragStateDelegate = mMediator.getDragStateDelegate();
        assertTrue(dragStateDelegate.getDragEnabled());

        when(mSelectionDelegate.isItemSelected(mFolderId2)).thenReturn(true);
        assertTrue(draggabilityProvider.isActivelyDraggable(mModelList.get(1).model));

        mModelList.move(1, 2);
        verify(mDragReorderableRecyclerViewAdapter)
                .addDragListener(mDragListenerArgumentCaptor.capture());
        mDragListenerArgumentCaptor.getValue().onSwap();
        verify(mBookmarkModel)
                .reorderBookmarks(mFolderId1, new long[] {mFolderId3.getId(), mFolderId2.getId()});

        mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.CHRONOLOGICAL);
        assertFalse(dragStateDelegate.getDragEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testDrag_improvedBookmarks_whileFiltering() {
        mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.MANUAL);

        finishLoading();
        mMediator.openFolder(mMobileFolderId);
        DraggabilityProvider draggabilityProvider = mMediator.getDraggabilityProvider();
        assertTrue(draggabilityProvider.isPassivelyDraggable(mModelList.get(1).model));
        assertTrue(draggabilityProvider.isActivelyDraggable(mModelList.get(1).model));
        DragStateDelegate dragStateDelegate = mMediator.getDragStateDelegate();
        assertTrue(dragStateDelegate.getDragEnabled());

        // When a filter is selected, dragging should be disabled.
        PropertyModel model = mModelList.get(0).model;
        assertTrue(model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY));
        model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK).onResult(true);
        assertTrue(draggabilityProvider.isPassivelyDraggable(mModelList.get(1).model));
        assertTrue(draggabilityProvider.isActivelyDraggable(mModelList.get(1).model));
        assertFalse(dragStateDelegate.getDragEnabled());
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
        mMediator.onBookmarkModelLoaded();
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
        assertNotNull(model.get(ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER));
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
        assertNotNull(model.get(ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER));
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
        assertEquals(
                mFolderItem2.getTitle() + " (1)", model.get(ImprovedBookmarkRowProperties.TITLE));
        assertFalse(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        assertEquals(ImageVisibility.DRAWABLE,
                model.get(ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_TINT));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.POPUP_LISTENER));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.DRAG_ENABLED));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE));
        assertEquals(true, model.get(ImprovedBookmarkRowProperties.EDITABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER));
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
        assertFalse(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        assertEquals(ImageVisibility.FOLDER_DRAWABLE,
                model.get(ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_TINT));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.POPUP_LISTENER));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.DRAG_ENABLED));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE));
        assertEquals(true, model.get(ImprovedBookmarkRowProperties.EDITABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER));
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
        assertEquals(
                mFolderItem2.getTitle() + " (1)", model.get(ImprovedBookmarkRowProperties.TITLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_TINT));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.POPUP_LISTENER));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.DRAG_ENABLED));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE));
        assertEquals(true, model.get(ImprovedBookmarkRowProperties.EDITABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER));
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
        assertFalse(model.get(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.POPUP_LISTENER));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        assertEquals(false, model.get(ImprovedBookmarkRowProperties.DRAG_ENABLED));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE));
        assertEquals(true, model.get(ImprovedBookmarkRowProperties.EDITABLE));
        assertNotNull(model.get(ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER));
    }

    @Test
    public void testCreateListMenuModelList() {
        finishLoading();
        mMediator.openFolder(mFolderId2);

        BookmarkListEntry entry = BookmarkListEntry.createBookmarkEntry(
                mBookmarkItem21, null, BookmarkRowDisplayPref.COMPACT);
        ModelList modelList = mMediator.createListMenuModelList(entry, Location.MIDDLE);
        assertEquals(6, modelList.size());
        verifyBookmarkListMenuItem(modelList.get(0), R.string.bookmark_item_select, true);
        verifyBookmarkListMenuItem(modelList.get(1), R.string.bookmark_item_edit, true);
        verifyBookmarkListMenuItem(modelList.get(2), R.string.bookmark_item_move, true);
        verifyBookmarkListMenuItem(modelList.get(3), R.string.bookmark_item_delete, true);

        mMediator.openSearchUi();
        modelList = mMediator.createListMenuModelList(entry, Location.MIDDLE);
        assertEquals(5, modelList.size());
        verifyBookmarkListMenuItem(modelList.get(4), R.string.bookmark_show_in_folder, true);
    }

    @Test
    public void testCreateListMenuModelList_ReadingList() {
        finishLoading();
        mMediator.openFolder(mReadingListFolderId);

        BookmarkListEntry entry = BookmarkListEntry.createBookmarkEntry(
                mReadingListItem, null, BookmarkRowDisplayPref.COMPACT);
        ModelList modelList = mMediator.createListMenuModelList(entry, Location.MIDDLE);
        assertEquals(5, modelList.size());
        verifyBookmarkListMenuItem(modelList.get(0), R.string.reading_list_mark_as_read, true);
        verifyBookmarkListMenuItem(modelList.get(1), R.string.bookmark_item_select, true);
        verifyBookmarkListMenuItem(modelList.get(2), R.string.bookmark_item_edit, true);
        verifyBookmarkListMenuItem(modelList.get(3), R.string.bookmark_item_move, true);
        verifyBookmarkListMenuItem(modelList.get(4), R.string.bookmark_item_delete, true);
    }

    @Test
    public void testcreateListMenuModelList_shopping() {
        finishLoading();
        mMediator.openFolder(mFolderId2);

        doReturn(true).when(mShoppingService).isSubscribedFromCache(any());
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(ShoppingSpecifics.newBuilder()
                                                      .setProductClusterId(123)
                                                      .setOfferId(456)
                                                      .setCountryCode("us")
                                                      .setCurrentPrice(ProductPrice.newBuilder()
                                                                               .setAmountMicros(100)
                                                                               .build())
                                                      .build())
                        .build();
        BookmarkListEntry entry = BookmarkListEntry.createBookmarkEntry(
                mBookmarkItem21, meta, BookmarkRowDisplayPref.COMPACT);
        ModelList modelList = mMediator.createListMenuModelList(entry, Location.MIDDLE);
        assertEquals(7, modelList.size());
        verifyBookmarkListMenuItem(
                modelList.get(6), R.string.disable_price_tracking_menu_item, true);

        doReturn(false).when(mShoppingService).isSubscribedFromCache(any());
        modelList = mMediator.createListMenuModelList(entry, Location.MIDDLE);
        assertEquals(7, modelList.size());
        verifyBookmarkListMenuItem(
                modelList.get(6), R.string.enable_price_tracking_menu_item, true);
    }

    @Test
    public void testcreateListMenuModelList_shopping_notEligible() {
        ShoppingFeatures.setShoppingListEligibleForTesting(false);

        finishLoading();
        mMediator.openFolder(mFolderId2);

        doReturn(true).when(mShoppingService).isSubscribedFromCache(any());
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(ShoppingSpecifics.newBuilder()
                                                      .setProductClusterId(123)
                                                      .setOfferId(456)
                                                      .setCountryCode("us")
                                                      .setCurrentPrice(ProductPrice.newBuilder()
                                                                               .setAmountMicros(100)
                                                                               .build())
                                                      .build())
                        .build();
        BookmarkListEntry entry = BookmarkListEntry.createBookmarkEntry(
                mBookmarkItem21, meta, BookmarkRowDisplayPref.COMPACT);
        ModelList modelList = mMediator.createListMenuModelList(entry, Location.MIDDLE);
        // The 7th item would be the enable/disable price tracking.
        assertEquals(6, modelList.size());
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
    public void testCreateListMenuForBookmark_priceTracking() {
        finishLoading();

        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(ShoppingSpecifics.newBuilder()
                                                      .setProductClusterId(123)
                                                      .setOfferId(456)
                                                      .setCountryCode("us")
                                                      .setCurrentPrice(ProductPrice.newBuilder()
                                                                               .setAmountMicros(100)
                                                                               .build())
                                                      .build())
                        .build();
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(mBookmarkId21);
        mMediator.openFolder(mFolderId2);

        doReturn(true).when(mShoppingService).isSubscribedFromCache(any());
        BasicListMenu menu =
                (BasicListMenu) mMediator.createListMenuForBookmark(mModelList.get(1).model);
        assertNotNull(menu);

        // delete.
        menu.onItemClick(null, null, 4, 0);
        verify(mPriceTrackingUtilsJniMock)
                .setPriceTrackingStateForBookmark(
                        any(), anyLong(), anyBoolean(), any(), anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testParentFolderUpdatedWhenChildDeleted() {
        finishLoading();

        doReturn(1).when(mBookmarkModel).getTotalBookmarkCount(mFolderId2);
        doReturn(2).when(mBookmarkModel).getTotalBookmarkCount(mFolderId3);
        mMediator.openFolder(mFolderId1);
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.VISUAL);

        assertEquals(3, mModelList.size());
        PropertyModel coordinatorModel =
                mModelList.get(1)
                        .model.get(ImprovedBookmarkRowProperties.FOLDER_COORDINATOR)
                        .getModelForTesting();
        assertEquals(
                1, coordinatorModel.get(ImprovedBookmarkFolderViewProperties.FOLDER_CHILD_COUNT));

        doReturn(0).when(mBookmarkModel).getTotalBookmarkCount(mFolderId2);
        verify(mBookmarkModel).addObserver(mBookmarkModelObserverArgumentCaptor.capture());
        mBookmarkModelObserverArgumentCaptor.getValue().bookmarkNodeRemoved(
                mFolderItem2, 0, mBookmarkItem21, false);
        coordinatorModel = mModelList.get(1)
                                   .model.get(ImprovedBookmarkRowProperties.FOLDER_COORDINATOR)
                                   .getModelForTesting();
        assertEquals(
                0, coordinatorModel.get(ImprovedBookmarkFolderViewProperties.FOLDER_CHILD_COUNT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testChildRemoved() {
        finishLoading();

        mMediator.openFolder(mFolderId1);
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.VISUAL);

        assertEquals(3, mModelList.size());

        reset(mBookmarkUiObserver);
        verify(mBookmarkModel).addObserver(mBookmarkModelObserverArgumentCaptor.capture());
        mBookmarkModelObserverArgumentCaptor.getValue().bookmarkNodeRemoved(
                mFolderItem2, 0, mFolderItem2, false);
        assertEquals(2, mModelList.size());
        verify(mBookmarkUiObserver, times(0)).onUiModeChanged(BookmarkUiMode.FOLDER);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testChildRemoved_indexNotFound() {
        finishLoading();

        mMediator.openFolder(mFolderId1);
        mBookmarkUiPrefs.setBookmarkRowDisplayPref(BookmarkRowDisplayPref.VISUAL);

        assertEquals(3, mModelList.size());

        reset(mBookmarkUiObserver);
        verify(mBookmarkModel).addObserver(mBookmarkModelObserverArgumentCaptor.capture());
        mBookmarkModelObserverArgumentCaptor.getValue().bookmarkNodeRemoved(
                mFolderItem2, 0, mBookmarkItem21, false);
        assertEquals(3, mModelList.size());
        verify(mBookmarkUiObserver, times(1)).onUiModeChanged(BookmarkUiMode.FOLDER);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testPromoHeader() {
        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.PROMO_FOR_SIGNED_IN_STATE);
        mMediator.getPromoHeaderManager().syncStateChanged();
        finishLoading();
        mMediator.openFolder(mFolderId1);

        verifyCurrentViewTypes(ViewType.SEARCH_BOX, ViewType.PERSONALIZED_SYNC_PROMO,
                ViewType.IMPROVED_BOOKMARK_COMPACT, ViewType.IMPROVED_BOOKMARK_COMPACT);

        BookmarkPromoHeader.forcePromoStateForTesting(SyncPromoState.NO_PROMO);
        mMediator.getPromoHeaderManager().syncStateChanged();

        verifyCurrentViewTypes(ViewType.SEARCH_BOX, ViewType.IMPROVED_BOOKMARK_COMPACT,
                ViewType.IMPROVED_BOOKMARK_COMPACT);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testSearchBox() {
        when(mBookmarkModel.searchBookmarks(eq("3"), anyInt()))
                .thenReturn(Collections.singletonList(mFolderId3));
        finishLoading();
        mMediator.openFolder(mFolderId1);
        verifyCurrentViewTypes(ViewType.SEARCH_BOX, ViewType.IMPROVED_BOOKMARK_COMPACT,
                ViewType.IMPROVED_BOOKMARK_COMPACT);

        mModelList.addObserver(mListObserver);
        mModelList.get(0)
                .model.get(BookmarkSearchBoxRowProperties.SEARCH_TEXT_CHANGE_CALLBACK)
                .onResult("3");
        verifyCurrentViewTypes(ViewType.SEARCH_BOX, ViewType.IMPROVED_BOOKMARK_COMPACT);
        verify(mListObserver, never()).onItemRangeChanged(any(), eq(0), anyInt(), any());
        verify(mListObserver, never()).onItemRangeRemoved(any(), eq(0), anyInt());
        verify(mListObserver, never()).onItemRangeInserted(any(), eq(0), anyInt());
        verify(mListObserver).onItemRangeChanged(any(), eq(1), anyInt(), any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testDeleteDuringSelection() {
        // Inspired by https://crbug.com/1449447 where the search row didn't have a property and
        // we crashed when trying to handle deletion during selection.

        finishLoading();
        mMediator.openFolder(mFolderId1);
        assertEquals(3, mModelList.size());

        // Setup selection mock to make it seem like folder 2 is selected.
        when(mSelectionDelegate.isItemSelected(mFolderId2)).thenReturn(true);
        doReturn(Arrays.asList(mFolderId2)).when(mSelectionDelegate).getSelectedItemsAsList();

        // Pretend to delete folder 2.
        doReturn(Arrays.asList(mFolderId3)).when(mBookmarkModel).getChildIds(mFolderId1);
        verify(mBookmarkModel).addObserver(mBookmarkModelObserverArgumentCaptor.capture());
        mBookmarkModelObserverArgumentCaptor.getValue().bookmarkNodeRemoved(
                mFolderItem1, 0, mFolderItem2, /*isDoingExtensiveChanges*/ false);

        // Mostly just verifying that #syncAdapterAndSelectionDelegate() doesn't crash.
        assertEquals(2, mModelList.size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testSearchTextChangeCallback() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        reset(mHideKeyboardRunnable);
        assertEquals(3, mModelList.size());
        assertEquals(ViewType.SEARCH_BOX, mModelList.get(0).type);

        PropertyModel searchBoxModel = mModelList.get(0).model;
        Callback<String> searchTextChangeCallback =
                searchBoxModel.get(BookmarkSearchBoxRowProperties.SEARCH_TEXT_CHANGE_CALLBACK);
        assertNotNull(searchTextChangeCallback);

        String searchText = "foo";
        searchTextChangeCallback.onResult(searchText);
        assertEquals(BookmarkUiMode.SEARCHING, mMediator.getCurrentUiMode());
        assertEquals(searchText, searchBoxModel.get(BookmarkSearchBoxRowProperties.SEARCH_TEXT));
        verify(mBookmarkModel).searchBookmarks(eq(searchText), anyInt());
        assertTrue(searchBoxModel.get(
                BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_BUTTON_VISIBILITY));
        verifyNoInteractions(mHideKeyboardRunnable);

        searchText = "";
        searchTextChangeCallback.onResult(searchText);
        assertEquals(BookmarkUiMode.SEARCHING, mMediator.getCurrentUiMode());
        assertEquals(searchText, searchBoxModel.get(BookmarkSearchBoxRowProperties.SEARCH_TEXT));
        verify(mBookmarkModel).searchBookmarks(eq(searchText), anyInt());
        assertFalse(searchBoxModel.get(
                BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_BUTTON_VISIBILITY));
        verifyNoInteractions(mHideKeyboardRunnable);

        searchTextChangeCallback.onResult("bar");
        mMediator.onBackPressed();
        assertEquals(BookmarkUiMode.FOLDER, mMediator.getCurrentUiMode());
        assertEquals("", searchBoxModel.get(BookmarkSearchBoxRowProperties.SEARCH_TEXT));
        assertFalse(searchBoxModel.get(
                BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_BUTTON_VISIBILITY));
        verify(mHideKeyboardRunnable).run();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testImprovedSpecialFolders() {
        mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.ALPHABETICAL);
        final @ColorInt int specialBackgroundColor =
                SemanticColorUtils.getColorPrimaryContainer(mActivity);
        final @ColorInt int normalBackgroundColor =
                ChromeColors.getSurfaceColor(mActivity, R.dimen.default_elevation_1);
        finishLoading();

        mMediator.openFolder(mRootFolderId);

        assertEquals(5, mModelList.size());
        verifyCurrentBookmarkIds(
                null, mDesktopFolderId, mMobileFolderId, mOtherFolderId, mReadingListFolderId);
        assertEquals(specialBackgroundColor,
                mModelList.get(1).model.get(
                        ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        assertEquals(specialBackgroundColor,
                mModelList.get(2).model.get(
                        ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        assertEquals(specialBackgroundColor,
                mModelList.get(3).model.get(
                        ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        assertEquals(specialBackgroundColor,
                mModelList.get(4).model.get(
                        ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));

        mMediator.openFolder(mFolderId1);

        assertEquals(3, mModelList.size());
        verifyCurrentBookmarkIds(null, mFolderId2, mFolderId3);
        assertEquals(normalBackgroundColor,
                mModelList.get(1).model.get(
                        ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
        assertEquals(normalBackgroundColor,
                mModelList.get(2).model.get(
                        ImprovedBookmarkRowProperties.START_AREA_BACKGROUND_COLOR));
    }

    // Tests directly related to a regression.

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testShowInFolder() { // https://crbug.com/1456275
        when(mBookmarkModel.searchBookmarks(eq("test"), anyInt()))
                .thenReturn(Collections.singletonList(mBookmarkId21));
        finishLoading();

        mMediator.openFolder(mFolderId1);
        mMediator.openSearchUi();
        mMediator.search("test");
        BasicListMenu menu =
                (BasicListMenu) mMediator.createListMenuForBookmark(mModelList.get(1).model);
        assertNotNull(menu);
        assertFalse(mModelList.get(1).model.get(BookmarkManagerProperties.IS_HIGHLIGHTED));

        // Show in folder.
        menu.onItemClick(null, null, 4, 0);
        assertTrue(mModelList.get(1).model.get(BookmarkManagerProperties.IS_HIGHLIGHTED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testChangeSortOrderDuringSearch() { // https://crbug.com/1464965
        mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.ALPHABETICAL);
        when(mBookmarkModel.searchBookmarks(eq("test"), anyInt()))
                .thenReturn(Arrays.asList(mFolderId1, mFolderId2));
        finishLoading();
        mMediator.openFolder(mFolderId1);

        mMediator.search("test");
        verifyCurrentBookmarkIds(null, mFolderId1, mFolderId2);

        mBookmarkUiPrefs.setBookmarkRowSortOrder(BookmarkRowSortOrder.REVERSE_ALPHABETICAL);
        verifyCurrentBookmarkIds(null, mFolderId2, mFolderId1);
    }

    @Test
    public void testSelectionMoved_dropSelection() {
        finishLoading();
        mMediator.openFolder(mFolderId1);

        doReturn(true).when(mSelectionDelegate).isItemSelected(mFolderId2);
        mModelList.removeAt(mMediator.getPositionForBookmark(mFolderId2));

        verify(mBookmarkModel).addObserver(mBookmarkModelObserverArgumentCaptor.capture());
        mBookmarkModelObserverArgumentCaptor.getValue().bookmarkNodeChanged(mFolderItem2);

        verify(mSelectionDelegate).toggleSelectionForItem(mFolderId2);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testSelection_multiple() {
        finishLoading();

        mMediator.openFolder(mFolderId1);
        mMediator.toggleSelectionForRow(mFolderId2);

        verify(mSelectionDelegate).addObserver(mSelectionObserver.capture());
        mSelectionObserver.getValue().onSelectionStateChange(Arrays.asList(mFolderId2));
        doReturn(true).when(mSelectionDelegate).isSelectionEnabled();

        mMediator.bookmarkRowClicked(mFolderId3);

        verify(mSelectionDelegate).toggleSelectionForItem(mFolderId2);
        verify(mSelectionDelegate).toggleSelectionForItem(mFolderId3);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testChangeSelectionMode() {
        finishLoading();

        mMediator.openFolder(mFolderId1);
        mMediator.changeSelectionMode(true);

        assertTrue(mModelList.get(1).model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
        assertTrue(mModelList.get(2).model.get(ImprovedBookmarkRowProperties.SELECTION_ACTIVE));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testClearFocusOnScroll() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        reset(mHideKeyboardRunnable);

        assertEquals(ViewType.SEARCH_BOX, mModelList.get(0).type);
        verify(mOnScrollListenerConsumer).accept(mOnScrollListenerCaptor.capture());
        OnScrollListener onScrollListener = mOnScrollListenerCaptor.getValue();

        PropertyModel searchBoxRowPropertyModel = mModelList.get(0).model;
        searchBoxRowPropertyModel.get(BookmarkSearchBoxRowProperties.FOCUS_CHANGE_CALLBACK)
                .onResult(true);
        assertTrue(searchBoxRowPropertyModel.get(BookmarkSearchBoxRowProperties.HAS_FOCUS));

        onScrollListener.onScrolled(mRecyclerView, 0, -1);
        verifyNoInteractions(mHideKeyboardRunnable);
        assertTrue(searchBoxRowPropertyModel.get(BookmarkSearchBoxRowProperties.HAS_FOCUS));

        onScrollListener.onScrolled(mRecyclerView, 0, 1);
        assertFalse(searchBoxRowPropertyModel.get(BookmarkSearchBoxRowProperties.HAS_FOCUS));
        verify(mHideKeyboardRunnable).run();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testHideKeyboardOnLostSearchFocus() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        reset(mHideKeyboardRunnable);

        assertEquals(ViewType.SEARCH_BOX, mModelList.get(0).type);
        PropertyModel searchBoxRowPropertyModel = mModelList.get(0).model;

        searchBoxRowPropertyModel.get(BookmarkSearchBoxRowProperties.FOCUS_CHANGE_CALLBACK)
                .onResult(true);
        assertTrue(searchBoxRowPropertyModel.get(BookmarkSearchBoxRowProperties.HAS_FOCUS));
        verifyNoInteractions(mHideKeyboardRunnable);

        searchBoxRowPropertyModel.get(BookmarkSearchBoxRowProperties.FOCUS_CHANGE_CALLBACK)
                .onResult(false);
        assertFalse(searchBoxRowPropertyModel.get(BookmarkSearchBoxRowProperties.HAS_FOCUS));
        verify(mHideKeyboardRunnable).run();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testChangeSelectionIndexWhenNoBookmarksPresent() {
        finishLoading();

        mMediator.openFolder(mFolderId3);
        mMediator.changeSelectionMode(true);

        assertEquals(1, mModelList.size());
        assertEquals(ViewType.SEARCH_BOX, mModelList.get(0).type);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testClearSearchTextRunnable() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        assertEquals(ViewType.SEARCH_BOX, mModelList.get(0).type);

        PropertyModel propertyModel = mModelList.get(0).model;
        Callback<String> searchTextCallback =
                propertyModel.get(BookmarkSearchBoxRowProperties.SEARCH_TEXT_CHANGE_CALLBACK);
        assertNotNull(searchTextCallback);
        Runnable clearSearchTextRunnable =
                propertyModel.get(BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_RUNNABLE);
        assertNotNull(clearSearchTextRunnable);

        String searchText = "foo";
        searchTextCallback.onResult(searchText);
        assertEquals(searchText, propertyModel.get(BookmarkSearchBoxRowProperties.SEARCH_TEXT));
        assertTrue(propertyModel.get(
                BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_BUTTON_VISIBILITY));

        clearSearchTextRunnable.run();
        assertEquals("", propertyModel.get(BookmarkSearchBoxRowProperties.SEARCH_TEXT));
        assertFalse(propertyModel.get(
                BookmarkSearchBoxRowProperties.CLEAR_SEARCH_TEXT_BUTTON_VISIBILITY));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testBackNavigationDoesNotRestoreSearch() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        assertEquals(BookmarkUiMode.FOLDER, mMediator.getCurrentUiMode());
        verifyCurrentBookmarkIds(null, mFolderId2, mFolderId3);

        Callback<String> searchTextChangeCallback = mModelList.get(0).model.get(
                BookmarkSearchBoxRowProperties.SEARCH_TEXT_CHANGE_CALLBACK);
        searchTextChangeCallback.onResult("foo");
        assertEquals(BookmarkUiMode.SEARCHING, mMediator.getCurrentUiMode());

        mMediator.openFolder(mFolderId2);
        assertEquals(BookmarkUiMode.FOLDER, mMediator.getCurrentUiMode());
        verifyCurrentBookmarkIds(null, mBookmarkId21);

        assertTrue(mMediator.onBackPressed());
        // Should have gone back to mFolderId1.
        assertEquals(BookmarkUiMode.FOLDER, mMediator.getCurrentUiMode());
        verifyCurrentBookmarkIds(null, mFolderId2, mFolderId3);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testSearchBox_priceTrackingFilterVisible() {
        finishLoading();

        mMediator.openFolder(mFolderId3);

        assertEquals(1, mModelList.size());
        assertEquals(ViewType.SEARCH_BOX, mModelList.get(0).type);

        PropertyModel model = mModelList.get(0).model;
        assertTrue(model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testSearchBox_priceTrackingFilterClicked() {
        finishLoading();

        mMediator.openFolder(mMobileFolderId);

        assertEquals(2, mModelList.size());
        assertEquals(ViewType.SEARCH_BOX, mModelList.get(0).type);

        PropertyModel model = mModelList.get(0).model;
        assertTrue(model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY));
        model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK).onResult(true);

        // The price-tracked bookmark item should still be there.
        assertEquals(2, mModelList.size());

        model = mModelList.get(0).model;
        model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK).onResult(false);

        // Going back should still show the one bookmark.
        assertEquals(2, mModelList.size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testSearchBox_priceTrackingFilterClicked_noResults() {
        finishLoading();

        mMediator.openFolder(mFolderId1);

        assertEquals(3, mModelList.size());
        assertEquals(ViewType.SEARCH_BOX, mModelList.get(0).type);

        PropertyModel model = mModelList.get(0).model;
        assertTrue(model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY));
        model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_TOGGLE_CALLBACK).onResult(true);

        assertEquals(1, mModelList.size());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testSearchBox_priceTrackingFilterGoneInReadingList() {
        finishLoading();

        mMediator.openFolder(mReadingListFolderId);

        assertEquals(4, mModelList.size());
        assertEquals(ViewType.SEARCH_BOX, mModelList.get(0).type);

        PropertyModel model = mModelList.get(0).model;
        assertFalse(model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testSearchBox_priceTrackingFilterGoneWithoutBookmarks() {
        finishLoading();

        mMediator.openFolder(mFolderId3);

        assertEquals(1, mModelList.size());
        assertEquals(ViewType.SEARCH_BOX, mModelList.get(0).type);

        PropertyModel model = mModelList.get(0).model;
        assertTrue(model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY));

        // The price-tracked filter shouldn't be visible without any price-tracked bookmarks.
        doCallback(0, (Callback<List<BookmarkId>> callback) -> {
            callback.onResult(Arrays.asList());
        }).when(mShoppingService).getAllPriceTrackedBookmarks(any());

        mMediator.updateShoppingFilterVisible();
        assertEquals(1, mModelList.size());
        assertEquals(ViewType.SEARCH_BOX, mModelList.get(0).type);

        model = mModelList.get(0).model;
        assertFalse(model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testSearchBox_priceTrackingFilterGoneWithEligibility() {
        ShoppingFeatures.setShoppingListEligibleForTesting(false);
        finishLoading();

        mMediator.openFolder(mFolderId3);

        assertEquals(1, mModelList.size());
        assertEquals(ViewType.SEARCH_BOX, mModelList.get(0).type);

        PropertyModel model = mModelList.get(0).model;
        assertFalse(model.get(BookmarkSearchBoxRowProperties.SHOPPING_CHIP_VISIBILITY));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testSearchWithWhitespace() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        assertEquals(ViewType.SEARCH_BOX, mModelList.get(0).type);

        PropertyModel propertyModel = mModelList.get(0).model;
        Callback<String> searchTextCallback =
                propertyModel.get(BookmarkSearchBoxRowProperties.SEARCH_TEXT_CHANGE_CALLBACK);
        assertNotNull(searchTextCallback);

        String queryWithWhitespace = " foo ";
        searchTextCallback.onResult(queryWithWhitespace);
        // Model queries should be trimmed, but the View property should still have whitespace.
        assertEquals(
                queryWithWhitespace, propertyModel.get(BookmarkSearchBoxRowProperties.SEARCH_TEXT));
        verify(mBookmarkModel).searchBookmarks(eq("foo"), anyInt());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testModelChangeDuringSearch() {
        finishLoading();
        mMediator.openFolder(mFolderId1);
        PropertyModel searchBoxModel = mModelList.get(0).model;

        when(mBookmarkModel.searchBookmarks(anyString(), anyInt()))
                .thenReturn(Collections.singletonList(mFolderId1));
        searchBoxModel.get(BookmarkSearchBoxRowProperties.FOCUS_CHANGE_CALLBACK).onResult(true);

        assertEquals(BookmarkUiMode.SEARCHING, mMediator.getCurrentUiMode());
        verifyCurrentBookmarkIds(null, mFolderId1);

        when(mBookmarkModel.searchBookmarks(anyString(), anyInt()))
                .thenReturn(Collections.singletonList(mFolderId2));
        verify(mBookmarkModel).addObserver(mBookmarkModelObserverArgumentCaptor.capture());
        mBookmarkModelObserverArgumentCaptor.getValue().bookmarkModelChanged();

        // Should still be in search mode, and should have refreshed and picked up new results.
        assertEquals(BookmarkUiMode.SEARCHING, mMediator.getCurrentUiMode());
        verifyCurrentBookmarkIds(null, mFolderId2);
    }
}
