// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridge;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

/** Unit tests for {@link BookmarkUtils}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(SyncFeatureMap.SYNC_ENABLE_BOOKMARKS_IN_TRANSPORT_MODE)
public class BookmarkUtilsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Tracker mTracker;
    @Mock private LargeIconBridge mLargeIconBridge;
    @Mock private LargeIconBridge.Natives mLargeIconBridgeNatives;
    @Mock private ImageServiceBridge.Natives mImageServiceBridgeJni;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Callback<BookmarkId> mBookmarkIdCallback;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;
    @Mock private ShoppingService mShoppingService;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private IdentityManager mIdentityManager;

    private Activity mActivity;
    private FakeBookmarkModel mBookmarkModel;
    private CoreAccountInfo mAccountInfo =
            CoreAccountInfo.createFromEmailAndGaiaId("test@gmail.com", "testGaiaId");

    @Before
    public void setup() {
        mBookmarkModel = FakeBookmarkModel.createModel();

        TrackerFactory.setTrackerForTests(mTracker);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);

        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mLargeIconBridgeNatives);
        mJniMocker.mock(ImageServiceBridgeJni.TEST_HOOKS, mImageServiceBridgeJni);
        mJniMocker.mock(CommerceFeatureUtilsJni.TEST_HOOKS, mCommerceFeatureUtilsJniMock);
        mJniMocker.mock(ShoppingServiceFactoryJni.TEST_HOOKS, mShoppingServiceFactoryJniMock);
        doReturn(mShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());
        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(any());
        doReturn(mAccountInfo).when(mIdentityManager).getPrimaryAccountInfo(anyInt());
        doReturn(mProfile).when(mTab).getProfile();
        doReturn(false).when(mProfile).isOffTheRecord();

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
    }

    @Test
    public void testAddToReadingList() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Bookmarks.AddBookmarkType", BookmarkType.READING_LIST)
                        .expectIntRecords(
                                "Bookmarks.AddedPerProfileType", BrowserProfileType.REGULAR)
                        .build();

        BookmarkModel bookmarkModel = FakeBookmarkModel.createModel();
        BookmarkUtils.addToReadingList(
                mActivity,
                bookmarkModel,
                "Test title",
                new GURL("https://test.com"),
                mSnackbarManager,
                mProfile,
                mBottomSheetController);
        // Normally, a snackbar is shown.
        verify(mSnackbarManager).showSnackbar(any());
        verify(mTracker).notifyEvent(EventConstants.READ_LATER_ARTICLE_SAVED);

        histograms.assertExpected();
    }

    @Test
    public void testAddToReadingList_withAccountBookmarks() {
        mBookmarkModel.setAreAccountBookmarkFoldersActive(true);
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Bookmarks.AddBookmarkType", BookmarkType.READING_LIST)
                        .expectIntRecords(
                                "Bookmarks.AddedPerProfileType", BrowserProfileType.REGULAR)
                        .build();

        BookmarkUtils.addToReadingList(
                mActivity,
                mBookmarkModel,
                "Test title",
                new GURL("https://test.com"),
                mSnackbarManager,
                mProfile,
                mBottomSheetController);
        // When account bookmarks are enabled, reading list saves use the regular save flow.
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
        verify(mTracker).notifyEvent(EventConstants.READ_LATER_ARTICLE_SAVED);

        histograms.assertExpected();
    }

    @Test
    public void testAddOrEditBookmark() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Bookmarks.AddBookmarkType", BookmarkType.NORMAL)
                        .expectIntRecords(
                                "Bookmarks.AddedPerProfileType", BrowserProfileType.REGULAR)
                        .build();

        doReturn("test title").when(mTab).getTitle();
        doReturn(new GURL("https://test.com")).when(mTab).getOriginalUrl();
        BookmarkUtils.addOrEditBookmark(
                null,
                mBookmarkModel,
                mTab,
                mBottomSheetController,
                mActivity,
                BookmarkType.NORMAL,
                mBookmarkIdCallback,
                /* fromExplicitTrackUi= */ false);

        histograms.assertExpected();
    }

    @Test
    public void testAddOrEditBookmark_readingList() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Bookmarks.AddBookmarkType", BookmarkType.READING_LIST)
                        .expectIntRecords(
                                "Bookmarks.AddedPerProfileType", BrowserProfileType.REGULAR)
                        .build();

        doReturn("test title").when(mTab).getTitle();
        doReturn(new GURL("https://test.com")).when(mTab).getOriginalUrl();
        BookmarkUtils.addOrEditBookmark(
                null,
                mBookmarkModel,
                mTab,
                mBottomSheetController,
                mActivity,
                BookmarkType.READING_LIST,
                mBookmarkIdCallback,
                /* fromExplicitTrackUi= */ false);

        histograms.assertExpected();
    }

    @Test
    public void testCanAddFolderToParent() {
        assertFalse(
                BookmarkUtils.canAddFolderToParent(
                        mBookmarkModel, mBookmarkModel.getRootFolderId()));
        assertTrue(
                BookmarkUtils.canAddFolderToParent(
                        mBookmarkModel, mBookmarkModel.getDesktopFolderId()));
        assertTrue(
                BookmarkUtils.canAddFolderToParent(
                        mBookmarkModel, mBookmarkModel.getMobileFolderId()));
        assertFalse(
                BookmarkUtils.canAddFolderToParent(
                        mBookmarkModel, mBookmarkModel.getLocalOrSyncableReadingListFolder()));
        assertFalse(
                BookmarkUtils.canAddFolderToParent(
                        mBookmarkModel, mBookmarkModel.getAccountReadingListFolder()));
        assertFalse(
                BookmarkUtils.canAddFolderToParent(
                        mBookmarkModel, mBookmarkModel.getPartnerFolderId()));

        BookmarkId folder =
                mBookmarkModel.addFolder(mBookmarkModel.getMobileFolderId(), 0, "folder");
        assertTrue(BookmarkUtils.canAddFolderToParent(mBookmarkModel, folder));

        BookmarkId managedFolder =
                mBookmarkModel.addManagedFolder(mBookmarkModel.getMobileFolderId(), "managed");
        assertFalse(BookmarkUtils.canAddFolderToParent(mBookmarkModel, managedFolder));
    }
}
