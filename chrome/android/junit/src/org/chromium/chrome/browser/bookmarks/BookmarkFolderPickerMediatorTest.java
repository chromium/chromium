// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.Menu;
import android.view.MenuItem;

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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;

/** Unit tests for {@link BookmarkFolderPickerMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkFolderPickerMediatorTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    // Initial structure:
    // Root
    //  Mobile
    //   UserFolder
    //    UserBookmark
    //   UserFolder
    private final BookmarkId mRootFolderId = new BookmarkId(/*id=*/1, BookmarkType.NORMAL);
    private final BookmarkId mDesktopFolderId = new BookmarkId(/*id=*/2, BookmarkType.NORMAL);
    private final BookmarkId mMobileFolderId = new BookmarkId(/*id=*/3, BookmarkType.NORMAL);
    private final BookmarkId mOtherFolderId = new BookmarkId(/*id=*/4, BookmarkType.NORMAL);
    private final BookmarkId mReadingListFolderId =
            new BookmarkId(/*id=*/5, BookmarkType.READING_LIST);
    private final BookmarkId mUserFolderId = new BookmarkId(/*id=*/6, BookmarkType.NORMAL);
    private final BookmarkId mUserBookmarkId = new BookmarkId(/*id=*/7, BookmarkType.NORMAL);
    private final BookmarkId mUserFolderId2 = new BookmarkId(/*id=*/8, BookmarkType.NORMAL);
    private final BookmarkId mUserBookmarkId1 = new BookmarkId(/*id=*/9, BookmarkType.NORMAL);

    private final BookmarkItem mRootFolderItem =
            new BookmarkItem(mRootFolderId, "Root", null, true, null, false, false, 0, false, 0);
    private final BookmarkItem mDesktopFolderItem = new BookmarkItem(
            mDesktopFolderId, "Bookmarks bar", null, true, mRootFolderId, true, false, 0, false, 0);
    private final BookmarkItem mMobileFolderItem = new BookmarkItem(mMobileFolderId,
            "Mobile bookmarks", null, true, mRootFolderId, true, false, 0, false, 0);
    private final BookmarkItem mOtherFolderItem = new BookmarkItem(
            mOtherFolderId, "Other bookmarks", null, true, mRootFolderId, true, false, 0, false, 0);
    private final BookmarkItem mReadingListFolderItem = new BookmarkItem(mReadingListFolderId,
            "Reading List", null, true, mRootFolderId, false, false, 0, false, 0);
    private final BookmarkItem mUserFolderItem = new BookmarkItem(
            mUserFolderId, "UserFolder", null, true, mMobileFolderId, false, false, 0, false, 0);
    private final BookmarkItem mUserBookmarkItem = new BookmarkItem(mUserBookmarkId, "Bookmark",
            JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), false, mUserFolderId, true, false,
            0, false, 0);
    private final BookmarkItem mUserFolderItem2 = new BookmarkItem(
            mUserFolderId2, "UserFolder2", null, true, mMobileFolderId, false, false, 0, false, 0);
    private final BookmarkItem mUserBookmarkItem1 = new BookmarkItem(mUserBookmarkId1, "Bookmark1",
            JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), false, mUserFolderId, true, false,
            0, false, 0);

    @Mock
    private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private Runnable mFinishRunnable;
    @Mock
    private BookmarkUiPrefs mBookmarkUiPrefs;
    @Mock
    private Bitmap mBitmap;
    @Mock
    private Profile mProfile;
    @Mock
    private Tracker mTracker;
    @Mock
    private Menu mMenu;
    @Mock
    private MenuItem mMenuItem;
    @Mock
    private BookmarkAddNewFolderCoordinator mAddNewFolderCoordinator;
    @Mock
    private ShoppingService mShoppingService;

    private Activity mActivity;
    private BookmarkFolderPickerMediator mMediator;
    private PropertyModel mModel = new PropertyModel(BookmarkFolderPickerProperties.ALL_KEYS);
    private ModelList mModelList = new ModelList();

    @Before
    public void setUp() throws Exception {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        // Setup profile-related factories.
        Profile.setLastUsedProfileForTesting(mProfile);
        TrackerFactory.setTrackerForTests(mTracker);

        // Setup BookmarkModel.
        doReturn(true).when(mBookmarkModel).isFolderVisible(any());
        doReturn(mRootFolderId).when(mBookmarkModel).getRootFolderId();
        doReturn(mRootFolderItem).when(mBookmarkModel).getBookmarkById(mRootFolderId);
        // Reading list folder
        doReturn(mReadingListFolderId).when(mBookmarkModel).getReadingListFolder();
        doReturn(mReadingListFolderItem).when(mBookmarkModel).getBookmarkById(mReadingListFolderId);
        doReturn(Arrays.asList(mReadingListFolderId))
                .when(mBookmarkModel)
                .getTopLevelFolderIds(/*getSpecial=*/true, /*getNormal=*/false);
        // Mobile bookmarks folder
        doReturn(mMobileFolderId).when(mBookmarkModel).getMobileFolderId();
        doReturn(mMobileFolderItem).when(mBookmarkModel).getBookmarkById(mMobileFolderId);
        doReturn(Arrays.asList(mUserFolderId, mUserFolderId2))
                .when(mBookmarkModel)
                .getChildIds(mMobileFolderId);
        doReturn(2).when(mBookmarkModel).getTotalBookmarkCount(mMobileFolderId);
        // Desktop folder.
        doReturn(mDesktopFolderId).when(mBookmarkModel).getDesktopFolderId();
        doReturn(mDesktopFolderItem).when(mBookmarkModel).getBookmarkById(mDesktopFolderId);
        doReturn(Arrays.asList()).when(mBookmarkModel).getChildIds(mDesktopFolderId);
        doReturn(0).when(mBookmarkModel).getTotalBookmarkCount(mMobileFolderId);
        // Other folder.
        doReturn(mOtherFolderId).when(mBookmarkModel).getOtherFolderId();
        doReturn(mOtherFolderItem).when(mBookmarkModel).getBookmarkById(mOtherFolderId);
        doReturn(Arrays.asList()).when(mBookmarkModel).getChildIds(mOtherFolderId);
        doReturn(0).when(mBookmarkModel).getTotalBookmarkCount(mMobileFolderId);
        // User folders/bookmarks.
        doReturn(mUserFolderItem).when(mBookmarkModel).getBookmarkById(mUserFolderId);
        doReturn(Arrays.asList(mUserBookmarkId)).when(mBookmarkModel).getChildIds(mUserFolderId);
        doReturn(1).when(mBookmarkModel).getTotalBookmarkCount(mUserFolderId);
        doReturn(mUserFolderItem2).when(mBookmarkModel).getBookmarkById(mUserFolderId2);
        doReturn(Arrays.asList()).when(mBookmarkModel).getChildIds(mUserFolderId2);
        doReturn(0).when(mBookmarkModel).getTotalBookmarkCount(mUserFolderId2);
        doReturn(mUserBookmarkItem).when(mBookmarkModel).getBookmarkById(mUserBookmarkId);
        doReturn(true).when(mBookmarkModel).doesBookmarkExist(any());
        doAnswer((invocation) -> {
            Runnable runnable = invocation.getArgument(0);
            runnable.run();
            return null;
        })
                .when(mBookmarkModel)
                .finishLoadingBookmarkModel(any());

        // Setup menu
        doReturn(mMenuItem).when(mMenu).add(anyInt());
        doReturn(mMenuItem).when(mMenuItem).setIcon(any());
        doReturn(mMenuItem).when(mMenuItem).setShowAsActionFlags(anyInt());

        // Setup BookmarkImageFetcher.
        doAnswer((invocation) -> {
            Callback<Pair<Drawable, Drawable>> callback = invocation.getArgument(1);
            callback.onResult(new Pair<>(null, null));
            return null;
        })
                .when(mBookmarkImageFetcher)
                .fetchFirstTwoImagesForFolder(any(), any());

        mMediator = new BookmarkFolderPickerMediator(mActivity, mBookmarkModel,
                mBookmarkImageFetcher, Arrays.asList(mUserBookmarkId), mUserFolderId,
                mFinishRunnable, mBookmarkUiPrefs, mModel, mModelList, mAddNewFolderCoordinator,
                new ImprovedBookmarkRowCoordinator(mActivity, mBookmarkImageFetcher, mBookmarkModel,
                        mBookmarkUiPrefs, mShoppingService));
    }

    @Test
    public void testMoveFolder() {
        mMediator = new BookmarkFolderPickerMediator(mActivity, mBookmarkModel,
                mBookmarkImageFetcher, Arrays.asList(mUserFolderId), mUserFolderId, mFinishRunnable,
                mBookmarkUiPrefs, mModel, mModelList, mAddNewFolderCoordinator,
                new ImprovedBookmarkRowCoordinator(mActivity, mBookmarkImageFetcher, mBookmarkModel,
                        mBookmarkUiPrefs, mShoppingService));
        mMediator.populateFoldersForParentId(mMobileFolderId);

        // Check that the UserFolder isn't a row since it should be filtered out because it's the
        // same as the bookmark being moved.
        for (ListItem item : mModelList) {
            assertNotEquals(mUserFolderId, item.model.get(BookmarkManagerProperties.BOOKMARK_ID));
        }
    }

    @Test
    public void testCancel() {
        mMediator.populateFoldersForParentId(mUserFolderId2);
        mModel.get(BookmarkFolderPickerProperties.MOVE_CLICK_LISTENER).onClick(null);
        verify(mBookmarkModel).moveBookmarks(Arrays.asList(mUserBookmarkId), mUserFolderId2);
        verify(mFinishRunnable).run();
    }

    @Test
    public void testMove() {
        mModel.get(BookmarkFolderPickerProperties.CANCEL_CLICK_LISTENER).onClick(null);
        verify(mFinishRunnable).run();
    }

    @Test
    public void testInitialParent_skipsNonFolder() {
        assertEquals(0, mModelList.size());
        assertEquals(mUserFolderItem.getTitle(),
                mModel.get(BookmarkFolderPickerProperties.TOOLBAR_TITLE));
        assertFalse(mModel.get(BookmarkFolderPickerProperties.MOVE_BUTTON_ENABLED));
    }

    @Test
    public void testParentWithDifferentFolders() {
        mMediator.populateFoldersForParentId(mMobileFolderId);
        assertEquals(2, mModelList.size());
        assertEquals(mMobileFolderItem.getTitle(),
                mModel.get(BookmarkFolderPickerProperties.TOOLBAR_TITLE));
        assertTrue(mModel.get(BookmarkFolderPickerProperties.MOVE_BUTTON_ENABLED));
    }

    @Test
    public void testRootFolder() {
        mMediator.populateFoldersForParentId(mRootFolderId);
        assertEquals(4, mModelList.size());
        assertEquals("Move to…", mModel.get(BookmarkFolderPickerProperties.TOOLBAR_TITLE));
        assertTrue(mModel.get(BookmarkFolderPickerProperties.MOVE_BUTTON_ENABLED));
    }

    @Test
    public void testOptionsItemSelected_BackPressed() {
        mMediator.optionsItemSelected(android.R.id.home);
        assertEquals(mMobileFolderItem.getTitle(),
                mModel.get(BookmarkFolderPickerProperties.TOOLBAR_TITLE));
        mMediator.optionsItemSelected(android.R.id.home);
        assertEquals("Move to…", mModel.get(BookmarkFolderPickerProperties.TOOLBAR_TITLE));
        mMediator.optionsItemSelected(android.R.id.home);
        verify(mFinishRunnable).run();
    }

    @Test
    public void testOptionsItemSelected_AddNewFolder() {
        mMediator.optionsItemSelected(R.id.create_new_folder_menu_id);
        verify(mAddNewFolderCoordinator).show(any());
    }
}
