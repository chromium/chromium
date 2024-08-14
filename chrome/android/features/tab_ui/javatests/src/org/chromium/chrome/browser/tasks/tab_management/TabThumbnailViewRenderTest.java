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
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tab_ui.TabUiThemeUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.List;

/** Render tests for the UI elements of the {@link TabThumbnailView}. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class TabThumbnailViewRenderTest {
    @ParameterAnnotations.ClassParameter
    public static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID)
                    .setRevision(5)
                    .build();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;

    private FrameLayout mContentView;
    private ViewGroup mTabCard;
    private TabThumbnailView mTabThumbnailView;
    private Bitmap mBitmap;

    public TabThumbnailViewRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);
        mActivityTestRule.getActivity().setTheme(R.style.Theme_BrowserUI_DayNight);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new FrameLayout(mActivityTestRule.getActivity());
                    mContentView.setBackgroundColor(Color.WHITE);

                    mTabCard =
                            (ViewGroup)
                                    mActivityTestRule
                                            .getActivity()
                                            .getLayoutInflater()
                                            .inflate(
                                                    R.layout.tab_grid_card_item,
                                                    mContentView,
                                                    false);
                    ((TabGridView) mTabCard).setTabActionState(TabActionState.CLOSABLE);
                    mTabCard.setVisibility(View.VISIBLE);
                    mContentView.addView(mTabCard);

                    mTabThumbnailView = mContentView.findViewById(R.id.tab_thumbnail);

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);
                    mActivityTestRule.getActivity().setContentView(mContentView, params);
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final int cardWidthPx = mContentView.getMeasuredWidth() / 2;
                    final int cardHeightPx =
                            TabUtils.deriveGridCardHeight(
                                    cardWidthPx,
                                    mActivityTestRule.getActivity(),
                                    mBrowserControlsStateProvider);
                    mTabCard.setMinimumWidth(cardWidthPx);
                    mTabCard.setMinimumHeight(cardHeightPx);
                    mTabCard.getLayoutParams().width = cardWidthPx;
                    mTabCard.getLayoutParams().height = cardHeightPx;
                    mTabCard.setLayoutParams(mTabCard.getLayoutParams());
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBitmap = createBitmapFourColor();
                });
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
                });
    }

    private Bitmap createBitmapFourColor() {
        final int width = mTabThumbnailView.getMeasuredWidth();
        final int height = mTabThumbnailView.getMeasuredHeight();

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
    public void testPlaceholderDrawable() throws IOException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    updateColor(/* isIncognito= */ true, /* isSelected= */ false);
                    mTabThumbnailView.setImageDrawable(null);
                });
        mRenderTestRule.render(mTabCard, "placeholder_incognito_without_thumbnail_deselected");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    updateColor(/* isIncognito= */ false, /* isSelected= */ false);
                    mTabThumbnailView.setImageDrawable(null);
                });
        mRenderTestRule.render(mTabCard, "placeholder_without_thumbnail_deselected");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabThumbnailView.setImageBitmap(mBitmap);
                    mTabThumbnailView.setImageMatrix(new Matrix());
                });
        mRenderTestRule.render(mTabCard, "placeholder_with_thumbnail_deselected");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    updateColor(/* isIncognito= */ false, /* isSelected= */ true);
                    mTabThumbnailView.setImageDrawable(null);
                });
        mRenderTestRule.render(mTabCard, "placeholder_without_thumbnail_selected");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabThumbnailView.setImageBitmap(mBitmap);
                    mTabThumbnailView.setImageMatrix(new Matrix());
                });
        mRenderTestRule.render(mTabCard, "placeholder_with_thumbnail_selected");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    updateColor(/* isIncognito= */ true, /* isSelected= */ true);
                    mTabThumbnailView.setImageDrawable(null);
                });
        mRenderTestRule.render(mTabCard, "placeholder_incognito_without_thumbnail_selected");
    }

    private void updateColor(boolean isIncognito, boolean isSelected) {
        View cardView = mTabCard.findViewById(R.id.card_view);
        cardView.getBackground().mutate();
        final @ColorInt int backgroundColor =
                TabUiThemeUtils.getCardViewBackgroundColor(
                        cardView.getContext(), isIncognito, isSelected);
        ViewCompat.setBackgroundTintList(cardView, ColorStateList.valueOf(backgroundColor));

        mTabThumbnailView.updateThumbnailPlaceholder(isIncognito, isSelected);
    }
}
