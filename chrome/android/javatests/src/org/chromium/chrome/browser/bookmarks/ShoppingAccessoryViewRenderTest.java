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
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.LocaleUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.ShoppingAccessoryViewProperties.PriceInfo;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.NightModeTestUtils.NightModeParams;
import org.chromium.url.GURLJavaTestHelper;

import java.io.IOException;
import java.util.List;

/** Render tests for the improved bookmark row. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
public class ShoppingAccessoryViewRenderTest {
    private static final long MICRO_CURRENCY_QUOTIENT = 1000000;
    private static final long MICRO_CURRENCY_QUOTIENT_WITH_CENTS = 10000;

    @ClassParameter
    private static List<ParameterSet> sClassParams = new NightModeParams().getParameters();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .setRevision(1)
                    .build();

    private CurrencyFormatter mFormatter;
    private ShoppingAccessoryView mShoppingAccessoryView;
    private PropertyModel mModel;
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

        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        GURLJavaTestHelper.nativeInitializeICU();

        mFormatter = new CurrencyFormatter("USD", LocaleUtils.forLanguageTag("en-US"));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mContentView = new LinearLayout(mActivityTestRule.getActivity());
                    mContentView.setBackgroundColor(Color.WHITE);

                    FrameLayout.LayoutParams params =
                            new FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.WRAP_CONTENT);
                    mActivityTestRule.getActivity().setContentView(mContentView, params);

                    mShoppingAccessoryView =
                            ShoppingAccessoryCoordinator.buildView(mActivityTestRule.getActivity());
                    mModel = new PropertyModel(ShoppingAccessoryViewProperties.ALL_KEYS);
                    PropertyModelChangeProcessor.create(
                            mModel, mShoppingAccessoryView, ShoppingAccessoryViewBinder::bind);

                    mContentView.addView(mShoppingAccessoryView);
                });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNormal() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, true);
                    mModel.set(
                            ShoppingAccessoryViewProperties.PRICE_INFO,
                            new PriceInfo(
                                    100L * MICRO_CURRENCY_QUOTIENT,
                                    100L * MICRO_CURRENCY_QUOTIENT,
                                    mFormatter));
                });
        mRenderTestRule.render(mContentView, "testNormal");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPrice_UntrackAfterInfoSet() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, true);
                    mModel.set(
                            ShoppingAccessoryViewProperties.PRICE_INFO,
                            new PriceInfo(
                                    100L * MICRO_CURRENCY_QUOTIENT,
                                    100L * MICRO_CURRENCY_QUOTIENT,
                                    mFormatter));
                    mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, false);
                });
        mRenderTestRule.render(mContentView, "testPrice_UntrackAfterInfoSet");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPrice_TrackAfterInfoSet() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, false);
                    mModel.set(
                            ShoppingAccessoryViewProperties.PRICE_INFO,
                            new PriceInfo(
                                    100L * MICRO_CURRENCY_QUOTIENT,
                                    100L * MICRO_CURRENCY_QUOTIENT,
                                    mFormatter));
                    mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, true);
                });
        mRenderTestRule.render(mContentView, "testPrice_TrackAfterInfoSet");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testNormal_WithCents() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, true);
                    mModel.set(
                            ShoppingAccessoryViewProperties.PRICE_INFO,
                            new PriceInfo(
                                    1999L * MICRO_CURRENCY_QUOTIENT_WITH_CENTS,
                                    1999L * MICRO_CURRENCY_QUOTIENT_WITH_CENTS,
                                    mFormatter));
                });
        mRenderTestRule.render(mContentView, "testNormal_WithCents");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPriceDrop() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, true);
                    mModel.set(
                            ShoppingAccessoryViewProperties.PRICE_INFO,
                            new PriceInfo(
                                    100L * MICRO_CURRENCY_QUOTIENT,
                                    50L * MICRO_CURRENCY_QUOTIENT,
                                    mFormatter));
                });
        mRenderTestRule.render(mContentView, "testPriceDrop");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPriceDrop_Untracked() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, false);
                    mModel.set(
                            ShoppingAccessoryViewProperties.PRICE_INFO,
                            new PriceInfo(
                                    100L * MICRO_CURRENCY_QUOTIENT,
                                    50L * MICRO_CURRENCY_QUOTIENT,
                                    mFormatter));
                });
        mRenderTestRule.render(mContentView, "testPriceDrop_Untracked");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPriceDrop_UntrackedAfterInfoSet() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, true);
                    mModel.set(
                            ShoppingAccessoryViewProperties.PRICE_INFO,
                            new PriceInfo(
                                    100L * MICRO_CURRENCY_QUOTIENT,
                                    50L * MICRO_CURRENCY_QUOTIENT,
                                    mFormatter));
                    mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, false);
                });
        mRenderTestRule.render(mContentView, "testPriceDrop_UntrackedAfterInfoSet");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPriceDrop_TrackedAfterInfoSet() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, false);
                    mModel.set(
                            ShoppingAccessoryViewProperties.PRICE_INFO,
                            new PriceInfo(
                                    100L * MICRO_CURRENCY_QUOTIENT,
                                    50L * MICRO_CURRENCY_QUOTIENT,
                                    mFormatter));
                    mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, true);
                });
        mRenderTestRule.render(mContentView, "testPriceDrop_TrackedAfterInfoSet");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testPriceDrop_withCents() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel.set(ShoppingAccessoryViewProperties.PRICE_TRACKED, true);
                    mModel.set(
                            ShoppingAccessoryViewProperties.PRICE_INFO,
                            new PriceInfo(
                                    2999L * MICRO_CURRENCY_QUOTIENT_WITH_CENTS,
                                    1999L * MICRO_CURRENCY_QUOTIENT_WITH_CENTS,
                                    mFormatter));
                });
        mRenderTestRule.render(mContentView, "testPriceDrop_withCents");
    }
}
