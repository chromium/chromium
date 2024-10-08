// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.DESKTOP_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.MOBILE_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.OTHER_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.PARTNER_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.READING_LIST_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.ROOT_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_B;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_C;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_D;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_E;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_F;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_G;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_H;

import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowSortOrder;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.PowerBookmarkType;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Unit tests for {@link ImprovedBookmarkQueryHandler}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImprovedBookmarkQueryHandlerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private BookmarkModel mBookmarkModel;
    @Mock private Tracker mTracker;
    @Mock private Profile mProfile;
    @Mock private BookmarkUiPrefs mBookmarkUiPrefs;
    @Mock private ShoppingService mShoppingService;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;

    private ImprovedBookmarkQueryHandler mHandler;

    @Before
    public void setup() {
        doReturn(false).when(mBookmarkModel).areAccountBookmarkFoldersActive();
        TrackerFactory.setTrackerForTests(mTracker);
        SharedBookmarkModelMocks.initMocks(mBookmarkModel);

        mJniMocker.mock(CommerceFeatureUtilsJni.TEST_HOOKS, mCommerceFeatureUtilsJniMock);

        mHandler =
                new ImprovedBookmarkQueryHandler(
                        mBookmarkModel, mBookmarkUiPrefs, mShoppingService);
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_chronological() {
        doReturn(BookmarkRowSortOrder.CHRONOLOGICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        List<BookmarkId> expected =
                Arrays.asList(
                        DESKTOP_BOOKMARK_ID,
                        OTHER_BOOKMARK_ID,
                        MOBILE_BOOKMARK_ID,
                        READING_LIST_BOOKMARK_ID,
                        PARTNER_BOOKMARK_ID);
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_withAccountFolders() {
        FakeBookmarkModel fakeBookmarkModel = FakeBookmarkModel.createModel();
        fakeBookmarkModel.setAreAccountBookmarkFoldersActive(true);
        mHandler =
                new ImprovedBookmarkQueryHandler(
                        fakeBookmarkModel, mBookmarkUiPrefs, mShoppingService);

        doReturn(BookmarkRowSortOrder.CHRONOLOGICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        List<BookmarkId> expected =
                Arrays.asList(
                        null,
                        fakeBookmarkModel.getAccountOtherFolderId(),
                        fakeBookmarkModel.getAccountDesktopFolderId(),
                        fakeBookmarkModel.getAccountMobileFolderId(),
                        fakeBookmarkModel.getAccountReadingListFolder(),
                        null,
                        fakeBookmarkModel.getOtherFolderId(),
                        fakeBookmarkModel.getDesktopFolderId(),
                        fakeBookmarkModel.getMobileFolderId(),
                        fakeBookmarkModel.getLocalOrSyncableReadingListFolder());
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_reverseChronological() {
        doReturn(BookmarkRowSortOrder.REVERSE_CHRONOLOGICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        List<BookmarkId> expected =
                Arrays.asList(
                        PARTNER_BOOKMARK_ID,
                        READING_LIST_BOOKMARK_ID,
                        MOBILE_BOOKMARK_ID,
                        OTHER_BOOKMARK_ID,
                        DESKTOP_BOOKMARK_ID);
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_alphabetical() {
        doReturn(BookmarkRowSortOrder.ALPHABETICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        List<BookmarkId> expected =
                Arrays.asList(
                        DESKTOP_BOOKMARK_ID,
                        MOBILE_BOOKMARK_ID,
                        OTHER_BOOKMARK_ID,
                        PARTNER_BOOKMARK_ID,
                        READING_LIST_BOOKMARK_ID);
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_reverseAlphabetical() {
        doReturn(BookmarkRowSortOrder.REVERSE_ALPHABETICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        List<BookmarkId> expected =
                Arrays.asList(
                        READING_LIST_BOOKMARK_ID,
                        PARTNER_BOOKMARK_ID,
                        OTHER_BOOKMARK_ID,
                        MOBILE_BOOKMARK_ID,
                        DESKTOP_BOOKMARK_ID);
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_recentlyUsed() {
        doReturn(BookmarkRowSortOrder.RECENTLY_USED)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        List<BookmarkId> expected =
                Arrays.asList(
                        DESKTOP_BOOKMARK_ID,
                        OTHER_BOOKMARK_ID,
                        MOBILE_BOOKMARK_ID,
                        READING_LIST_BOOKMARK_ID,
                        PARTNER_BOOKMARK_ID);
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testBuildBookmarkListForParent_readingList() {
        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForParent(READING_LIST_BOOKMARK_ID);
        List<BookmarkId> expected = Arrays.asList(null, URL_BOOKMARK_ID_E, null, URL_BOOKMARK_ID_D);
        verifyBookmarkIds(expected, result);
        verify(mBookmarkUiPrefs, never()).getBookmarkRowSortOrder();
    }

    @Test
    public void testBuildBookmarkListForParent_manualOrdering() {
        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(MOBILE_BOOKMARK_ID);
        List<BookmarkId> expected =
                Arrays.asList(
                        FOLDER_BOOKMARK_ID_A,
                        URL_BOOKMARK_ID_A,
                        URL_BOOKMARK_ID_F,
                        URL_BOOKMARK_ID_G,
                        URL_BOOKMARK_ID_H);
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testBuildBookmarkListForParent_withShoppingFilter() {
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

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
        doReturn(shoppingMetaTracked).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_A);
        PowerBookmarkMeta shoppingMetaNotTracked =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setProductClusterId(2).build())
                        .build();
        doReturn(shoppingMetaNotTracked)
                .when(mBookmarkModel)
                .getPowerBookmarkMeta(URL_BOOKMARK_ID_B);
        PowerBookmarkMeta metaNoShopping = PowerBookmarkMeta.newBuilder().build();
        doReturn(metaNoShopping).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_C);
        doReturn(null).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_D);

        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForParent(
                        MOBILE_BOOKMARK_ID, Collections.singleton(PowerBookmarkType.SHOPPING));
        verifyBookmarkIds(Collections.singletonList(URL_BOOKMARK_ID_A), result);
    }

    @Test
    public void testSearch() {
        doReturn(BookmarkRowSortOrder.ALPHABETICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();
        // Order these initially in a non-alphabetical order.
        List<BookmarkId> queryIds =
                Arrays.asList(
                        URL_BOOKMARK_ID_D,
                        URL_BOOKMARK_ID_A,
                        URL_BOOKMARK_ID_C,
                        URL_BOOKMARK_ID_B,
                        URL_BOOKMARK_ID_E);
        doReturn(queryIds)
                .when(mBookmarkModel)
                .searchBookmarks(ArgumentMatchers.any(), ArgumentMatchers.anyInt());

        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForSearch("Url", /* powerFilter= */ null);
        List<BookmarkId> expected =
                Arrays.asList(
                        URL_BOOKMARK_ID_A,
                        URL_BOOKMARK_ID_B,
                        URL_BOOKMARK_ID_C,
                        URL_BOOKMARK_ID_D,
                        URL_BOOKMARK_ID_E);
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testSearchWithShoppingFilter() {
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        List<BookmarkId> queryIds =
                Arrays.asList(
                        URL_BOOKMARK_ID_A, URL_BOOKMARK_ID_B, URL_BOOKMARK_ID_C, URL_BOOKMARK_ID_D);
        doReturn(queryIds)
                .when(mBookmarkModel)
                .searchBookmarks(ArgumentMatchers.eq("test"), ArgumentMatchers.anyInt());
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
        doReturn(shoppingMetaTracked).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_A);
        PowerBookmarkMeta shoppingMetaNotTracked =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setProductClusterId(2).build())
                        .build();
        doReturn(shoppingMetaNotTracked)
                .when(mBookmarkModel)
                .getPowerBookmarkMeta(URL_BOOKMARK_ID_B);
        PowerBookmarkMeta metaNoShopping = PowerBookmarkMeta.newBuilder().build();
        doReturn(metaNoShopping).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_C);
        doReturn(null).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_D);

        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForSearch(
                        "test", Collections.singleton(PowerBookmarkType.SHOPPING));
        verifyBookmarkIds(Collections.singletonList(URL_BOOKMARK_ID_A), result);
    }

    @Test
    public void testSearchWithShoppingFilter_shoppingListNotEligible() {
        doReturn(false).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        List<BookmarkId> queryIds =
                Arrays.asList(
                        URL_BOOKMARK_ID_A, URL_BOOKMARK_ID_B, URL_BOOKMARK_ID_C, URL_BOOKMARK_ID_D);
        doReturn(queryIds)
                .when(mBookmarkModel)
                .searchBookmarks(ArgumentMatchers.any(), ArgumentMatchers.anyInt());
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
        doReturn(shoppingMetaTracked).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_A);
        PowerBookmarkMeta shoppingMetaNotTracked =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setProductClusterId(2).build())
                        .build();
        doReturn(shoppingMetaNotTracked)
                .when(mBookmarkModel)
                .getPowerBookmarkMeta(URL_BOOKMARK_ID_B);
        PowerBookmarkMeta metaNoShopping = PowerBookmarkMeta.newBuilder().build();
        doReturn(metaNoShopping).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_C);
        doReturn(null).when(mBookmarkModel).getPowerBookmarkMeta(URL_BOOKMARK_ID_D);

        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForSearch(
                        "", Collections.singleton(PowerBookmarkType.SHOPPING));
        verifyBookmarkIds(Collections.emptyList(), result);
    }

    @Test
    public void testBuildBookmarkListForSearch_empty() {
        doReturn(Arrays.asList(FOLDER_BOOKMARK_ID_A, URL_BOOKMARK_ID_A))
                .when(mBookmarkModel)
                .searchBookmarks(anyString(), anyInt());
        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForSearch("", /* powerFilter= */ null);
        assertEquals(0, result.size());
    }

    @Test
    public void testBuildBookmarkListForFolderSelect_rootFolder() {
        FakeBookmarkModel fakeBookmarkModel = FakeBookmarkModel.createModel();
        mHandler =
                new ImprovedBookmarkQueryHandler(
                        fakeBookmarkModel, mBookmarkUiPrefs, mShoppingService);

        doReturn(BookmarkRowSortOrder.ALPHABETICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();
        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForFolderSelect(mBookmarkModel.getRootFolderId());
        List<BookmarkId> expected =
                Arrays.asList(
                        fakeBookmarkModel.getDesktopFolderId(),
                        fakeBookmarkModel.getMobileFolderId(),
                        fakeBookmarkModel.getOtherFolderId(),
                        fakeBookmarkModel.getLocalOrSyncableReadingListFolder());
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void
            testBuildBookmarkListForFolderSelect_rootFolder_alphabetical_WithAccountBookmarks() {
        FakeBookmarkModel fakeBookmarkModel = FakeBookmarkModel.createModel();
        fakeBookmarkModel.setAreAccountBookmarkFoldersActive(true);
        mHandler =
                new ImprovedBookmarkQueryHandler(
                        fakeBookmarkModel, mBookmarkUiPrefs, mShoppingService);

        doReturn(BookmarkRowSortOrder.ALPHABETICAL)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowSortOrder();
        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForFolderSelect(mBookmarkModel.getRootFolderId());
        List<BookmarkId> expected =
                Arrays.asList(
                        null,
                        fakeBookmarkModel.getAccountDesktopFolderId(),
                        fakeBookmarkModel.getAccountMobileFolderId(),
                        fakeBookmarkModel.getAccountOtherFolderId(),
                        fakeBookmarkModel.getAccountReadingListFolder(),
                        null,
                        fakeBookmarkModel.getDesktopFolderId(),
                        fakeBookmarkModel.getMobileFolderId(),
                        fakeBookmarkModel.getOtherFolderId(),
                        fakeBookmarkModel.getLocalOrSyncableReadingListFolder());
        verifyBookmarkIds(expected, result);
    }

    @Test
    public void testBuildBookmarkListForFolderSelect_rootFolder_manual_WithAccountBookmarks() {
        FakeBookmarkModel fakeBookmarkModel = FakeBookmarkModel.createModel();
        fakeBookmarkModel.setAreAccountBookmarkFoldersActive(true);
        mHandler =
                new ImprovedBookmarkQueryHandler(
                        fakeBookmarkModel, mBookmarkUiPrefs, mShoppingService);

        doReturn(BookmarkRowSortOrder.MANUAL).when(mBookmarkUiPrefs).getBookmarkRowSortOrder();
        List<BookmarkListEntry> result =
                mHandler.buildBookmarkListForFolderSelect(mBookmarkModel.getRootFolderId());
        List<BookmarkId> expected =
                Arrays.asList(
                        null,
                        fakeBookmarkModel.getAccountOtherFolderId(),
                        fakeBookmarkModel.getAccountDesktopFolderId(),
                        fakeBookmarkModel.getAccountMobileFolderId(),
                        fakeBookmarkModel.getAccountReadingListFolder(),
                        null,
                        fakeBookmarkModel.getOtherFolderId(),
                        fakeBookmarkModel.getDesktopFolderId(),
                        fakeBookmarkModel.getMobileFolderId(),
                        fakeBookmarkModel.getLocalOrSyncableReadingListFolder());
        verifyBookmarkIds(expected, result);
    }

    private void verifyBookmarkIds(
            List<BookmarkId> expectedList, List<BookmarkListEntry> actualList) {
        assertEquals("Lists differ in size", expectedList.size(), actualList.size());
        for (int i = 0; i < expectedList.size(); ++i) {
            final @Nullable BookmarkId expectedId = expectedList.get(i);
            final @Nullable BookmarkItem actualBookmarkItem = actualList.get(i).getBookmarkItem();
            final @Nullable BookmarkId actualBookmarkId =
                    actualBookmarkItem == null ? null : actualBookmarkItem.getId();
            assertEquals("Mismatch at index: " + i, expectedId, actualBookmarkId);
        }
    }
}
