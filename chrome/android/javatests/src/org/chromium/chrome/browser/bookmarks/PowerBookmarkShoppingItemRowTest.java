// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.commerce.ShoppingFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/**
 * Tests for the Shopping power bookmarks experience.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class PowerBookmarkShoppingItemRowTest extends BlankUiTestActivityTestCase {
    private static final long CURRENCY_MUTLIPLIER = 1000000;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private ImageFetcher mImageFetcher;
    @Mock
    private CurrencyFormatter mCurrencyFormatter;
    @Mock
    private BookmarkModel mBookmarkModel;
    @Mock
    private SnackbarManager mSnackbarManager;

    private Bitmap mBitmap;
    private PowerBookmarkShoppingItemRow mPowerBookmarkShoppingItemRow;
    private ViewGroup mContentView;

    public void setupFeatureOverrides() {
        FeatureList.TestValues testValuesOverride = new FeatureList.TestValues();
        testValuesOverride.addFeatureFlagOverride(ChromeFeatureList.BOOKMARKS_REFRESH, true);
        ShoppingFeatures.setShoppingListEligibleForTesting(true);
        FeatureList.setTestValues(testValuesOverride);
    }

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);
        setupFeatureOverrides();

        mBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mBitmap.eraseColor(Color.GREEN);

        ArgumentCaptor<String> currencyCaptor = ArgumentCaptor.forClass(String.class);
        doAnswer((invocation) -> { return "$" + currencyCaptor.getValue(); })
                .when(mCurrencyFormatter)
                .format(currencyCaptor.capture());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mContentView = new LinearLayout(getActivity());
            mContentView.setBackgroundColor(Color.WHITE);

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

            getActivity().setContentView(mContentView, params);
            mPowerBookmarkShoppingItemRow =
                    (PowerBookmarkShoppingItemRow) getActivity()
                            .getLayoutInflater()
                            .inflate(R.layout.power_bookmark_shopping_item_row, mContentView, true)
                            .findViewById(R.id.power_bookmark_shopping_row);
            mPowerBookmarkShoppingItemRow.setBackgroundColor(
                    SemanticColorUtils.getDefaultBgColor(getActivity()));
            ((TextView) mPowerBookmarkShoppingItemRow.findViewById(R.id.title))
                    .setText("Test Bookmark");
            ((TextView) mPowerBookmarkShoppingItemRow.findViewById(R.id.description))
                    .setText("http://google.com");
            mPowerBookmarkShoppingItemRow.init(mImageFetcher, mBookmarkModel, mSnackbarManager);
            mPowerBookmarkShoppingItemRow.setCurrencyFormatterForTesting(mCurrencyFormatter);
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        ShoppingFeatures.setShoppingListEligibleForTesting(null);
    }

    @Test
    @SmallTest
    public void initPriceTrackingUI_NullImage() {
        doAnswer((invocation) -> {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> ((Callback) invocation.getArgument(1)).onResult(null));
            return null;
        })
                .when(mImageFetcher)
                .fetchImage(any(), any());

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPowerBookmarkShoppingItemRow.initPriceTrackingUI("http://foo.com/img", true, 100, 100);
        });

        Assert.assertFalse(mPowerBookmarkShoppingItemRow.getFaviconCancelledForTesting());
    }
}