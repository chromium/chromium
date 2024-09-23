// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.doReturn;

import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.NightModeTestUtils.NightModeParams;

import java.io.IOException;
import java.util.List;

/** Render tests for the improved bookmark row. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ImprovedBookmarkFolderViewRenderTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = new NightModeParams().getParameters();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .setRevision(3)
                    .build();

    @Mock private CurrencyFormatter mFormatter;
    @Mock private BookmarkModel mBookmarkModel;

    private ImprovedBookmarkFolderView mView;
    private PropertyModel mModel;
    private Bitmap mPrimaryBitmap;
    private Bitmap mSecondaryBitmap;
    private BitmapDrawable mPrimaryDrawable;
    private BitmapDrawable mSecondaryDrawable;
    private LinearLayout mContentView;
    private ImprovedBookmarkFolderView mFolderView;

    public ImprovedBookmarkFolderViewRenderTest(boolean nightModeEnabled) {
        // Sets a fake background color to make the screenshots easier to compare with bare eyes.
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivityTestRule.getActivity().setTheme(R.style.Theme_BrowserUI_DayNight);

        mPrimaryBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mPrimaryBitmap.eraseColor(Color.GREEN);
        mPrimaryDrawable =
                new BitmapDrawable(mActivityTestRule.getActivity().getResources(), mPrimaryBitmap);

        mSecondaryBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mSecondaryBitmap.eraseColor(Color.RED);
        mSecondaryDrawable =
                new BitmapDrawable(
                        mActivityTestRule.getActivity().getResources(), mSecondaryBitmap);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new LinearLayout(mActivityTestRule.getActivity());
                    mContentView.setBackgroundColor(Color.WHITE);

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);
                    mActivityTestRule.getActivity().setContentView(mContentView, params);

                    ImprovedBookmarkRow row =
                            ImprovedBookmarkRow.buildView(mActivityTestRule.getActivity(), true);
                    mFolderView = row.getFolderView();
                    mContentView.addView(row);

                    mModel = new PropertyModel(ImprovedBookmarkRowProperties.ALL_KEYS);
                    PropertyModelChangeProcessor.create(
                            mModel, row, ImprovedBookmarkRowViewBinder::bind);
                    mModel.set(ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT, 5);
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT_TEXT_STYLE,
                            R.style.TextAppearance_RegularFolderChildCount);
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNoImage() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LazyOneshotSupplier<Pair<Drawable, Drawable>> imageSupplier =
                            LazyOneshotSupplier.fromSupplier(() -> new Pair<>(null, null));
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES,
                            imageSupplier);
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_ICON_DRAWABLE,
                            BookmarkUtils.getFolderIcon(
                                    mActivityTestRule.getActivity(),
                                    new BookmarkId(0, BookmarkType.NORMAL),
                                    mBookmarkModel,
                                    BookmarkRowDisplayPref.VISUAL));
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_AREA_BACKGROUND_COLOR,
                            ChromeColors.getSurfaceColor(
                                    mActivityTestRule.getActivity(), R.dimen.default_elevation_1));
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_ICON_TINT,
                            AppCompatResources.getColorStateList(
                                    mActivityTestRule.getActivity(),
                                    R.color.default_icon_color_secondary_tint_list));
                });
        mRenderTestRule.render(mFolderView, "no_image");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNoImage_bookmarksBar() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BookmarkId bookmarksBarId = new BookmarkId(1, BookmarkType.NORMAL);
                    doReturn(bookmarksBarId).when(mBookmarkModel).getDesktopFolderId();

                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES,
                            LazyOneshotSupplier.fromSupplier(() -> new Pair<>(null, null)));
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_ICON_DRAWABLE,
                            BookmarkUtils.getFolderIcon(
                                    mActivityTestRule.getActivity(),
                                    bookmarksBarId,
                                    mBookmarkModel,
                                    BookmarkRowDisplayPref.VISUAL));
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_AREA_BACKGROUND_COLOR,
                            SemanticColorUtils.getColorPrimaryContainer(
                                    mActivityTestRule.getActivity()));
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_ICON_TINT,
                            ColorStateList.valueOf(
                                    SemanticColorUtils.getDefaultIconColorAccent1(
                                            mActivityTestRule.getActivity())));
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT_TEXT_STYLE,
                            R.style.TextAppearance_SpecialFolderChildCount);
                });
        mRenderTestRule.render(mFolderView, "no_image_bookmarks_bar");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNoImage_readingList() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES,
                            LazyOneshotSupplier.fromSupplier(() -> new Pair<>(null, null)));
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_ICON_DRAWABLE,
                            BookmarkUtils.getFolderIcon(
                                    mActivityTestRule.getActivity(),
                                    new BookmarkId(0, BookmarkType.READING_LIST),
                                    mBookmarkModel,
                                    BookmarkRowDisplayPref.VISUAL));
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_AREA_BACKGROUND_COLOR,
                            SemanticColorUtils.getColorPrimaryContainer(
                                    mActivityTestRule.getActivity()));
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_ICON_TINT,
                            ColorStateList.valueOf(
                                    SemanticColorUtils.getDefaultIconColorAccent1(
                                            mActivityTestRule.getActivity())));
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT_TEXT_STYLE,
                            R.style.TextAppearance_SpecialFolderChildCount);
                });
        mRenderTestRule.render(mFolderView, "no_image_reading_list");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testOneImage() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LazyOneshotSupplier<Pair<Drawable, Drawable>> imageSupplier =
                            LazyOneshotSupplier.fromSupplier(
                                    () -> new Pair<>(mPrimaryDrawable, null));
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES,
                            imageSupplier);
                });
        mRenderTestRule.render(mFolderView, "one_image");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTwoImages() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LazyOneshotSupplier<Pair<Drawable, Drawable>> imageSupplier =
                            LazyOneshotSupplier.fromSupplier(
                                    () -> new Pair<>(mPrimaryDrawable, mSecondaryDrawable));
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES,
                            imageSupplier);
                });
        mRenderTestRule.render(mFolderView, "two_images");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTwoImages_99Children() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LazyOneshotSupplier<Pair<Drawable, Drawable>> imageSupplier =
                            LazyOneshotSupplier.fromSupplier(
                                    () -> new Pair<>(mPrimaryDrawable, mSecondaryDrawable));
                    mModel.set(ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT, 99);
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES,
                            imageSupplier);
                });
        mRenderTestRule.render(mFolderView, "two_images_99_children");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTwoImages_999Children() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LazyOneshotSupplier<Pair<Drawable, Drawable>> imageSupplier =
                            LazyOneshotSupplier.fromSupplier(
                                    () -> new Pair<>(mPrimaryDrawable, mSecondaryDrawable));
                    mModel.set(ImprovedBookmarkRowProperties.FOLDER_CHILD_COUNT, 999);
                    mModel.set(
                            ImprovedBookmarkRowProperties.FOLDER_START_IMAGE_FOLDER_DRAWABLES,
                            imageSupplier);
                });
        mRenderTestRule.render(mFolderView, "two_images_999_children");
    }
}
