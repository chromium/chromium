// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assume.assumeFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.util.TypedValue;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.TestThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.List;

/** Render tests for the {@link BookmarkFolderPickerMediator}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
@DisableFeatures({
    ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_LAYOUT,
    ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_DIALOG
})
public class BookmarkFolderPickerRenderTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(true, true).name("VisualRow_NightModeEnabled"),
                    new ParameterSet().value(true, false).name("VisualRow_NightModeDisabled"),
                    new ParameterSet().value(false, true).name("CompactRow_NightModeEnabled"),
                    new ParameterSet().value(false, false).name("CompactRow_NightModeDisabled"));

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(12)
                    .setDescription(
                            "Fix dark mode rendering and add desktop folder picker render tests")
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();

    private final boolean mUseVisualRowLayout;
    private final boolean mNightModeEnabled;

    @Mock private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock private Runnable mFinishRunnable;
    @Mock private Tracker mTracker;
    @Mock private BookmarkAddNewFolderCoordinator mAddNewFolderCoordinator;
    @Mock private BookmarkUiPrefs mBookmarkUiPrefs;
    @Mock private ShoppingService mShoppingService;

    private AppCompatActivity mActivity;
    private Context mThemedContext;
    private FrameLayout mContentView;
    private BookmarkFolderPickerCoordinator mCoordinator;
    private ImprovedBookmarkRowCoordinator mImprovedBookmarkRowCoordinator;
    private RecyclerView mRecyclerView;
    private FakeBookmarkModel mBookmarkModel;

    public BookmarkFolderPickerRenderTest(boolean useVisualRowLayout, boolean nightModeEnabled) {
        mUseVisualRowLayout = useVisualRowLayout;
        mNightModeEnabled = nightModeEnabled;
        mRenderTestRule.setVariantPrefix(mUseVisualRowLayout ? "visual_" : "compact_");
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        ImprovedBookmarkRow.setEnableIconAnimationForTesting(false);
        mBookmarkModel = runOnUiThreadBlocking(() -> FakeBookmarkModel.createModel());
        mBookmarkModel.setAreAccountBookmarkFoldersActive(false);

        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(mNightModeEnabled);
        BlankUiTestActivity.setTestTheme(R.style.Theme_BrowserUI_DayNight);
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();

        Configuration config = new Configuration(mActivity.getResources().getConfiguration());
        config.uiMode =
                (config.uiMode & ~Configuration.UI_MODE_NIGHT_MASK)
                        | (mNightModeEnabled
                                ? Configuration.UI_MODE_NIGHT_YES
                                : Configuration.UI_MODE_NIGHT_NO);
        // Production hosts the picker in BookmarkFolderPickerActivity, which the manifest themes
        // Theme.Chromium.DialogWhenLarge for both layouts. Inflate with the same theme.
        mThemedContext =
                new ContextThemeWrapper(
                        mActivity.createConfigurationContext(config),
                        R.style.Theme_Chromium_DialogWhenLarge);

        // Setup profile-related factories.
        TrackerFactory.setTrackerForTests(mTracker);

        // Setup BookmarkImageFetcher. Use the activity's Resources: BitmapDrawable takes its
        // intrinsic size from their DisplayMetrics, and createConfigurationContext() has its own.
        final Resources resources = mActivity.getResources();
        int bitmapSize = resources.getDimensionPixelSize(R.dimen.improved_bookmark_row_size);
        Bitmap primaryBitmap = Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        primaryBitmap.eraseColor(Color.GREEN);
        Bitmap secondaryBitmap =
                Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        secondaryBitmap.eraseColor(Color.RED);
        doCallback(
                        /* index= */ 2,
                        (Callback<Pair<Drawable, Drawable>> callback) ->
                                callback.onResult(
                                        new Pair<>(
                                                new BitmapDrawable(resources, primaryBitmap),
                                                new BitmapDrawable(resources, secondaryBitmap))))
                .when(mBookmarkImageFetcher)
                .fetchFirstTwoImagesForFolder(any(), anyInt(), any());

        // Setup BookmarkUiPrefs. Desktop ignores the visual pref and always falls back to compact
        // rows, see BookmarkUiPrefs#getBookmarkRowDisplayPref().
        boolean useVisualRow =
                mUseVisualRowLayout && !BookmarkUtils.isDesktopBookmarksDialogEnabled();
        doReturn(useVisualRow ? BookmarkRowDisplayPref.VISUAL : BookmarkRowDisplayPref.COMPACT)
                .when(mBookmarkUiPrefs)
                .getBookmarkRowDisplayPref();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new FrameLayout(mThemedContext);
                    mContentView.setBackgroundColor(
                            SemanticColorUtils.getDefaultBgColor(mThemedContext));

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT);
                    mActivity.setContentView(mContentView, params);

                    mImprovedBookmarkRowCoordinator =
                            new ImprovedBookmarkRowCoordinator(
                                    mThemedContext,
                                    mBookmarkImageFetcher,
                                    mBookmarkModel,
                                    mBookmarkUiPrefs,
                                    mShoppingService);
                });
    }

    @After
    public void tearDown() {
        // Static with no resetter, and @Batch(PER_CLASS) shares the process with later classes.
        BlankUiTestActivity.setTestTheme(0);
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    /**
     * Desktop always falls back to {@link BookmarkRowDisplayPref#COMPACT}, so the visual row layout
     * is unreachable there. Skip that parameterization so the desktop goldens aren't duplicated.
     */
    private void assumeDesktopUsesCompactRows() {
        assumeFalse(mUseVisualRowLayout);
    }

    void createCoordinatorToMoveBookmarkIds(BookmarkId... ids) throws Exception {
        runOnUiThreadBlocking(
                () -> {
                    mCoordinator =
                            new BookmarkFolderPickerCoordinator(
                                    mThemedContext,
                                    mBookmarkModel,
                                    Arrays.asList(ids),
                                    mFinishRunnable,
                                    mAddNewFolderCoordinator,
                                    mBookmarkUiPrefs,
                                    mImprovedBookmarkRowCoordinator,
                                    mShoppingService,
                                    /* isFromBookmarkDialog= */ false);

                    if (BookmarkUtils.isDesktopBookmarksDialogEnabled()) {
                        // The desktop layout has no background of its own; production draws it
                        // from the theme's windowBackground, which render tests don't capture
                        // because it lives on the DecorView.
                        TypedValue windowBackground = new TypedValue();
                        mThemedContext
                                .getTheme()
                                .resolveAttribute(
                                        android.R.attr.windowBackground, windowBackground, true);
                        mCoordinator.getView().setBackgroundResource(windowBackground.resourceId);
                    }
                    mContentView.addView(mCoordinator.getView());

                    Toolbar toolbar = mCoordinator.getToolbar();
                    if (toolbar != null) {
                        mActivity.setSupportActionBar(toolbar);
                        assumeNonNull(mActivity.getSupportActionBar())
                                .setDisplayHomeAsUpEnabled(true);
                    }

                    mRecyclerView = mContentView.findViewById(R.id.folder_recycler_view);
                });
    }

    /**
     * Waits for the folder row with image thumbnails at {@code position} in {@link #mRecyclerView}
     * to finish binding and rendering its start image/drawables.
     */
    private void waitForFolderRowImages(int position) {
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mRecyclerView);
        final boolean isVisualRow =
                mUseVisualRowLayout && !BookmarkUtils.isDesktopBookmarksDialogEnabled();
        CriteriaHelper.pollUiThread(
                () -> {
                    RecyclerView.ViewHolder viewHolder =
                            mRecyclerView.findViewHolderForAdapterPosition(position);
                    Criteria.checkThat("ViewHolder is null", viewHolder, Matchers.notNullValue());
                    final View itemView = viewHolder.itemView;
                    Criteria.checkThat(
                            "ViewHolder item is not ImprovedBookmarkRow",
                            itemView,
                            Matchers.instanceOf(ImprovedBookmarkRow.class));

                    if (isVisualRow) {
                        ImageView primaryImage = itemView.findViewById(R.id.primary_image);
                        Criteria.checkThat(
                                "Primary image view is null",
                                primaryImage,
                                Matchers.notNullValue());
                        Criteria.checkThat(
                                "Primary image is not visible",
                                primaryImage.getVisibility(),
                                Matchers.is(View.VISIBLE));
                        Criteria.checkThat(
                                "Primary image drawable is null",
                                primaryImage.getDrawable(),
                                Matchers.notNullValue());
                    } else {
                        ImageView startImage = itemView.findViewById(R.id.start_image);
                        Criteria.checkThat(
                                "Start image view is null", startImage, Matchers.notNullValue());
                        Criteria.checkThat(
                                "Start image is not visible",
                                startImage.getVisibility(),
                                Matchers.is(View.VISIBLE));
                        Criteria.checkThat(
                                "Start image drawable is null",
                                startImage.getDrawable(),
                                Matchers.notNullValue());
                    }
                });
    }

    /**
     * Waits for the folder rows to finish binding, then renders {@code view}. Some row properties
     * are applied by callbacks that the view binder posts to the looper, so drain it and wait for
     * the resulting layout pass before capturing.
     */
    private void waitForStableViewAndRender(View view, String id) throws Exception {
        RecyclerViewTestUtils.waitForStableMvcRecyclerView(mRecyclerView);
        TestThreadUtils.flushNonDelayedLooperTasks();
        ViewUtils.waitForStableView(view);
        mRenderTestRule.render(view, id);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testMoveBookmarkFromUserFolder() throws Exception {
        BookmarkId folderId =
                runOnUiThreadBlocking(
                        () ->
                                mBookmarkModel.addFolder(
                                        mBookmarkModel.getMobileFolderId(), 0, "user folder"));
        BookmarkId bookmarkId =
                runOnUiThreadBlocking(
                        () ->
                                mBookmarkModel.addBookmark(
                                        folderId,
                                        0,
                                        "user bookmark",
                                        new GURL("https://test.com")));
        createCoordinatorToMoveBookmarkIds(bookmarkId);

        waitForFolderRowImages(/* position= */ 0);
        waitForStableViewAndRender(mContentView, "move_bookmark_from_user_folder");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testMoveBookmarkFromMobileBookmarksShowsRoot() throws Exception {
        runOnUiThreadBlocking(
                () ->
                        mBookmarkModel.addFolder(
                                mBookmarkModel.getMobileFolderId(), 0, "user folder"));
        BookmarkId bookmarkId =
                runOnUiThreadBlocking(
                        () ->
                                mBookmarkModel.addBookmark(
                                        mBookmarkModel.getMobileFolderId(),
                                        0,
                                        "user bookmark",
                                        new GURL("https://test.com")));
        createCoordinatorToMoveBookmarkIds(bookmarkId);

        waitForStableViewAndRender(mContentView, "move_bookmark_from_mobile_bookmarks_shows_root");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testMoveBookmarkFromMobileBookmarksShowsRoot_withAccountFolders() throws Exception {
        mBookmarkModel.setAreAccountBookmarkFoldersActive(true);
        BookmarkId bookmarkId =
                runOnUiThreadBlocking(
                        () ->
                                mBookmarkModel.addBookmark(
                                        mBookmarkModel.getMobileFolderId(),
                                        0,
                                        "user bookmark",
                                        new GURL("https://test.com")));
        createCoordinatorToMoveBookmarkIds(bookmarkId);

        waitForStableViewAndRender(
                mContentView, "move_bookmark_from_mobile_bookmarks_shows_root_with_account");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_DIALOG)
    public void testMoveBookmarkFromUserFolder_Desktop() throws Exception {
        assumeDesktopUsesCompactRows();
        BookmarkId folderId =
                runOnUiThreadBlocking(
                        () ->
                                mBookmarkModel.addFolder(
                                        mBookmarkModel.getMobileFolderId(), 0, "user folder"));
        BookmarkId bookmarkId =
                runOnUiThreadBlocking(
                        () ->
                                mBookmarkModel.addBookmark(
                                        folderId,
                                        0,
                                        "user bookmark",
                                        new GURL("https://test.com")));
        createCoordinatorToMoveBookmarkIds(bookmarkId);

        waitForFolderRowImages(/* position= */ 0);
        waitForStableViewAndRender(
                mCoordinator.getView(), "move_bookmark_from_user_folder_desktop");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_DIALOG)
    public void testMoveBookmarkFromMobileBookmarksShowsRoot_Desktop() throws Exception {
        assumeDesktopUsesCompactRows();
        runOnUiThreadBlocking(
                () ->
                        mBookmarkModel.addFolder(
                                mBookmarkModel.getMobileFolderId(), 0, "user folder"));
        BookmarkId bookmarkId =
                runOnUiThreadBlocking(
                        () ->
                                mBookmarkModel.addBookmark(
                                        mBookmarkModel.getMobileFolderId(),
                                        0,
                                        "user bookmark",
                                        new GURL("https://test.com")));
        createCoordinatorToMoveBookmarkIds(bookmarkId);

        waitForStableViewAndRender(
                mCoordinator.getView(), "move_bookmark_from_mobile_bookmarks_shows_root_desktop");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.ANDROID_DESKTOP_BOOKMARK_DIALOG)
    public void testMoveBookmarkFromMobileBookmarksShowsRoot_withAccountFolders_Desktop()
            throws Exception {
        assumeDesktopUsesCompactRows();
        mBookmarkModel.setAreAccountBookmarkFoldersActive(true);
        BookmarkId bookmarkId =
                runOnUiThreadBlocking(
                        () ->
                                mBookmarkModel.addBookmark(
                                        mBookmarkModel.getMobileFolderId(),
                                        0,
                                        "user bookmark",
                                        new GURL("https://test.com")));
        createCoordinatorToMoveBookmarkIds(bookmarkId);

        waitForStableViewAndRender(
                mCoordinator.getView(),
                "move_bookmark_from_mobile_bookmarks_shows_root_with_account_desktop");
    }
}
