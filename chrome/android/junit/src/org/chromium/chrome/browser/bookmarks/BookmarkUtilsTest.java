// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.DESKTOP_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.PARTNER_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.READING_LIST_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.ROOT_BOOKMARK_ID;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncFeatureMap;
import org.chromium.ui.base.TestActivity;
import org.chromium.url.GURL;

/** Unit tests for {@link BookmarkUtils}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkUtilsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private BookmarkModel mBookmarkModel;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Profile mProfile;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Tracker mTracker;
    @Mock private ShoppingService mShoppingService;
    @Mock private IdentityManager mIdentityManager;
    @Mock private LargeIconBridge mLargeIconBridge;
    @Mock private LargeIconBridge.Natives mLargeIconBridgeNatives;

    private Activity mActivity;

    @Before
    public void setup() {
        SharedBookmarkModelMocks.initMocks(mBookmarkModel);
        TrackerFactory.setTrackerForTests(mTracker);
        ShoppingServiceFactory.setShoppingServiceForTesting(mShoppingService);

        IdentityServicesProvider identityServicesProvider =
                Mockito.mock(IdentityServicesProvider.class);
        doReturn(mIdentityManager).when(identityServicesProvider).getIdentityManager(mProfile);
        IdentityServicesProvider.setInstanceForTests(identityServicesProvider);

        mJniMocker.mock(LargeIconBridgeJni.TEST_HOOKS, mLargeIconBridgeNatives);

        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mActivity = activity;
    }

    @Test
    @DisableFeatures({SyncFeatureMap.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE})
    public void testAddToReadingList() {
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
    }

    @Test
    @EnableFeatures({SyncFeatureMap.ENABLE_BOOKMARK_FOLDERS_FOR_ACCOUNT_STORAGE})
    public void testAddToReadingList_withAccountBookmarks() {
        BookmarkModel bookmarkModel = FakeBookmarkModel.createModel();
        BookmarkUtils.addToReadingList(
                mActivity,
                bookmarkModel,
                "Test title",
                new GURL("https://test.com"),
                mSnackbarManager,
                mProfile,
                mBottomSheetController);
        // When account bookmarks are enabled, reading list saves use the regular save flow.
        verify(mBottomSheetController).requestShowContent(any(), anyBoolean());
        verify(mTracker).notifyEvent(EventConstants.READ_LATER_ARTICLE_SAVED);
    }

    @Test
    public void testCanAddFolderToParent_accountFolders() {
        BookmarkModel fakeBookmarkModel = FakeBookmarkModel.createModel();
        assertFalse(
                BookmarkUtils.canAddFolderToParent(
                        fakeBookmarkModel, fakeBookmarkModel.getAccountReadingListFolder()));
    }

    @Test
    public void testCanAddFolderToParent() {
        assertFalse(BookmarkUtils.canAddFolderToParent(mBookmarkModel, ROOT_BOOKMARK_ID));
        assertTrue(BookmarkUtils.canAddFolderToParent(mBookmarkModel, DESKTOP_BOOKMARK_ID));
        assertTrue(BookmarkUtils.canAddFolderToParent(mBookmarkModel, FOLDER_BOOKMARK_ID_A));
        assertFalse(BookmarkUtils.canAddFolderToParent(mBookmarkModel, READING_LIST_BOOKMARK_ID));
        assertFalse(BookmarkUtils.canAddFolderToParent(mBookmarkModel, PARTNER_BOOKMARK_ID));

        BookmarkId managedBookmarkId = new BookmarkId(123, BookmarkType.NORMAL);
        BookmarkItem managedBookmarkItem =
                new BookmarkItem(
                        managedBookmarkId,
                        "managed",
                        null,
                        true,
                        ROOT_BOOKMARK_ID,
                        false,
                        true,
                        0,
                        false,
                        0,
                        false);
        doReturn(managedBookmarkItem).when(mBookmarkModel).getBookmarkById(managedBookmarkId);
        assertFalse(BookmarkUtils.canAddFolderToParent(mBookmarkModel, managedBookmarkId));
    }

    @Test
    public void testCanAddBookmarkToParent() {
        assertFalse(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, ROOT_BOOKMARK_ID));
        assertTrue(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, DESKTOP_BOOKMARK_ID));
        assertTrue(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, FOLDER_BOOKMARK_ID_A));
        assertTrue(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, READING_LIST_BOOKMARK_ID));
        assertFalse(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, PARTNER_BOOKMARK_ID));

        // Null case
        BookmarkId nullBookmarkItemId = new BookmarkId(123, BookmarkType.NORMAL);
        doReturn(null).when(mBookmarkModel).getBookmarkById(nullBookmarkItemId);
        assertFalse(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, nullBookmarkItemId));

        BookmarkId managedBookmarkId = new BookmarkId(123, BookmarkType.NORMAL);
        BookmarkItem managedBookmarkItem =
                new BookmarkItem(
                        managedBookmarkId,
                        "managed",
                        null,
                        true,
                        ROOT_BOOKMARK_ID,
                        false,
                        true,
                        0,
                        false,
                        0,
                        false);
        doReturn(managedBookmarkItem).when(mBookmarkModel).getBookmarkById(managedBookmarkId);
        assertFalse(BookmarkUtils.canAddBookmarkToParent(mBookmarkModel, managedBookmarkId));
    }
}
