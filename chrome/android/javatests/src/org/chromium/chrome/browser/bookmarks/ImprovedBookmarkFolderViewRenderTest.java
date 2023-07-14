// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

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
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.NightModeTestUtils.NightModeParams;

import java.io.IOException;
import java.util.List;

/**
 * Render tests for the improved bookmark row.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ImprovedBookmarkFolderViewRenderTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = new NightModeParams().getParameters();

    @Rule
    public final DisableAnimationsTestRule mDisableAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    CurrencyFormatter mFormatter;

    private ImprovedBookmarkFolderView mView;
    private PropertyModel mModel;
    private Bitmap mPrimaryBitmap;
    private Bitmap mSecondaryBitmap;
    private BitmapDrawable mPrimaryDrawable;
    private BitmapDrawable mSecondaryDrawable;
    private LinearLayout mContentView;

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
        mSecondaryDrawable = new BitmapDrawable(
                mActivityTestRule.getActivity().getResources(), mSecondaryBitmap);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContentView = new LinearLayout(mActivityTestRule.getActivity());
            mContentView.setBackgroundColor(Color.WHITE);

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            mActivityTestRule.getActivity().setContentView(mContentView, params);

            mView = (ImprovedBookmarkFolderView) LayoutInflater
                            .from(mActivityTestRule.getActivity())
                            .inflate(R.layout.improved_bookmark_folder_view_layout, null);
            mContentView.addView(mView);

            mModel = new PropertyModel(ImprovedBookmarkFolderViewProperties.ALL_KEYS);
            PropertyModelChangeProcessor.create(
                    mModel, mView, ImprovedBookmarkFolderViewBinder::bind);
            mModel.set(ImprovedBookmarkFolderViewProperties.FOLDER_CHILD_COUNT, 5);
        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNoImage() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ImprovedBookmarkFolderViewProperties.START_IMAGE_FOLDER_DRAWABLES,
                    new Pair<>(null, null));
            mModel.set(ImprovedBookmarkFolderViewProperties.START_ICON_DRAWABLE,
                    BookmarkUtils.getFolderIcon(mActivityTestRule.getActivity(),
                            BookmarkType.NORMAL, BookmarkRowDisplayPref.VISUAL));
            mModel.set(ImprovedBookmarkFolderViewProperties.START_AREA_BACKGROUND_COLOR,
                    ChromeColors.getSurfaceColor(
                            mActivityTestRule.getActivity(), R.dimen.default_elevation_1));
            mModel.set(ImprovedBookmarkFolderViewProperties.START_ICON_TINT,
                    AppCompatResources.getColorStateList(mActivityTestRule.getActivity(),
                            R.color.default_icon_color_secondary_tint_list));
        });
        mRenderTestRule.render(mContentView, "no_image");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNoImage_readingList() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ImprovedBookmarkFolderViewProperties.START_IMAGE_FOLDER_DRAWABLES,
                    new Pair<>(null, null));
            mModel.set(ImprovedBookmarkFolderViewProperties.START_ICON_DRAWABLE,
                    BookmarkUtils.getFolderIcon(mActivityTestRule.getActivity(),
                            BookmarkType.READING_LIST, BookmarkRowDisplayPref.VISUAL));
            mModel.set(ImprovedBookmarkFolderViewProperties.START_AREA_BACKGROUND_COLOR,
                    SemanticColorUtils.getColorPrimaryContainer(mActivityTestRule.getActivity()));
            mModel.set(ImprovedBookmarkFolderViewProperties.START_ICON_TINT,
                    ColorStateList.valueOf(SemanticColorUtils.getDefaultIconColorAccent1(
                            mActivityTestRule.getActivity())));
        });
        mRenderTestRule.render(mContentView, "no_image_reading_list");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testOneImage() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ImprovedBookmarkFolderViewProperties.START_IMAGE_FOLDER_DRAWABLES,
                    new Pair<>(mPrimaryDrawable, null));
        });
        mRenderTestRule.render(mContentView, "one_image");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTwoImages() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ImprovedBookmarkFolderViewProperties.START_IMAGE_FOLDER_DRAWABLES,
                    new Pair<>(mPrimaryDrawable, mSecondaryDrawable));
        });
        mRenderTestRule.render(mContentView, "two_images");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTwoImages_99Children() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ImprovedBookmarkFolderViewProperties.FOLDER_CHILD_COUNT, 99);
            mModel.set(ImprovedBookmarkFolderViewProperties.START_IMAGE_FOLDER_DRAWABLES,
                    new Pair<>(mPrimaryDrawable, mSecondaryDrawable));
        });
        mRenderTestRule.render(mContentView, "two_images_99_children");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTwoImages_999Children() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ImprovedBookmarkFolderViewProperties.FOLDER_CHILD_COUNT, 999);
            mModel.set(ImprovedBookmarkFolderViewProperties.START_IMAGE_FOLDER_DRAWABLES,
                    new Pair<>(mPrimaryDrawable, mSecondaryDrawable));
        });
        mRenderTestRule.render(mContentView, "two_images_999_children");
    }
}
