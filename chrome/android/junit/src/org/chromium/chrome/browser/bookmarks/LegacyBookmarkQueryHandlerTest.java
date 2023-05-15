// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.DESKTOP_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.MOBILE_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.OTHER_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.READING_LIST_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.ROOT_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_A;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.SyncService.SyncStateChangedListener;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.feature_engagement.Tracker;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link LegacyBookmarkQueryHandler}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
public class LegacyBookmarkQueryHandlerTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private SyncService mSyncService;
    @Mock
    private Tracker mTracker;
    @Mock
    private Profile mProfile;
    @Mock
    private BookmarkUiPrefs mBookmarkUiPrefs;

    @Captor
    private ArgumentCaptor<Runnable> mFinishLoadingBookmarkModelCaptor;
    @Captor
    private ArgumentCaptor<SyncStateChangedListener> mSyncStateChangedListenerCaptor;

    private LegacyBookmarkQueryHandler mHandler;

    @Before
    public void setup() {
        SyncService.overrideForTests(mSyncService);
        TrackerFactory.setTrackerForTests(mTracker);
        Profile.setLastUsedProfileForTesting(mProfile);
        SharedBookmarkModelMocks.initMocks(mBookmarkModel);
        ShoppingFeatures.setShoppingListEligibleForTesting(false);

        mHandler = new LegacyBookmarkQueryHandler(mBookmarkModel, mBookmarkUiPrefs);
    }

    @Test
    public void testDestroy() {
        verify(mSyncService).addSyncStateChangedListener(any());

        mHandler.destroy();
        verify(mSyncService).removeSyncStateChangedListener(any());
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_isFolderVisible() {
        verify(mBookmarkModel)
                .finishLoadingBookmarkModel(mFinishLoadingBookmarkModelCaptor.capture());
        doReturn(true).when(mBookmarkModel).isBookmarkModelLoaded();
        mFinishLoadingBookmarkModelCaptor.getValue().run();

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        assertEquals(3, result.size());

        doReturn(true).when(mBookmarkModel).isFolderVisible(OTHER_BOOKMARK_ID);
        verify(mSyncService).addSyncStateChangedListener(mSyncStateChangedListenerCaptor.capture());
        mSyncStateChangedListenerCaptor.getValue().syncStateChanged();

        List<BookmarkListEntry> updatedResult =
                mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        assertEquals(4, updatedResult.size());
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_isBookmarkModelLoaded() {
        verify(mSyncService).addSyncStateChangedListener(mSyncStateChangedListenerCaptor.capture());
        mSyncStateChangedListenerCaptor.getValue().syncStateChanged();

        assertTrue(mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID).isEmpty());

        doReturn(true).when(mBookmarkModel).isBookmarkModelLoaded();
        mSyncStateChangedListenerCaptor.getValue().syncStateChanged();

        assertFalse(mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID).isEmpty());
    }

    @Test
    public void testBuildBookmarkListForParent_rootFolder_withShopping() {
        ShoppingFeatures.setShoppingListEligibleForTesting(true);
        verify(mBookmarkModel)
                .finishLoadingBookmarkModel(mFinishLoadingBookmarkModelCaptor.capture());
        doReturn(true).when(mBookmarkModel).isBookmarkModelLoaded();
        mFinishLoadingBookmarkModelCaptor.getValue().run();

        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID);
        assertEquals(5, result.size());
        assertEquals(READING_LIST_BOOKMARK_ID, result.get(0).getBookmarkItem().getId());
        assertEquals(MOBILE_BOOKMARK_ID, result.get(1).getBookmarkItem().getId());
        assertEquals(DESKTOP_BOOKMARK_ID, result.get(2).getBookmarkItem().getId());
        assertEquals(ViewType.DIVIDER, result.get(3).getViewType());
        assertEquals(ViewType.SHOPPING_FILTER, result.get(4).getViewType());

        assertFalse(mHandler.buildBookmarkListForParent(ROOT_BOOKMARK_ID).isEmpty());
    }

    @Test
    public void testBuildBookmarkListForParent_nonRootFolder() {
        List<BookmarkListEntry> result = mHandler.buildBookmarkListForParent(MOBILE_BOOKMARK_ID);

        assertEquals(2, result.size());
        assertEquals(FOLDER_BOOKMARK_ID_A, result.get(0).getBookmarkItem().getId());
        assertEquals(URL_BOOKMARK_ID_A, result.get(1).getBookmarkItem().getId());
    }

    @Test
    public void testBuildBookmarkListForSearch() {
        doReturn(Arrays.asList(FOLDER_BOOKMARK_ID_A, URL_BOOKMARK_ID_A))
                .when(mBookmarkModel)
                .searchBookmarks("A", 500);
        List<BookmarkListEntry> result = mHandler.buildBookmarkListForSearch("A");
        assertEquals(2, result.size());
    }
}
