// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.NightModeTestUtils.NightModeParams;

import java.io.IOException;
import java.util.List;

/**
 * Render tests for the Shopping power bookmarks experience.
 */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@EnableFeatures({ChromeFeatureList.BOOKMARKS_REFRESH})
public class PowerBookmarkShoppingItemRowRenderTest {
    @ParameterAnnotations.ClassParameter
    private static List<ParameterSet> sClassParams = new NightModeParams().getParameters();

    private static final long CURRENCY_MUTLIPLIER = 1000000;

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BOOKMARKS)
                    .build();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private ImageFetcher mImageFetcher;
    @Mock
    private CurrencyFormatter mCurrencyFormatter;
    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private SnackbarManager mSnackbarManager;
    @Mock
    private Profile mProfile;

    private Bitmap mBitmap;
    private PowerBookmarkShoppingItemRow mPowerBookmarkShoppingItemRow;
    private LinearLayout mContentView;

    public PowerBookmarkShoppingItemRowRenderTest(boolean nightModeEnabled) {
        // Sets a fake background color to make the screenshots easier to compare with bare eyes.
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.launchActivity(null);

        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mBitmap.eraseColor(Color.GREEN);

        ArgumentCaptor<Callback<Bitmap>> bitmapCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        doAnswer((invocation) -> {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> bitmapCallbackCaptor.getValue().onResult(mBitmap));
            return null;
        })
                .when(mImageFetcher)
                .fetchImage(any(), bitmapCallbackCaptor.capture());

        ArgumentCaptor<String> currencyCaptor = ArgumentCaptor.forClass(String.class);
        doAnswer((invocation) -> { return "$" + currencyCaptor.getValue(); })
                .when(mCurrencyFormatter)
                .format(currencyCaptor.capture());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingFeatures.setShoppingListEligibleForTesting(true);
            mContentView = new LinearLayout(mActivityTestRule.getActivity());
            mContentView.setBackgroundColor(Color.WHITE);

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            mActivityTestRule.getActivity().setContentView(mContentView, params);

            mPowerBookmarkShoppingItemRow =
                    BookmarkManagerCoordinator.buildShoppingItemView(mContentView);
            mContentView.addView(mPowerBookmarkShoppingItemRow);
            mPowerBookmarkShoppingItemRow.setBackgroundColor(
                    SemanticColorUtils.getDefaultBgColor(mActivityTestRule.getActivity()));
            ((TextView) mPowerBookmarkShoppingItemRow.findViewById(R.id.title))
                    .setText("Test Bookmark");
            ((TextView) mPowerBookmarkShoppingItemRow.findViewById(R.id.description))
                    .setText("http://google.com");
            mPowerBookmarkShoppingItemRow.findViewById(R.id.more).setVisibility(View.VISIBLE);
            mPowerBookmarkShoppingItemRow.init(
                    mImageFetcher, mBookmarkModel, mSnackbarManager, mProfile);
            mPowerBookmarkShoppingItemRow.setCurrencyFormatterForTesting(mCurrencyFormatter);
        });
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShoppingNormalPriceWithTrackingEnabled() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPowerBookmarkShoppingItemRow.initPriceTrackingUI("http://foo.com/img", true,
                    100 * CURRENCY_MUTLIPLIER, 100 * CURRENCY_MUTLIPLIER);
        });
        mRenderTestRule.render(mContentView, "shopping_normal_price_with_tracking_enabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShoppingNormalPriceWithTrackingDisabled() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPowerBookmarkShoppingItemRow.initPriceTrackingUI("http://foo.com/img", false,
                    100 * CURRENCY_MUTLIPLIER, 100 * CURRENCY_MUTLIPLIER);
        });
        mRenderTestRule.render(mContentView, "shopping_normal_price_with_tracking_disabled");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShoppingPriceDrop() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPowerBookmarkShoppingItemRow.initPriceTrackingUI("http://foo.com/img", false,
                    100 * CURRENCY_MUTLIPLIER, 50 * CURRENCY_MUTLIPLIER);
        });
        mRenderTestRule.render(mContentView, "shopping_price_drop");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testShoppingRebindUI() throws IOException {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPowerBookmarkShoppingItemRow.initPriceTrackingUI("http://foo.com/img", false,
                    100 * CURRENCY_MUTLIPLIER, 100 * CURRENCY_MUTLIPLIER);
        });
        mRenderTestRule.render(mContentView, "shopping_rebind_normal_price_tracking_disabled");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPowerBookmarkShoppingItemRow.initPriceTrackingUI("http://foo.com/img", true,
                    100 * CURRENCY_MUTLIPLIER, 100 * CURRENCY_MUTLIPLIER);
        });
        mRenderTestRule.render(mContentView, "shopping_rebind_normal_price_tracking_enabled");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPowerBookmarkShoppingItemRow.initPriceTrackingUI("http://foo.com/img", true,
                    100 * CURRENCY_MUTLIPLIER, 200 * CURRENCY_MUTLIPLIER);
        });
        mRenderTestRule.render(mContentView, "shopping_rebind_price_increase_tracking_enabled");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPowerBookmarkShoppingItemRow.initPriceTrackingUI("http://foo.com/img", true,
                    200 * CURRENCY_MUTLIPLIER, 100 * CURRENCY_MUTLIPLIER);
        });
        mRenderTestRule.render(mContentView, "shopping_rebind_price_decrease_tracking_enabled");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPowerBookmarkShoppingItemRow.initPriceTrackingUI("http://foo.com/img", true,
                    100 * CURRENCY_MUTLIPLIER, 100 * CURRENCY_MUTLIPLIER);
        });
        mRenderTestRule.render(
                mContentView, "shopping_rebind_back_to_normal_price_tracking_enabled");
    }
}
