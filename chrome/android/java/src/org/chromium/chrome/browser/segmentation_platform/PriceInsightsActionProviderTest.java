// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.os.Handler;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController.ActionProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfoCallback;
import org.chromium.components.commerce.core.ShoppingService.PricePoint;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;

/** Unit tests for {@link PriceInsightsActionProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceInsightsActionProviderTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tab mMockTab;
    @Mock private ShoppingService mShoppingService;

    private static final PriceInsightsInfo EMPTY_PRICE_INSIGHTS_INFO =
            new PriceInsightsInfo(null, "", null, null, null, new ArrayList<>(), null, 0, false);
    private static final PriceInsightsInfo PRICE_INSIGHTS_INFO =
            new PriceInsightsInfo(
                    null,
                    "USD",
                    null,
                    null,
                    "Stainless steel, Espresso Bundle",
                    Arrays.asList(new PricePoint("08-08-2024", 65000000L)),
                    JUnitTestGURLs.EXAMPLE_URL,
                    0,
                    true);

    @Test
    public void testPriceInsightsDisabledForNonHttpUrls() {
        // Use a non-http(s) url (about:blank).
        doReturn(JUnitTestGURLs.ABOUT_BLANK).when(mMockTab).getUrl();
        HashMap<Integer, ActionProvider> providers = new HashMap<>();
        PriceInsightsActionProvider provider =
                new PriceInsightsActionProvider(() -> mShoppingService);
        providers.put(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS, provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.getSignal(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS));
    }

    @Test
    public void testPriceInsightsDisabledForShoppingServiceIneligible() {
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mMockTab).getUrl();
        HashMap<Integer, ActionProvider> providers = new HashMap<>();
        PriceInsightsActionProvider provider =
                new PriceInsightsActionProvider(() -> mShoppingService);
        providers.put(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS, provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        mockShoppingServiceIsPriceInsightsEligibleResult(false);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.getSignal(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS));
    }

    @Test
    public void testPriceInsightsDisabledForNullPriceInsightsInfo() {
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mMockTab).getUrl();
        HashMap<Integer, ActionProvider> providers = new HashMap<>();
        PriceInsightsActionProvider provider =
                new PriceInsightsActionProvider(() -> mShoppingService);
        providers.put(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS, provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        mockShoppingServiceIsPriceInsightsEligibleResult(true);
        mockShoppingServiceGetPriceInsightsInfoForUrlResult(null);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.getSignal(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS));
    }

    @Test
    public void testPriceInsightsDisabledForEmptyPriceInsightsInfo() {
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mMockTab).getUrl();
        HashMap<Integer, ActionProvider> providers = new HashMap<>();
        PriceInsightsActionProvider provider =
                new PriceInsightsActionProvider(() -> mShoppingService);
        providers.put(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS, provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        mockShoppingServiceIsPriceInsightsEligibleResult(true);
        mockShoppingServiceGetPriceInsightsInfoForUrlResult(EMPTY_PRICE_INSIGHTS_INFO);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.getSignal(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS));
    }

    @Test
    public void testPriceInsightsEnabled() {
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mMockTab).getUrl();
        HashMap<Integer, ActionProvider> providers = new HashMap<>();
        PriceInsightsActionProvider provider =
                new PriceInsightsActionProvider(() -> mShoppingService);
        providers.put(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS, provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        mockShoppingServiceIsPriceInsightsEligibleResult(true);
        mockShoppingServiceGetPriceInsightsInfoForUrlResult(PRICE_INSIGHTS_INFO);
        provider.getAction(mMockTab, accumulator);
        Assert.assertTrue(accumulator.getSignal(AdaptiveToolbarButtonVariant.PRICE_INSIGHTS));
    }

    private void mockShoppingServiceIsPriceInsightsEligibleResult(boolean isEligible) {
        doReturn(isEligible).when(mShoppingService).isPriceInsightsEligible();
    }

    private void mockShoppingServiceGetPriceInsightsInfoForUrlResult(
            @Nullable PriceInsightsInfo info) {
        doAnswer(
                        invocation -> {
                            PriceInsightsInfoCallback callback = invocation.getArgument(1);
                            callback.onResult(invocation.getArgument(0), info);
                            return null;
                        })
                .when(mShoppingService)
                .getPriceInsightsInfoForUrl(any(), any());
    }
}
