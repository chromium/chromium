// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CallbackUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.widget.async_image.AsyncImageView;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.List;

@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class TabCardLabelViewRenderTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID)
                    .setRevision(1)
                    .build();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private TabCardLabelView mTabCardLabelView;
    private Runnable mCancelRunnable = CallbackUtils.emptyRunnable();

    public TabCardLabelViewRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        mActivityTestRule.launchActivity(/* startIntent= */ null);
        Activity activity = mActivityTestRule.getActivity();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        runOnUiThreadBlocking(
                () -> {
                    FrameLayout contentView = new FrameLayout(activity);
                    contentView.setBackgroundColor(Color.WHITE);

                    mTabCardLabelView =
                            (TabCardLabelView)
                                    LayoutInflater.from(activity)
                                            .inflate(R.layout.tab_card_label_layout, null);
                    LayoutParams params =
                            new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
                    contentView.addView(mTabCardLabelView, params);

                    params = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
                    activity.setContentView(contentView, params);
                });
    }

    @After
    public void tearDown() throws Exception {
        runOnUiThreadBlocking(NightModeTestUtils::tearDownNightModeForBlankUiTestActivity);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPriceDrop_LTR() throws Exception {
        TextResolver textResolver =
                new PriceDropTextResolver(/* price= */ "$100", /* previousPrice= */ "$200");
        buildAndSetData(TabCardLabelType.PRICE_DROP, textResolver, /* asyncImageFactory= */ null);
        mRenderTestRule.render(mTabCardLabelView, "price_drop_ltr");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPriceDrop_RTL() throws Exception {
        LocalizationUtils.setRtlForTesting(true);
        TextResolver textResolver =
                new PriceDropTextResolver(/* price= */ "$10", /* previousPrice= */ "$2000");
        buildAndSetData(TabCardLabelType.PRICE_DROP, textResolver, /* asyncImageFactory= */ null);
        mRenderTestRule.render(mTabCardLabelView, "price_drop_rtl");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testActivityUpdate_TextOnly() throws Exception {
        TextResolver textResolver = buildTextResolver("John changed");
        buildAndSetData(
                TabCardLabelType.ACTIVITY_UPDATE, textResolver, /* asyncImageFactory= */ null);
        mRenderTestRule.render(mTabCardLabelView, "activity_update_text");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testActivityUpdate_Avatar() throws Exception {
        AsyncImageView.Factory asyncImageFactory =
                (callback, width, height) -> {
                    Bitmap bmp = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
                    Canvas canvas = new Canvas(bmp);
                    canvas.drawColor(Color.RED);
                    callback.onResult(new BitmapDrawable(bmp));
                    return mCancelRunnable;
                };
        TextResolver textResolver =
                buildTextResolver(
                        "my_really_really_long_name_caused_an_update_and_cannot_fit changed");
        buildAndSetData(TabCardLabelType.ACTIVITY_UPDATE, textResolver, asyncImageFactory);
        mRenderTestRule.render(mTabCardLabelView, "activity_update_avatar");

        runOnUiThreadBlocking(
                () -> {
                    mTabCardLabelView.setData(null);
                    assertEquals(View.GONE, mTabCardLabelView.getVisibility());
                    // Set to be visible to confirm all the UI is cleaned up.
                    mTabCardLabelView.setVisibility(View.VISIBLE);
                });
        mRenderTestRule.render(mTabCardLabelView, "post_reset");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testActivityUpdate_NoAvatar() throws Exception {
        AsyncImageView.Factory asyncImageFactory =
                (callback, width, height) -> {
                    callback.onResult(null);
                    return mCancelRunnable;
                };
        TextResolver textResolver = buildTextResolver("Bob added");
        buildAndSetData(TabCardLabelType.ACTIVITY_UPDATE, textResolver, asyncImageFactory);
        mRenderTestRule.render(mTabCardLabelView, "activity_update_avatar_unavailable");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testActivityUpdate_WaitingOnAvatar() throws Exception {
        AsyncImageView.Factory asyncImageFactory =
                (callback, width, height) -> {
                    return mCancelRunnable;
                };
        TextResolver textResolver = buildTextResolver("Alice changed");
        buildAndSetData(TabCardLabelType.ACTIVITY_UPDATE, textResolver, asyncImageFactory);
        mRenderTestRule.render(mTabCardLabelView, "activity_update_avatar_pending");
    }

    private void buildAndSetData(
            @TabCardLabelType int labelType,
            TextResolver textResolver,
            @Nullable AsyncImageView.Factory asyncImageFactory) {
        TabCardLabelData data =
                new TabCardLabelData(
                        labelType,
                        textResolver,
                        asyncImageFactory,
                        /* contentDescriptionResolver= */ null);
        runOnUiThreadBlocking(
                () -> {
                    mTabCardLabelView.setData(data);
                });
    }

    private TextResolver buildTextResolver(String string) {
        return (context) -> {
            return string;
        };
    }
}
