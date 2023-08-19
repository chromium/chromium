// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.util.Pair;

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
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ImprovedBookmarkFolderViewCoordinator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImprovedBookmarkFolderViewCoordinatorTest {
    private static final int FOLDER_CHILD_COUNT = 10;
    private static final int UNREAD_CHILD_COUNT = 10;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private final BookmarkId mFolderId = new BookmarkId(/*id=*/1, BookmarkType.NORMAL);
    private final BookmarkId mReadingListFolderId =
            new BookmarkId(/*id=*/2, BookmarkType.READING_LIST);
    private final BookmarkItem mFolderItem =
            new BookmarkItem(mFolderId, "test folder", null, true, null, true, false, 0, false, 0);
    private final BookmarkItem mReadingListFolderItem = new BookmarkItem(
            mReadingListFolderId, "Reading List", null, true, null, true, false, 0, false, 0);

    @Mock
    private ImprovedBookmarkFolderView mView;
    @Mock
    private ImprovedBookmarkFolderView mSubstitueView;
    @Mock
    private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private Drawable mDrawable;

    private Activity mActivity;
    private ImprovedBookmarkFolderViewCoordinator mCoordinator;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        // Setup BookmarkModel.
        doReturn(mFolderItem).when(mBookmarkModel).getBookmarkById(mFolderId);
        doReturn(FOLDER_CHILD_COUNT).when(mBookmarkModel).getTotalBookmarkCount(mFolderId);
        doReturn(mReadingListFolderItem).when(mBookmarkModel).getBookmarkById(mReadingListFolderId);
        doReturn(UNREAD_CHILD_COUNT).when(mBookmarkModel).getUnreadCount(mReadingListFolderId);

        // Setup BookmarkImageFetcher.
        doCallback(1,
                (Callback<Pair<Drawable, Drawable>> callback)
                        -> callback.onResult(new Pair<>(mDrawable, mDrawable)))
                .when(mBookmarkImageFetcher)
                .fetchFirstTwoImagesForFolder(any(), any());
        createCoordinator();
    }

    private void createCoordinator() {
        mCoordinator = new ImprovedBookmarkFolderViewCoordinator(
                mActivity, mBookmarkImageFetcher, mBookmarkModel);
        mModel = mCoordinator.getModelForTesting();
    }

    @Test
    public void testSetView() {
        mCoordinator.setBookmarkId(mFolderId);
        mCoordinator.setView(mView);
        assertNotNull(mModel.get(ImprovedBookmarkFolderViewProperties.START_AREA_BACKGROUND_COLOR));
        assertNotNull(mModel.get(ImprovedBookmarkFolderViewProperties.START_ICON_TINT));
        assertNotNull(mModel.get(ImprovedBookmarkFolderViewProperties.START_ICON_DRAWABLE));
        assertEquals(FOLDER_CHILD_COUNT,
                mModel.get(ImprovedBookmarkFolderViewProperties.FOLDER_CHILD_COUNT));
        assertEquals(new Pair<>(mDrawable, mDrawable),
                mModel.get(ImprovedBookmarkFolderViewProperties.START_IMAGE_FOLDER_DRAWABLES));
    }

    @Test
    public void testSetView_readingList() {
        mCoordinator.setBookmarkId(mReadingListFolderId);
        mCoordinator.setView(mView);
        assertNotNull(mModel.get(ImprovedBookmarkFolderViewProperties.START_AREA_BACKGROUND_COLOR));
        assertNotNull(mModel.get(ImprovedBookmarkFolderViewProperties.START_ICON_TINT));
        assertNotNull(mModel.get(ImprovedBookmarkFolderViewProperties.START_ICON_DRAWABLE));
        assertEquals(UNREAD_CHILD_COUNT,
                mModel.get(ImprovedBookmarkFolderViewProperties.FOLDER_CHILD_COUNT));
        assertEquals(new Pair<>(mDrawable, mDrawable),
                mModel.get(ImprovedBookmarkFolderViewProperties.START_IMAGE_FOLDER_DRAWABLES));
    }

    @Test
    public void testSetView_noImages() {
        doReturn(mFolderId).when(mBookmarkModel).getDesktopFolderId();
        createCoordinator();
        mCoordinator.setBookmarkId(mFolderId);

        mCoordinator.setView(mView);
        assertEquals(new Pair<>(null, null),
                mModel.get(ImprovedBookmarkFolderViewProperties.START_IMAGE_FOLDER_DRAWABLES));
    }

    @Test
    public void testSetView_rebindView() {
        mCoordinator.setView(mView);
        mCoordinator.setBookmarkId(mFolderId);
        assertEquals(mView, mCoordinator.getViewForTesting());

        mCoordinator.setView(mSubstitueView);
        assertEquals(mSubstitueView, mCoordinator.getViewForTesting());
    }
}
