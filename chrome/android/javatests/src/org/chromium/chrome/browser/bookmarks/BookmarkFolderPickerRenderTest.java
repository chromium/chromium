// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.RecyclerView;
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
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.url.JUnitTestGURLs;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;

/** Render tests for the {@link BookmarkFolderPickerMediator}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class BookmarkFolderPickerRenderTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(true, true).name("VisualRow_NightModeEnabled"),
                    new ParameterSet().value(true, false).name("VisualRow_NightModeDisabled"),
                    new ParameterSet().value(false, true).name("CompactRow_NightModeEnabled"),
                    new ParameterSet().value(false, false).name("CompactRow_NightModeDisabled"));

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
                    .setRevision(1)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();

    // Initial structure:
    // Root
    //  Mobile
    //   UserFolder
    //    UserBookmark
    //   UserFolder2
    private final BookmarkId mRootFolderId = new BookmarkId(/*id=*/1, BookmarkType.NORMAL);
    private final BookmarkId mDesktopFolderId = new BookmarkId(/*id=*/2, BookmarkType.NORMAL);
    private final BookmarkId mMobileFolderId = new BookmarkId(/*id=*/3, BookmarkType.NORMAL);
    private final BookmarkId mOtherFolderId = new BookmarkId(/*id=*/5, BookmarkType.NORMAL);
    private final BookmarkId mReadingListFolderId =
            new BookmarkId(/*id=*/6, BookmarkType.READING_LIST);
    private final BookmarkId mUserFolderId = new BookmarkId(/*id=*/7, BookmarkType.NORMAL);
    private final BookmarkId mUserBookmarkId = new BookmarkId(/*id=*/8, BookmarkType.NORMAL);
    private final BookmarkId mUserFolderId2 = new BookmarkId(/*id=*/9, BookmarkType.NORMAL);

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
    private final BookmarkItem mUserBookmarkItem = new BookmarkItem(mUserBookmarkId, "UserBookmark",
            JUnitTestGURLs.EXAMPLE_URL, false, mUserFolderId, true, false, 0, false, 0);
    private final BookmarkItem mUserFolderItem2 = new BookmarkItem(
            mUserFolderId2, "UserFolder2", null, true, mMobileFolderId, false, false, 0, false, 0);

    @Mock
    private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private Runnable mFinishRunnable;
    @Mock
    private Profile mProfile;
    @Mock
    private Tracker mTracker;
    @Mock
    private BookmarkAddNewFolderCoordinator mAddNewFolderCoordinator;
    @Mock
    private BookmarkUiPrefs mBookmarkUiPrefs;
    @Mock
    private ShoppingService mShoppingService;

    private final boolean mUseVisualRowLayout;

    private AppCompatActivity mActivity;
    private FrameLayout mContentView;
    private BookmarkFolderPickerCoordinator mCoordinator;
    private ImprovedBookmarkRowCoordinator mImprovedBookmarkRowCoordinator;
    private RecyclerView mRecyclerView;

    public BookmarkFolderPickerRenderTest(boolean useVisualRowLayout, boolean nightModeEnabled) {
        mUseVisualRowLayout = useVisualRowLayout;
        mRenderTestRule.setVariantPrefix(mUseVisualRowLayout ? "visual_" : "compact_");

        // Sets a fake background color to make the screenshots easier to compare with bare eyes.
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        mActivityTestRule.getActivity().setTheme(R.style.Theme_BrowserUI_DayNight);

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
        doReturn(Arrays.asList(
                         mDesktopFolderId, mMobileFolderId, mOtherFolderId, mReadingListFolderId))
                .when(mBookmarkModel)
                .getTopLevelFolderIds();
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
        // User folders/bookmarks
        doReturn(mUserFolderItem).when(mBookmarkModel).getBookmarkById(mUserFolderId);
        doReturn(Arrays.asList(mUserBookmarkId)).when(mBookmarkModel).getChildIds(mUserFolderId);
        doReturn(1).when(mBookmarkModel).getTotalBookmarkCount(mUserFolderId);
        doReturn(mUserFolderItem2).when(mBookmarkModel).getBookmarkById(mUserFolderId2);
        doReturn(Arrays.asList()).when(mBookmarkModel).getChildIds(mUserFolderId2);
        doReturn(0).when(mBookmarkModel).getTotalBookmarkCount(mUserFolderId2);
        doReturn(mUserBookmarkItem).when(mBookmarkModel).getBookmarkById(mUserBookmarkId);
        doReturn(true).when(mBookmarkModel).doesBookmarkExist(any());
        doCallback((Runnable r) -> r.run()).when(mBookmarkModel).finishLoadingBookmarkModel(any());

        // Setup BookmarkImageFetcher
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
                    new Pair<>(new BitmapDrawable(mActivity.getResources(), primaryBitmap),
                            new BitmapDrawable(mActivity.getResources(), secondaryBitmap)));
            return null;
        })
                .when(mBookmarkImageFetcher)
                .fetchFirstTwoImagesForFolder(any(), any());

        // Setup BookmarkUiPrefs.
        doReturn(mUseVisualRowLayout ? BookmarkRowDisplayPref.VISUAL
                                     : BookmarkRowDisplayPref.COMPACT)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowDisplayPref();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContentView = new FrameLayout(mActivityTestRule.getActivity());

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            mActivityTestRule.getActivity().setContentView(mContentView, params);

            mImprovedBookmarkRowCoordinator = new ImprovedBookmarkRowCoordinator(
                    mActivityTestRule.getActivity(), mBookmarkImageFetcher, mBookmarkModel,
                    mBookmarkUiPrefs, mShoppingService);

            mCoordinator = new BookmarkFolderPickerCoordinator(mActivity, mBookmarkModel,
                    mBookmarkImageFetcher, Arrays.asList(mUserBookmarkId), mFinishRunnable,
                    mAddNewFolderCoordinator, mBookmarkUiPrefs, mImprovedBookmarkRowCoordinator,
                    mShoppingService);
            mContentView.addView(mCoordinator.getView());

            Toolbar toolbar = (Toolbar) mContentView.findViewById(R.id.toolbar);
            mActivity.setSupportActionBar(toolbar);
            mActivity.getSupportActionBar().setDisplayHomeAsUpEnabled(true);

            mRecyclerView = mContentView.findViewById(R.id.folder_recycler_view);
        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testMoveBookmarkFromUserFolder() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mCoordinator.openFolderForTesting(mUserFolderId); });
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mRecyclerView);
        mRenderTestRule.render(mContentView, "move_bookmark_from_user_folder");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testMoveBookmarkFromMobileBookmarks() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mCoordinator.openFolderForTesting(mMobileFolderId); });
        onView(withText(mUserFolderItem.getTitle()));
        onView(withText(mUserFolderItem2.getTitle()));
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mRecyclerView);
        mRenderTestRule.render(mContentView, "move_bookmark_from_mobile_bookmarks");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testMoveBookmarkFromRoot() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mCoordinator.openFolderForTesting(mRootFolderId); });
        CriteriaHelper.pollUiThread(() -> mRecyclerView.getAdapter().getItemCount() == 4);
        onView(withText(mDesktopFolderItem.getTitle()));
        onView(withText(mReadingListFolderItem.getTitle()));
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mRecyclerView);
        mRenderTestRule.render(mContentView, "move_bookmark_from_root");
    }
}
