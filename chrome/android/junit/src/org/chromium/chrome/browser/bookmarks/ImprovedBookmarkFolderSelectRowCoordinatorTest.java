// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.LayoutInflater;

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
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;

/** Unit tests for {@link ImprovedBookmarkFolderSelectRowCoordinator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImprovedBookmarkFolderSelectRowCoordinatorTest {
    private static final int CHILD_COUNT = 5;
    private static final int READING_LIST_CHILD_COUNT = 1;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public final ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private final BookmarkId mFolderId = new BookmarkId(/*id=*/1, BookmarkType.NORMAL);
    private final BookmarkId mReadingListFolderId =
            new BookmarkId(/*id=*/2, BookmarkType.READING_LIST);
    private final BookmarkId mBookmarkId = new BookmarkId(/*id=*/3, BookmarkType.NORMAL);
    private final BookmarkId mReadingListId = new BookmarkId(/*id=*/4, BookmarkType.READING_LIST);

    private final BookmarkItem mFolderItem =
            new BookmarkItem(mFolderId, "User folder", null, true, null, true, false, 0, false, 0);
    private final BookmarkItem mBookmarkItem = new BookmarkItem(mBookmarkId, "Bookmark",
            JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), false, mFolderId, true, false, 0,
            false, 0);
    private final BookmarkItem mReadingListFolderItem = new BookmarkItem(
            mReadingListFolderId, "Reading List", null, true, null, true, false, 0, false, 0);
    private final BookmarkItem mReadingListItem = new BookmarkItem(mReadingListId, "ReadingList",
            JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), false, mReadingListFolderId, true,
            false, 0, false, 0);

    @Mock
    private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private Drawable mDrawable;
    @Mock
    private Runnable mClickListener;

    private Activity mActivity;
    private ImprovedBookmarkFolderSelectRow mView;
    private ImprovedBookmarkRow mImprovedBookmarkRow;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        mView = spy((ImprovedBookmarkFolderSelectRow) LayoutInflater.from(mActivity).inflate(
                R.layout.improved_bookmark_folder_select_layout, null));

        // Setup BookmarkModel.
        doReturn(mBookmarkItem).when(mBookmarkModel).getBookmarkById(mBookmarkId);
        doReturn(mReadingListFolderItem).when(mBookmarkModel).getBookmarkById(mReadingListFolderId);
        doReturn(mFolderItem).when(mBookmarkModel).getBookmarkById(mFolderId);
        doReturn(mReadingListItem).when(mBookmarkModel).getBookmarkById(mReadingListId);
        doReturn(CHILD_COUNT).when(mBookmarkModel).getChildCount(mFolderId);
        doReturn(READING_LIST_CHILD_COUNT).when(mBookmarkModel).getChildCount(mReadingListFolderId);

        // Setup BookmarkImageFetcher.
        doCallback(1,
                (Callback<Pair<Drawable, Drawable>> callback)
                        -> callback.onResult(new Pair<>(mDrawable, mDrawable)))
                .when(mBookmarkImageFetcher)
                .fetchFirstTwoImagesForFolder(any(), any());
    }

    @Test
    public void testConstructor_withImages() {
        ImprovedBookmarkFolderSelectRowCoordinator coordinator =
                new ImprovedBookmarkFolderSelectRowCoordinator(
                        mActivity, mBookmarkImageFetcher, mBookmarkModel, mClickListener);
        coordinator.setBookmarkId(mFolderId);
        coordinator.setView(mView);
        PropertyModel model = coordinator.getModel();

        assertEquals("User folder", model.get(ImprovedBookmarkFolderSelectRowProperties.TITLE));
        assertTrue(model.get(ImprovedBookmarkFolderSelectRowProperties.END_ICON_VISIBLE));

        verify(mView).setTitle("User folder");
        verify(mView).setEndIconVisible(true);
        verify(mView).setRowClickListener(any());
    }

    @Test
    public void testConstructor_readingList() {
        doReturn(Arrays.asList(mReadingListFolderId))
                .when(mBookmarkModel)
                .getTopLevelFolderIds(anyBoolean(), anyBoolean());
        ImprovedBookmarkFolderSelectRowCoordinator coordinator =
                new ImprovedBookmarkFolderSelectRowCoordinator(
                        mActivity, mBookmarkImageFetcher, mBookmarkModel, mClickListener);
        coordinator.setBookmarkId(mReadingListFolderId);
        coordinator.setView(mView);
        PropertyModel model = coordinator.getModel();

        assertEquals("Reading List", model.get(ImprovedBookmarkFolderSelectRowProperties.TITLE));
    }
}
