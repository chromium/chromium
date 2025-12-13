// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridge;
import org.chromium.chrome.browser.page_image_service.ImageServiceBridgeJni;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link BookmarkUtils}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkUtilsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Tracker mTracker;
    @Mock private FaviconHelperJni mFaviconHelperJni;
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
    @Mock private BookmarkManagerOpener mBookmarkManagerOpener;
    @Mock private PriceDropNotificationManager mPriceDropNotificationManager;

    private Activity mActivity;
    private FakeBookmarkModel mBookmarkModel;
    private final CoreAccountInfo mAccountInfo =
            CoreAccountInfo.createFromEmailAndGaiaId("test@gmail.com", new GaiaId("testGaiaId"));

    @Before
    public void setup() {
        mBookmarkModel = FakeBookmarkModel.createModel();

        TrackerFactory.setTrackerForTests(mTracker);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);

        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJni);
        ImageServiceBridgeJni.setInstanceForTesting(mImageServiceBridgeJni);
        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingServiceFactoryJniMock);
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
                mBottomSheetController,
                mBookmarkManagerOpener,
                mPriceDropNotificationManager);
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
                mBottomSheetController,
                mBookmarkManagerOpener,
                mPriceDropNotificationManager);
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
                /* fromExplicitTrackUi= */ false,
                mBookmarkManagerOpener,
                mPriceDropNotificationManager,
                false);

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
                /* fromExplicitTrackUi= */ false,
                mBookmarkManagerOpener,
                mPriceDropNotificationManager,
                false);

        histograms.assertExpected();
    }

    @Test
    public void testAddBookmarkInternal() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Bookmarks.AddBookmarkType", BookmarkType.NORMAL)
                        .expectIntRecords(
                                "Bookmarks.AddedPerProfileType", BrowserProfileType.REGULAR)
                        .build();
        UserActionTester userActionTester = new UserActionTester();
        BookmarkModel mMockBookmarkModel = mock(BookmarkModel.class);
        BookmarkId root = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkUtils.setLastUsedParent(root);
        assertTrue(BookmarkUtils.getLastUsedParent().equals(root));

        BookmarkId parent = new BookmarkId(1, BookmarkType.NORMAL);
        BookmarkItem parentBookmarkItem =
                new BookmarkItem(
                        parent, "parent", null, false, null, false, false, 0, false, 0, false);
        when(mMockBookmarkModel.getBookmarkById(parent)).thenReturn(parentBookmarkItem);
        when(mMockBookmarkModel.getDefaultBookmarkFolder()).thenReturn(parent);
        BookmarkId addedBookmark = new BookmarkId(2, BookmarkType.NORMAL);
        when(mMockBookmarkModel.addBookmark(
                        any(BookmarkId.class), anyInt(), anyString(), any(GURL.class)))
                .thenReturn(addedBookmark);

        BookmarkId bookmark =
                BookmarkUtils.addBookmarkInternal(
                        null,
                        mProfile,
                        mMockBookmarkModel,
                        "Test title",
                        new GURL("https://test.com"),
                        parent,
                        BookmarkType.NORMAL);
        verify(mMockBookmarkModel).getBookmarkById(parent);
        verify(mMockBookmarkModel).getDefaultBookmarkFolder();
        verify(mMockBookmarkModel)
                .addBookmark(parent, 0, "Test title", new GURL("https://test.com"));
        assertTrue(bookmark.equals(addedBookmark));
        // Ensure that cached bookmark parent is not set to parent bookmark folder
        assertFalse(BookmarkUtils.getLastUsedParent().equals(parent));
        histograms.assertExpected();
        assertFalse(userActionTester.getActions().contains("BookmarkAdded.Failure"));
        userActionTester.tearDown();
    }

    @Test
    public void testAddBookmarkInternal_failToAddBookmark() {
        // Do not expect to see any histograms for failed bookmark add
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Bookmarks.AddBookmarkType")
                        .expectNoRecords("Bookmarks.AddedPerProfileType")
                        .expectIntRecords("BookmarkAdded.Failure")
                        .build();
        UserActionTester userActionTester = new UserActionTester();
        BookmarkModel mMockBookmarkModel = mock(BookmarkModel.class);
        BookmarkId parent = new BookmarkId(0, BookmarkType.NORMAL);
        BookmarkItem parentBookmarkItem =
                new BookmarkItem(
                        parent, "parent", null, false, null, false, false, 0, false, 0, false);
        when(mMockBookmarkModel.getBookmarkById(parent)).thenReturn(parentBookmarkItem);
        when(mMockBookmarkModel.getDefaultBookmarkFolder()).thenReturn(parent);
        BookmarkUtils.setLastUsedParent(parent);
        assertTrue(BookmarkUtils.getLastUsedParent() != null);
        // Simulate failing to add bookmark by returning null in addBookmark
        when(mMockBookmarkModel.addBookmark(
                        any(BookmarkId.class), anyInt(), anyString(), any(GURL.class)))
                .thenReturn(null);

        BookmarkId bookmark =
                BookmarkUtils.addBookmarkInternal(
                        null,
                        mProfile,
                        mMockBookmarkModel,
                        "Test title",
                        new GURL("https://test.com"),
                        parent,
                        BookmarkType.NORMAL);
        verify(mMockBookmarkModel).getBookmarkById(parent);
        verify(mMockBookmarkModel).getDefaultBookmarkFolder();
        verify(mMockBookmarkModel)
                .addBookmark(parent, 0, "Test title", new GURL("https://test.com"));
        assertTrue(bookmark == null);
        // Ensure that cached bookmark parent is reset to default after failing to add bookmark
        assertTrue(BookmarkUtils.getLastUsedParent() == null);
        histograms.assertExpected();
        assertTrue(userActionTester.getActions().contains("BookmarkAdded.Failure"));
        userActionTester.tearDown();
    }

    @Test
    public void testAddBookmarkInternal_addWhenBookmarkBarVisible() {
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Bookmarks.AddBookmarkType", BookmarkType.NORMAL)
                        .expectIntRecords(
                                "Bookmarks.AddedPerProfileType", BrowserProfileType.REGULAR)
                        .build();
        UserActionTester userActionTester = new UserActionTester();
        BookmarkModel mMockBookmarkModel = mock(BookmarkModel.class);
        assertNull(BookmarkUtils.getLastUsedParent());

        // Create a default folder ("Mobile bookmarks").
        BookmarkId mobileBookmarkFolder = new BookmarkId(1, BookmarkType.NORMAL);
        when(mMockBookmarkModel.getDefaultBookmarkFolder()).thenReturn(mobileBookmarkFolder);
        BookmarkItem mobileBookmarkFolderItem =
                new BookmarkItem(
                        mobileBookmarkFolder,
                        "parent",
                        null,
                        true,
                        null,
                        false,
                        false,
                        0,
                        false,
                        0,
                        false);
        when(mMockBookmarkModel.getBookmarkById(mobileBookmarkFolder))
                .thenReturn(mobileBookmarkFolderItem);

        // Create a desktop folder ("Bookmark bar").
        BookmarkId bookmarkBarFolder = new BookmarkId(2, BookmarkType.NORMAL);
        when(mMockBookmarkModel.getDesktopFolderId()).thenReturn(bookmarkBarFolder);
        BookmarkItem bookmarkBarFolderItem =
                new BookmarkItem(
                        bookmarkBarFolder,
                        "parent",
                        null,
                        true,
                        null,
                        false,
                        false,
                        0,
                        false,
                        0,
                        false);
        when(mMockBookmarkModel.getBookmarkById(bookmarkBarFolder))
                .thenReturn(bookmarkBarFolderItem);

        // Mock adding a new bookmark.
        BookmarkId newBookmark = new BookmarkId(3, BookmarkType.NORMAL);
        when(mMockBookmarkModel.addBookmark(
                        any(BookmarkId.class), anyInt(), anyString(), any(GURL.class)))
                .thenReturn(newBookmark);

        BookmarkId bookmark =
                BookmarkUtils.addBookmarkInternal(
                        null,
                        mProfile,
                        mMockBookmarkModel,
                        "Test title",
                        new GURL("https://test.com"),
                        null,
                        BookmarkType.NORMAL,
                        true);

        // In this case we should not be adding to the defaultBookmarkFolder, and should instead be
        // adding to the desktopFolderId (bookmark bar). The last used parent should not update
        // when we save for this case.
        verify(mMockBookmarkModel).getDesktopFolderId();
        verify(mMockBookmarkModel).getBookmarkById(bookmarkBarFolder);
        verify(mMockBookmarkModel)
                .addBookmark(bookmarkBarFolder, 0, "Test title", new GURL("https://test.com"));
        assertTrue(bookmark.equals(newBookmark));
        // Ensure that cached bookmark parent is not set to parent bookmark folder
        assertNull(BookmarkUtils.getLastUsedParent());
        histograms.assertExpected();
        assertFalse(userActionTester.getActions().contains("BookmarkAdded.Failure"));
        userActionTester.tearDown();
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

    @Test
    public void isReadingListSupported() {
        assertFalse(BookmarkUtils.isReadingListSupported(null));
        assertFalse(BookmarkUtils.isReadingListSupported(GURL.emptyGURL()));
        assertFalse(BookmarkUtils.isReadingListSupported(JUnitTestGURLs.NTP_URL));
        assertTrue(BookmarkUtils.isReadingListSupported(JUnitTestGURLs.EXAMPLE_URL));
        assertTrue(BookmarkUtils.isReadingListSupported(JUnitTestGURLs.HTTP_URL));

        // empty url
        GURL testUrl = GURL.emptyGURL();
        assertFalse(BookmarkUtils.isReadingListSupported(testUrl));

        // invalid url
        assertFalse(BookmarkUtils.isReadingListSupported(JUnitTestGURLs.INVALID_URL));
    }
}
