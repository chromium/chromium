// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkSaveFlowProperties.FolderText;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
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
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class ImprovedBookmarkSaveFlowRenderTest {
    @ClassParameter
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
                    .setRevision(1)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();

    private Bitmap mBitmap;
    private ImprovedBookmarkSaveFlowView mView;
    private LinearLayout mContentView;
    private PropertyModel mModel;

    public ImprovedBookmarkSaveFlowRenderTest(boolean nightModeEnabled) {
        // Sets a fake background color to make the screenshots easier to compare with bare eyes.
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivityTestRule.getActivity().setTheme(R.style.Theme_BrowserUI_DayNight);

        int bitmapSize = mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                R.dimen.improved_bookmark_save_flow_image_size);
        mBitmap = Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        mBitmap.eraseColor(Color.GREEN);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContentView = new LinearLayout(mActivityTestRule.getActivity());
            mContentView.setBackgroundColor(Color.WHITE);

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            mActivityTestRule.getActivity().setContentView(mContentView, params);

            LayoutInflater.from(mActivityTestRule.getActivity())
                    .inflate(R.layout.improved_bookmark_save_flow, /*root=*/mContentView);

            mView = mContentView.findViewById(R.id.improved_bookmark_save_flow);
            mModel = new PropertyModel.Builder(ImprovedBookmarkSaveFlowProperties.ALL_KEYS)
                             .with(ImprovedBookmarkSaveFlowProperties.BOOKMARK_ROW_ICON,
                                     new BitmapDrawable(
                                             mActivityTestRule.getActivity().getResources(),
                                             mBitmap))
                             .with(ImprovedBookmarkSaveFlowProperties.FOLDER_TEXT,
                                     new FolderText("in test folder", 3, 11))
                             .with(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_VISIBLE, false)
                             .build();

            PropertyModelChangeProcessor.create(
                    mModel, mView, ImprovedBookmarkSaveFlowViewBinder::bind);
        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testImage() throws IOException {
        mRenderTestRule.render(mContentView, "image");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testFavicon() throws IOException {
        int bitmapSize = BookmarkUtils.getFaviconDisplaySize(
                mActivityTestRule.getActivity().getResources(), BookmarkRowDisplayPref.VISUAL);
        Bitmap bitmap = Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        bitmap.eraseColor(Color.GREEN);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ImprovedBookmarkSaveFlowProperties.BOOKMARK_ROW_ICON,
                    new BitmapDrawable(mActivityTestRule.getActivity().getResources(), bitmap));
        });
        mRenderTestRule.render(mContentView, "favicon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPriceTracking() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_VISIBLE, true);
            mModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED, true);
        });
        mRenderTestRule.render(mContentView, "price_tracking");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPriceTracking_switchUnchecked() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_VISIBLE, true);
            mModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED, false);
        });
        mRenderTestRule.render(mContentView, "price_tracking_switch_unchecked");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPriceTracking_visibleNotEnabled() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_VISIBLE, true);
            mModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED, true);
            mModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_ENABLED, false);
        });
        mRenderTestRule.render(mContentView, "price_tracking_visible_not_enabled");
    }
}
