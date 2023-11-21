// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.DESKTOP_BOOKMARK_ITEM;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.FOLDER_ITEM_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.MOBILE_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.READING_LIST_ITEM;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.ROOT_BOOKMARK_ID;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_BOOKMARK_ID_A;
import static org.chromium.chrome.browser.bookmarks.SharedBookmarkModelMocks.URL_ITEM_A;
import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.content.res.Resources;
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
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.NightModeTestUtils;

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
            Arrays.asList(
                    new ParameterSet().value(true, true).name("VisualRow_NightModeEnabled"),
                    new ParameterSet().value(true, false).name("VisualRow_NightModeDisabled"),
                    new ParameterSet().value(false, true).name("CompactRow_NightModeEnabled"),
                    new ParameterSet().value(false, false).name("CompactRow_NightModeDisabled"));

    @Rule
    public final DisableAnimationsTestRule mDisableAnimationsRule = new DisableAnimationsTestRule();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();

    private final boolean mUseVisualRowLayout;

    @Mock private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock private BookmarkModel mBookmarkModel;
    @Mock private Runnable mFinishRunnable;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private BookmarkAddNewFolderCoordinator mAddNewFolderCoordinator;
    @Mock private BookmarkUiPrefs mBookmarkUiPrefs;
    @Mock private ShoppingService mShoppingService;

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
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        // Setup profile-related factories.
        Profile.setLastUsedProfileForTesting(mProfile);
        TrackerFactory.setTrackerForTests(mTracker);

        SharedBookmarkModelMocks.initMocks(mBookmarkModel);

        // Setup BookmarkImageFetcher.
        final Resources resources = mActivity.getResources();
        int bitmapSize = resources.getDimensionPixelSize(R.dimen.improved_bookmark_row_size);
        Bitmap primaryBitmap = Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        primaryBitmap.eraseColor(Color.GREEN);
        Bitmap secondaryBitmap =
                Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        secondaryBitmap.eraseColor(Color.RED);
        doCallback(
                        /* index= */ 1,
                        (Callback<Pair<Drawable, Drawable>> callback) ->
                                callback.onResult(
                                        new Pair<>(
                                                new BitmapDrawable(resources, primaryBitmap),
                                                new BitmapDrawable(resources, secondaryBitmap))))
                .when(mBookmarkImageFetcher)
                .fetchFirstTwoImagesForFolder(any(), any());

        // Setup BookmarkUiPrefs.
        doReturn(
                        mUseVisualRowLayout
                                ? BookmarkRowDisplayPref.VISUAL
                                : BookmarkRowDisplayPref.COMPACT)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowDisplayPref();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new FrameLayout(mActivity);

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT);
                    mActivity.setContentView(mContentView, params);

                    mImprovedBookmarkRowCoordinator =
                            new ImprovedBookmarkRowCoordinator(
                                    mActivity,
                                    mBookmarkImageFetcher,
                                    mBookmarkModel,
                                    mBookmarkUiPrefs,
                                    mShoppingService);
                    mCoordinator =
                            new BookmarkFolderPickerCoordinator(
                                    mActivity,
                                    mBookmarkModel,
                                    Arrays.asList(URL_BOOKMARK_ID_A),
                                    mFinishRunnable,
                                    mAddNewFolderCoordinator,
                                    mBookmarkUiPrefs,
                                    mImprovedBookmarkRowCoordinator,
                                    mShoppingService);
                    mContentView.addView(mCoordinator.getView());

                    Toolbar toolbar = mContentView.findViewById(R.id.toolbar);
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
                () -> mCoordinator.openFolderForTesting(FOLDER_BOOKMARK_ID_A));
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mRecyclerView);
        mRenderTestRule.render(mContentView, "move_bookmark_from_user_folder");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testMoveBookmarkFromMobileBookmarks() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mCoordinator.openFolderForTesting(MOBILE_BOOKMARK_ID));
        onView(withText(FOLDER_ITEM_A.getTitle()));
        onView(withText(URL_ITEM_A.getTitle()));
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mRecyclerView);
        mRenderTestRule.render(mContentView, "move_bookmark_from_mobile_bookmarks");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testMoveBookmarkFromRoot() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mCoordinator.openFolderForTesting(ROOT_BOOKMARK_ID));
        CriteriaHelper.pollUiThread(() -> mRecyclerView.getAdapter().getItemCount() == 4);
        onView(withText(DESKTOP_BOOKMARK_ITEM.getTitle()));
        onView(withText(READING_LIST_ITEM.getTitle()));
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mRecyclerView);
        mRenderTestRule.render(mContentView, "move_bookmark_from_root");
    }
}
