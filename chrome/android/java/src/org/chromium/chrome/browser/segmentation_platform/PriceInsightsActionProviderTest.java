// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.os.Handler;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController.ActionProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfo;
import org.chromium.components.commerce.core.ShoppingService.PriceInsightsInfoCallback;
import org.chromium.components.commerce.core.ShoppingService.PricePoint;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;

/** Unit tests for {@link PriceInsightsActionProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceInsightsActionProviderTest {

    @Mock private Tab mMockTab;
    @Mock private ShoppingService mShoppingService;

    private static final PriceInsightsInfo EMPTY_PRICE_INSIGHTS_INFO =
            new PriceInsightsInfo(
                    Optional.empty(),
                    "",
                    Optional.empty(),
                    Optional.empty(),
                    Optional.empty(),
                    new ArrayList<>(),
                    Optional.empty(),
                    0,
                    false);
    private static final PriceInsightsInfo PRICE_INSIGHTS_INFO =
            new PriceInsightsInfo(
                    Optional.empty(),
                    "USD",
                    Optional.empty(),
                    Optional.empty(),
                    Optional.of("Stainless steel, Espresso Bundle"),
                    Arrays.asList(new PricePoint("08-08-2024", 65000000L)),
                    Optional.of(JUnitTestGURLs.EXAMPLE_URL),
                    0,
                    true);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testPriceInsightsDisabledForNonHttpUrls() {
        // Use a non-http(s) url (about:blank).
        doReturn(JUnitTestGURLs.ABOUT_BLANK).when(mMockTab).getUrl();
        List<ActionProvider> providers = new ArrayList<>();
        PriceInsightsActionProvider provider =
                new PriceInsightsActionProvider(() -> mShoppingService);
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.hasPriceInsights());
    }

    @Test
    public void testPriceInsightsDisabledForShoppingServiceIneligible() {
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mMockTab).getUrl();
        List<ActionProvider> providers = new ArrayList<>();
        PriceInsightsActionProvider provider =
                new PriceInsightsActionProvider(() -> mShoppingService);
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        mockShoppingServiceIsPriceInsightsEligibleResult(false);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.hasPriceInsights());
    }

    @Test
    public void testPriceInsightsDisabledForNullPriceInsightsInfo() {
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mMockTab).getUrl();
        List<ActionProvider> providers = new ArrayList<>();
        PriceInsightsActionProvider provider =
                new PriceInsightsActionProvider(() -> mShoppingService);
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        mockShoppingServiceIsPriceInsightsEligibleResult(true);
        mockShoppingServiceGetPriceInsightsInfoForUrlResult(null);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.hasPriceInsights());
    }

    @Test
    public void testPriceInsightsDisabledForEmptyPriceInsightsInfo() {
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mMockTab).getUrl();
        List<ActionProvider> providers = new ArrayList<>();
        PriceInsightsActionProvider provider =
                new PriceInsightsActionProvider(() -> mShoppingService);
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        mockShoppingServiceIsPriceInsightsEligibleResult(true);
        mockShoppingServiceGetPriceInsightsInfoForUrlResult(EMPTY_PRICE_INSIGHTS_INFO);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.hasPriceInsights());
    }

    @Test
    public void testPriceInsightsEnabled() {
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mMockTab).getUrl();
        List<ActionProvider> providers = new ArrayList<>();
        PriceInsightsActionProvider provider =
                new PriceInsightsActionProvider(() -> mShoppingService);
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        mockShoppingServiceIsPriceInsightsEligibleResult(true);
        mockShoppingServiceGetPriceInsightsInfoForUrlResult(PRICE_INSIGHTS_INFO);
        provider.getAction(mMockTab, accumulator);
        Assert.assertTrue(accumulator.hasPriceInsights());
    }

    private void mockShoppingServiceIsPriceInsightsEligibleResult(boolean isEligible) {
        doReturn(isEligible).when(mShoppingService).isPriceInsightsEligible();
    }

    private void mockShoppingServiceGetPriceInsightsInfoForUrlResult(PriceInsightsInfo info) {
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
