// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;

/** Unit tests for {@link ImprovedBookmarkFolderSelectRowCoordinator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImprovedBookmarkFolderSelectRowCoordinatorTest {
    private static final String TITLE = "Test title";
    private static final String READING_LIST_TITLE = "Reading list";
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
    private final BookmarkItem mBookmarkItem = new BookmarkItem(mBookmarkId, "Bookmark",
            JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), false, mFolderId, true, false, 0,
            false);
    private final BookmarkItem mReadingListItem = new BookmarkItem(mReadingListId, "ReadingList",
            JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL), false, mReadingListFolderId, true,
            false, 0, false);

    @Mock
    private ImprovedBookmarkFolderSelectRow mView;
    @Mock
    private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private Drawable mDrawable;

    private Activity mActivity;
    private ImprovedBookmarkRow mImprovedBookmarkRow;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        // Setup BookmarkModel.
        doReturn(mBookmarkItem).when(mBookmarkModel).getBookmarkById(mBookmarkId);
        doReturn(mReadingListItem).when(mBookmarkModel).getBookmarkById(mReadingListId);
        doReturn(TITLE).when(mBookmarkModel).getBookmarkTitle(mFolderId);
        doReturn(READING_LIST_TITLE).when(mBookmarkModel).getBookmarkTitle(mReadingListFolderId);
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
                        mActivity, mView, mBookmarkImageFetcher, mFolderId, mBookmarkModel);
        PropertyModel model = coordinator.getModel();

        assertEquals(TITLE, model.get(ImprovedBookmarkFolderSelectRowProperties.TITLE));
        assertEquals(CHILD_COUNT,
                model.get(ImprovedBookmarkFolderSelectRowProperties.FOLDER_CHILD_COUNT));
        assertNotNull(
                model.get(ImprovedBookmarkFolderSelectRowProperties.START_AREA_BACKGROUND_COLOR));
        assertNotNull(model.get(ImprovedBookmarkFolderSelectRowProperties.START_ICON_DRAWABLE));
        assertNotNull(model.get(ImprovedBookmarkFolderSelectRowProperties.START_ICON_TINT));
        assertTrue(model.get(ImprovedBookmarkFolderSelectRowProperties.END_ICON_VISIBLE));
        assertEquals(new Pair<>(mDrawable, mDrawable),
                model.get(ImprovedBookmarkFolderSelectRowProperties.START_IMAGE_FOLDER_DRAWABLES));

        verify(mView).setTitle(TITLE);
        verify(mView).setStartAreaBackgroundColor(anyInt());
        verify(mView).setStartIconDrawable(any());
        verify(mView).setStartIconTint(any());
        verify(mView, times(2)).setStartImageDrawables(any(), any());
        verify(mView).setChildCount(CHILD_COUNT);
        verify(mView).setEndIconVisible(true);
        verify(mView).setRowClickListener(any());
    }

    @Test
    public void testConstructor_noImages() {
        MockitoHelper
                .doCallback(1,
                        (Callback<Pair<Drawable, Drawable>> callback)
                                -> callback.onResult(new Pair<>(null, null)))
                .when(mBookmarkImageFetcher)
                .fetchFirstTwoImagesForFolder(any(), any());
        ImprovedBookmarkFolderSelectRowCoordinator coordinator =
                new ImprovedBookmarkFolderSelectRowCoordinator(
                        mActivity, mView, mBookmarkImageFetcher, mFolderId, mBookmarkModel);
        PropertyModel model = coordinator.getModel();
        assertEquals(new Pair<>(null, null),
                model.get(ImprovedBookmarkFolderSelectRowProperties.START_IMAGE_FOLDER_DRAWABLES));
    }

    @Test
    public void testConstructor_bookmarksBarFolder() {
        doReturn(Arrays.asList(mFolderId))
                .when(mBookmarkModel)
                .getTopLevelFolderIds(anyBoolean(), anyBoolean());
        ImprovedBookmarkFolderSelectRowCoordinator coordinator =
                new ImprovedBookmarkFolderSelectRowCoordinator(
                        mActivity, mView, mBookmarkImageFetcher, mFolderId, mBookmarkModel);
        PropertyModel model = coordinator.getModel();
        assertEquals(new Pair<>(null, null),
                model.get(ImprovedBookmarkFolderSelectRowProperties.START_IMAGE_FOLDER_DRAWABLES));
    }

    @Test
    public void testConstructor_readingList() {
        doReturn(Arrays.asList(mReadingListFolderId))
                .when(mBookmarkModel)
                .getTopLevelFolderIds(anyBoolean(), anyBoolean());
        ImprovedBookmarkFolderSelectRowCoordinator coordinator =
                new ImprovedBookmarkFolderSelectRowCoordinator(mActivity, mView,
                        mBookmarkImageFetcher, mReadingListFolderId, mBookmarkModel);
        PropertyModel model = coordinator.getModel();

        assertEquals(
                READING_LIST_TITLE, model.get(ImprovedBookmarkFolderSelectRowProperties.TITLE));
        assertEquals(READING_LIST_CHILD_COUNT,
                model.get(ImprovedBookmarkFolderSelectRowProperties.FOLDER_CHILD_COUNT));
        assertEquals(new Pair<>(null, null),
                model.get(ImprovedBookmarkFolderSelectRowProperties.START_IMAGE_FOLDER_DRAWABLES));
    }
}
