// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.coupons;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import static org.chromium.ui.test.util.RenderTestRule.Component.UI_BROWSER_SHOPPING_DEALS;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.commerce.core.DiscountInfo;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.DiscountInfoCallback;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.ENABLE_DISCOUNT_INFO_API})
@Batch(Batch.PER_CLASS)
public class DiscountsIntegrationTest {
    private static final int TEST_PORT = 12345;
    private static final String TEST_PAGE_URL_WITH_DISCOUNTS =
            "/chrome/test/data/android/test.html";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public final EmbeddedTestServerRule sEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(UI_BROWSER_SHOPPING_DEALS)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ShoppingService mMockShoppingService;

    private EmbeddedTestServer mTestServer;
    private GURL mTestPageWithDiscounts;

    @Before
    public void setUp() {
        ShoppingServiceFactory.setShoppingServiceForTesting(mMockShoppingService);
        doReturn(true).when(mMockShoppingService).isDiscountEligibleToShowOnNavigation();

        mTestServer = sEmbeddedTestServerRule.getServer();
        mTestPageWithDiscounts = new GURL(mTestServer.getURL(TEST_PAGE_URL_WITH_DISCOUNTS));

        mActivityTestRule.startMainActivityOnBlankPage();
        mockShoppingServiceDiscountsResponse();
    }

    private void mockShoppingServiceDiscountsResponse() {
        doAnswer(
                        invocation -> {
                            List<DiscountInfo> discountInfoList = new ArrayList<>();
                            discountInfoList.add(
                                    new DiscountInfo(
                                            0, 0, "en-US", "detail", "terms", "value", "code", 123,
                                            false, 10, 123));
                            DiscountInfoCallback callback = invocation.getArgument(1);
                            callback.onResult(invocation.getArgument(0), discountInfoList);
                            return null;
                        })
                .when(mMockShoppingService)
                .getDiscountInfoForUrl(any(), any());
    }

    private void navigateAndWaitForDiscountsContextualPageActionIcon() {
        mActivityTestRule.loadUrl(mTestPageWithDiscounts);
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        withContentDescription(R.string.discount_icon_expanded_text)));
    }

    @Test
    @SmallTest
    public void testDiscountsContextualPageActionIconShown() {
        navigateAndWaitForDiscountsContextualPageActionIcon();
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisableIf.Build(sdk_equals = 32)
    // Disabled on Android Automotive. See b/368117896
    public void testRenderDiscountContextualPageActionIcon() throws IOException {
        navigateAndWaitForDiscountsContextualPageActionIcon();
        mRenderTestRule.render(
                mActivityTestRule
                        .getActivity()
                        .findViewById(R.id.optional_toolbar_button_container),
                "Discount_found_icon");
    }

    @Test
    @SmallTest
    public void testDiscountsContextualPageActionOnClickOpenBottomSheet() {
        mActivityTestRule.loadUrl(mTestPageWithDiscounts);
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        withContentDescription(R.string.discount_icon_expanded_text)));
        onView(
                        allOf(
                                withId(R.id.optional_toolbar_button),
                                withContentDescription(R.string.discount_icon_expanded_text)))
                .perform(click());
        ViewUtils.waitForVisibleView(withId(R.id.commerce_bottom_sheet_content_container));
    }
}
