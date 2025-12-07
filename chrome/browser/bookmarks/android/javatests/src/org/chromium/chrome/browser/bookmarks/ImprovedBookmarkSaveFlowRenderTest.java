// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkSaveFlowProperties.FolderText;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.NightModeTestUtils.NightModeParams;

import java.io.IOException;
import java.util.List;

/** Render tests for the improved bookmark row. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class ImprovedBookmarkSaveFlowRenderTest {
    @ClassParameter
    private static final List<ParameterSet> sClassParams = new NightModeParams().getParameters();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(5)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();

    private Activity mActivity;
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
        mActivity = mActivityTestRule.getActivity();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        int bitmapSize =
                mActivityTestRule
                        .getActivity()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.improved_bookmark_save_flow_image_size);
        mBitmap = Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        mBitmap.eraseColor(Color.GREEN);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new LinearLayout(mActivity);
                    mContentView.setBackgroundColor(Color.WHITE);

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);
                    mActivity.setContentView(mContentView, params);

                    LayoutInflater.from(mActivity)
                            .inflate(
                                    R.layout.improved_bookmark_save_flow, /* root= */ mContentView);

                    mView = mContentView.findViewById(R.id.improved_bookmark_save_flow);
                    mModel =
                            new PropertyModel.Builder(ImprovedBookmarkSaveFlowProperties.ALL_KEYS)
                                    .with(
                                            ImprovedBookmarkSaveFlowProperties.BOOKMARK_ROW_ICON,
                                            new BitmapDrawable(mActivity.getResources(), mBitmap))
                                    .with(
                                            ImprovedBookmarkSaveFlowProperties.SUBTITLE,
                                            BookmarkSaveFlowMediator.createHighlightedCharSequence(
                                                    mActivity,
                                                    new FolderText("in test folder", 3, 11)))
                                    .with(
                                            ImprovedBookmarkSaveFlowProperties
                                                    .PRICE_TRACKING_VISIBLE,
                                            false)
                                    .build();

                    PropertyModelChangeProcessor.create(
                            mModel, mView, ImprovedBookmarkSaveFlowViewBinder::bind);
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
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
    public void testLongText() throws IOException {
        String folderText = "in really really long, I mean an extremely long folder";
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(
                            ImprovedBookmarkSaveFlowProperties.SUBTITLE,
                            BookmarkSaveFlowMediator.createHighlightedCharSequence(
                                    mActivity, new FolderText(folderText, 3, 51)));
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(mContentView, "long_text");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testFavicon() throws IOException {
        int bitmapSize = BookmarkViewUtils.getFaviconDisplaySize(mActivity.getResources());
        Bitmap bitmap = Bitmap.createBitmap(bitmapSize, bitmapSize, Bitmap.Config.ARGB_8888);
        bitmap.eraseColor(Color.GREEN);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(
                            ImprovedBookmarkSaveFlowProperties.BOOKMARK_ROW_ICON,
                            new BitmapDrawable(mActivity.getResources(), bitmap));
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(mContentView, "favicon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPriceTracking() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_VISIBLE, true);
                    mModel.set(
                            ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED, true);
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(mContentView, "price_tracking");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPriceTracking_switchUnchecked() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_VISIBLE, true);
                    mModel.set(
                            ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED,
                            false);
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(mContentView, "price_tracking_switch_unchecked");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPriceTracking_visibleNotEnabled() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_VISIBLE, true);
                    mModel.set(
                            ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED, true);
                    mModel.set(ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_ENABLED, false);
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(mContentView, "price_tracking_visible_not_enabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTitleAndSubtitle() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(
                            ImprovedBookmarkSaveFlowProperties.TITLE,
                            BookmarkSaveFlowMediator.createHighlightedCharSequence(
                                    mActivity, new FolderText("Saved in Mobile bookmarks", 9, 16)));
                    mModel.set(ImprovedBookmarkSaveFlowProperties.SUBTITLE, "On this device ");
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mRenderTestRule.render(mContentView, "title_and_subtitle");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRtlFlipsChevron() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocalizationUtils.setRtlForTesting(true);
                    mModel.set(
                            ImprovedBookmarkSaveFlowProperties.TITLE,
                            BookmarkSaveFlowMediator.createHighlightedCharSequence(
                                    mActivity, new FolderText("Saved in Mobile bookmarks", 9, 16)));
                    mModel.set(ImprovedBookmarkSaveFlowProperties.SUBTITLE, "On this device ");
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        mRenderTestRule.render(mContentView, "rtl_chevron");
        ThreadUtils.runOnUiThreadBlocking(() -> LocalizationUtils.setRtlForTesting(true));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRtlWithAccountBookmarkSubtitle() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    LocalizationUtils.setRtlForTesting(true);
                    // IDS_ACCOUNT_BOOKMARK_SAVE_FLOW_TITLE in ar.
                    String arabicSavedBookmarkString = "تم الحفظ في \"الإشارات المرجعية للجوال\"";
                    mModel.set(
                            ImprovedBookmarkSaveFlowProperties.TITLE,
                            BookmarkSaveFlowMediator.createHighlightedCharSequence(
                                    mActivity, new FolderText(arabicSavedBookmarkString, 13, 25)));
                    mModel.set(
                            ImprovedBookmarkSaveFlowProperties.ADJUST_SUBTITLE_LAYOUT_DIRECTION,
                            true);
                    mModel.set(ImprovedBookmarkSaveFlowProperties.SUBTITLE, "test@gmail.com");
                });
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        mRenderTestRule.render(mContentView, "rtl_with_account_bookmark_subtitle");
        ThreadUtils.runOnUiThreadBlocking(() -> LocalizationUtils.setRtlForTesting(true));
    }
}
