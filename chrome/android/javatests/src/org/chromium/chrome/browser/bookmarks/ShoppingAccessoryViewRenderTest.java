// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.Color;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
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
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
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
public class ShoppingAccessoryViewRenderTest {
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
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private ShoppingAccessoryView mShoppingAccessoryView;
    private LinearLayout mContentView;

    public ShoppingAccessoryViewRenderTest(boolean nightModeEnabled) {
        // Sets a fake background color to make the screenshots easier to compare with bare eyes.
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchActivity(null);
        mActivityTestRule.getActivity().setTheme(R.style.Theme_BrowserUI_DayNight);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContentView = new LinearLayout(mActivityTestRule.getActivity());
            mContentView.setBackgroundColor(Color.WHITE);

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            mActivityTestRule.getActivity().setContentView(mContentView, params);

            mShoppingAccessoryView =
                    ShoppingAccessoryView.buildView(mActivityTestRule.getActivity());
            mContentView.addView(mShoppingAccessoryView);
        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNormal() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mShoppingAccessoryView.setPriceTracked(true, true);
            mShoppingAccessoryView.setPriceInformation(10000L, "$100", 10000L, "$100");
        });
        mRenderTestRule.render(mContentView, "normal");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNormal_noIcon() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mShoppingAccessoryView.setPriceTracked(true, false);
            mShoppingAccessoryView.setPriceInformation(1999L, "$19.99", 1999L, "$19.99");
        });
        mRenderTestRule.render(mContentView, "normal_noIcon");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPriceDrop() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mShoppingAccessoryView.setPriceTracked(true, true);
            mShoppingAccessoryView.setPriceInformation(100L, "$100", 50L, "$50");
        });
        mRenderTestRule.render(mContentView, "priceDrop");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPriceDrop_noIcon() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mShoppingAccessoryView.setPriceTracked(true, false);
            mShoppingAccessoryView.setPriceInformation(2999L, "$29.99", 1999L, "$19.99");
        });
        mRenderTestRule.render(mContentView, "priceDrop_noIcon");
    }
}
