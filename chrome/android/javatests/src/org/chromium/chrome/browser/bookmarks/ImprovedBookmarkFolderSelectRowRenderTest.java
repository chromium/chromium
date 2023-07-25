// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.NightModeTestUtils.NightModeParams;
import org.chromium.url.JUnitTestGURLs;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Render tests for the {@link ImprovedBookmarkFolderSelectRow}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class ImprovedBookmarkFolderSelectRowRenderTest {
    private static final String TITLE = "Test title";
    private static final String READING_LIST_TITLE = "Reading list";
    private static final int CHILD_COUNT = 5;
    private static final int READING_LIST_CHILD_COUNT = 1;

    @ParameterAnnotations.ClassParameter
    private static final List<ParameterSet> CLASS_PARAMS = new NightModeParams().getParameters();

    @Rule
    public final DisableAnimationsTestRule mDisableAnimationsRule = new DisableAnimationsTestRule();
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);
    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();

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
    private Runnable mRunnable;

    private LinearLayout mContentView;
    private ImprovedBookmarkFolderSelectRow mView;

    public ImprovedBookmarkFolderSelectRowRenderTest(boolean nightModeEnabled) {
        // Sets a fake background color to make the screenshots easier to compare with bare eyes.
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivityTestRule.getActivity().setTheme(R.style.Theme_BrowserUI_DayNight);

        // Setup BookmarkModel.
        doReturn(mBookmarkItem).when(mBookmarkModel).getBookmarkById(mBookmarkId);
        doReturn(mReadingListFolderItem).when(mBookmarkModel).getBookmarkById(mReadingListFolderId);
        doReturn(mFolderItem).when(mBookmarkModel).getBookmarkById(mFolderId);
        doReturn(mReadingListItem).when(mBookmarkModel).getBookmarkById(mReadingListId);
        doReturn(CHILD_COUNT).when(mBookmarkModel).getChildCount(mFolderId);
        doReturn(READING_LIST_CHILD_COUNT).when(mBookmarkModel).getChildCount(mReadingListFolderId);

        // Setup BookmarkImageFetcher.
        int bitmapSize = mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.improved_bookmark_row_size);
        Bitmap primaryBitmap = Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        primaryBitmap.eraseColor(Color.GREEN);
        Bitmap secondaryBitmap =
                Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        secondaryBitmap.eraseColor(Color.RED);
        doAnswer((invocation) -> {
            Callback<Pair<Drawable, Drawable>> callback = invocation.getArgument(1);
            callback.onResult(
                    new Pair<>(new BitmapDrawable(mActivityTestRule.getActivity().getResources(),
                                       primaryBitmap),
                            new BitmapDrawable(mActivityTestRule.getActivity().getResources(),
                                    secondaryBitmap)));
            return null;
        })
                .when(mBookmarkImageFetcher)
                .fetchFirstTwoImagesForFolder(any(), any());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContentView = new LinearLayout(mActivityTestRule.getActivity());
            mContentView.setOrientation(LinearLayout.VERTICAL);
            mContentView.setBackgroundColor(Color.WHITE);

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            mActivityTestRule.getActivity().setContentView(mContentView, params);

            mView = (ImprovedBookmarkFolderSelectRow) LayoutInflater
                            .from(mActivityTestRule.getActivity())
                            .inflate(R.layout.improved_bookmark_folder_select_layout, null);
            mContentView.addView(mView);
        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNormal() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ImprovedBookmarkFolderSelectRowCoordinator coordinator =
                    new ImprovedBookmarkFolderSelectRowCoordinator(mActivityTestRule.getActivity(),
                            mBookmarkImageFetcher, mBookmarkModel, mRunnable);
            coordinator.setBookmarkId(mFolderId);
            coordinator.setView(mView);
        });
        mRenderTestRule.render(mContentView, "normal");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTopLevelFolder() throws IOException {
        doReturn(Arrays.asList(mFolderId))
                .when(mBookmarkModel)
                .getTopLevelFolderIds(anyBoolean(), anyBoolean());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ImprovedBookmarkFolderSelectRowCoordinator coordinator =
                    new ImprovedBookmarkFolderSelectRowCoordinator(mActivityTestRule.getActivity(),
                            mBookmarkImageFetcher, mBookmarkModel, mRunnable);
            coordinator.setBookmarkId(mFolderId);
            coordinator.setView(mView);
        });
        mRenderTestRule.render(mContentView, "top_level_folder");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testReadingList() throws IOException {
        doReturn(Arrays.asList(mReadingListFolderId))
                .when(mBookmarkModel)
                .getTopLevelFolderIds(anyBoolean(), anyBoolean());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ImprovedBookmarkFolderSelectRowCoordinator coordinator =
                    new ImprovedBookmarkFolderSelectRowCoordinator(mActivityTestRule.getActivity(),
                            mBookmarkImageFetcher, mBookmarkModel, mRunnable);
            coordinator.setBookmarkId(mReadingListFolderId);
            coordinator.setView(mView);
        });
        mRenderTestRule.render(mContentView, "reading_list");
    }
}
