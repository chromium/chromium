// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.ColorInt;
import androidx.core.view.ViewCompat;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.List;

/**
 * Render tests for the UI elements of the {@link TabGridThumbnailView}.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class TabGridThumbnailViewRenderTest {
    @ParameterAnnotations.ClassParameter
    public static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID)
                    .setRevision(3)
                    .build();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public TestRule mJunitProcessor = new Features.JUnitProcessor();

    @Mock
    private BrowserControlsStateProvider mBrowserControlsStateProvider;

    private FrameLayout mContentView;
    private ViewGroup mTabCard;
    private TabGridThumbnailView mTabGridThumbnailView;
    private Bitmap mBitmap;

    public TabGridThumbnailViewRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);
        mActivityTestRule.getActivity().setTheme(R.style.Theme_BrowserUI_DayNight);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContentView = new FrameLayout(mActivityTestRule.getActivity());
            mContentView.setBackgroundColor(Color.WHITE);

            mTabCard = (ViewGroup) mActivityTestRule.getActivity().getLayoutInflater().inflate(
                    R.layout.closable_tab_grid_card_item, mContentView, false);
            mTabCard.setVisibility(View.VISIBLE);
            mContentView.addView(mTabCard);

            mTabGridThumbnailView = mContentView.findViewById(R.id.tab_thumbnail);

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            mActivityTestRule.getActivity().setContentView(mContentView, params);
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            final int cardWidthPx = mContentView.getMeasuredWidth() / 2;
            final int cardHeightPx = TabUtils.deriveGridCardHeight(
                    cardWidthPx, mActivityTestRule.getActivity(), mBrowserControlsStateProvider);
            mTabCard.setMinimumWidth(cardWidthPx);
            mTabCard.setMinimumHeight(cardHeightPx);
            mTabCard.getLayoutParams().width = cardWidthPx;
            mTabCard.getLayoutParams().height = cardHeightPx;
            mTabCard.setLayoutParams(mTabCard.getLayoutParams());
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> { mBitmap = createBitmapFourColor(); });
    }

    @After
    public void tearDown() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { NightModeTestUtils.tearDownNightModeForBlankUiTestActivity(); });
    }

    private Bitmap createBitmapFourColor() {
        final int width = mTabGridThumbnailView.getMeasuredWidth();
        final int height = mTabGridThumbnailView.getMeasuredHeight();

        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.TRANSPARENT);

        Paint paint = new Paint();
        paint.setStyle(Paint.Style.FILL);
        paint.setAntiAlias(true);
        paint.setFilterBitmap(true);
        paint.setDither(true);

        paint.setColor(Color.RED);
        final float halfWidth = width / 2.0f;
        final float halfHeight = height / 2.0f;
        final float space = 5f;
        canvas.drawRect(0f, 0f, halfWidth - space, halfHeight - space, paint);

        paint.setColor(Color.GREEN);
        canvas.drawRect(halfWidth + space, 0f, width, halfHeight - space, paint);

        paint.setColor(Color.BLUE);
        canvas.drawRect(0f, halfHeight + space, halfWidth - space, height, paint);

        paint.setColor(Color.WHITE);
        canvas.drawRect(halfWidth + space, halfHeight + space, width, height, paint);
        return bitmap;
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.THUMBNAIL_PLACEHOLDER})
    public void testPlaceholderDrawable() throws IOException, InterruptedException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            updateColor(/*isIncognito=*/true, /*isSelected=*/false);
            mTabGridThumbnailView.setImageDrawable(null);
        });
        mRenderTestRule.render(mTabCard, "placeholder_incognito_without_thumbnail_deselected");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            updateColor(/*isIncognito=*/false, /*isSelected=*/false);
            mTabGridThumbnailView.setImageDrawable(null);
        });
        mRenderTestRule.render(mTabCard, "placeholder_without_thumbnail_deselected");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridThumbnailView.setImageBitmap(mBitmap);
            mTabGridThumbnailView.setImageMatrix(new Matrix());
        });
        mRenderTestRule.render(mTabCard, "placeholder_with_thumbnail_deselected");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            updateColor(/*isIncognito=*/false, /*isSelected=*/true);
            mTabGridThumbnailView.setImageDrawable(null);
        });
        mRenderTestRule.render(mTabCard, "placeholder_without_thumbnail_selected");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridThumbnailView.setImageBitmap(mBitmap);
            mTabGridThumbnailView.setImageMatrix(new Matrix());
        });
        mRenderTestRule.render(mTabCard, "placeholder_with_thumbnail_selected");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            updateColor(/*isIncognito=*/true, /*isSelected=*/true);
            mTabGridThumbnailView.setImageDrawable(null);
        });
        mRenderTestRule.render(mTabCard, "placeholder_incognito_without_thumbnail_selected");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.THUMBNAIL_PLACEHOLDER})
    public void testNoPlaceholderDrawable() throws IOException, InterruptedException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            updateColor(/*isIncognito=*/true, /*isSelected=*/false);
            mTabGridThumbnailView.setImageDrawable(null);
        });
        mRenderTestRule.render(mTabCard, "no_placeholder_incognito_without_thumbnail_deselected");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            updateColor(/*isIncognito=*/false, /*isSelected=*/false);
            mTabGridThumbnailView.setImageDrawable(null);
        });
        mRenderTestRule.render(mTabCard, "no_placeholder_without_thumbnail_deselected");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridThumbnailView.setImageBitmap(mBitmap);
            mTabGridThumbnailView.setImageMatrix(new Matrix());
        });
        mRenderTestRule.render(mTabCard, "no_placeholder_with_thumbnail_deselected");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            updateColor(/*isIncognito=*/false, /*isSelected=*/true);
            mTabGridThumbnailView.setImageDrawable(null);
        });
        mRenderTestRule.render(mTabCard, "no_placeholder_without_thumbnail_selected");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabGridThumbnailView.setImageBitmap(mBitmap);
            mTabGridThumbnailView.setImageMatrix(new Matrix());
        });
        mRenderTestRule.render(mTabCard, "no_placeholder_with_thumbnail_selected");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            updateColor(/*isIncognito=*/true, /*isSelected=*/true);
            mTabGridThumbnailView.setImageDrawable(null);
        });
        mRenderTestRule.render(mTabCard, "no_placeholder_incognito_without_thumbnail_selected");
    }

    private void updateColor(boolean isIncognito, boolean isSelected) {
        View cardView = mTabCard.findViewById(R.id.card_view);
        cardView.getBackground().mutate();
        final @ColorInt int backgroundColor = TabUiThemeProvider.getCardViewBackgroundColor(
                cardView.getContext(), isIncognito, isSelected);
        ViewCompat.setBackgroundTintList(cardView, ColorStateList.valueOf(backgroundColor));

        mTabGridThumbnailView.updateThumbnailPlaceholder(isIncognito, isSelected);
    }
}
