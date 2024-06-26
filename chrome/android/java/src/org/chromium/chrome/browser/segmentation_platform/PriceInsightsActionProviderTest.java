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
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/** Unit tests for {@link PriceInsightsActionProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceInsightsActionProviderTest {

    @Mock private Tab mMockTab;

    @Mock private ShoppingService mShoppingService;

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
    public void testPriceInsightsDisabledForEmptyPriceInsightsInfo() {
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mMockTab).getUrl();
        List<ActionProvider> providers = new ArrayList<>();
        PriceInsightsActionProvider provider =
                new PriceInsightsActionProvider(() -> mShoppingService);
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        mockShoppingServiceIsPriceInsightsEligibleResult(true);
        mockShoppingServiceGetPriceInsightsInfoForUrlResult(false);
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
        mockShoppingServiceGetPriceInsightsInfoForUrlResult(true);
        provider.getAction(mMockTab, accumulator);
        Assert.assertTrue(accumulator.hasPriceInsights());
    }

    private void mockShoppingServiceIsPriceInsightsEligibleResult(boolean isEligible) {
        doReturn(isEligible).when(mShoppingService).isPriceInsightsEligible();
    }

    private void mockShoppingServiceGetPriceInsightsInfoForUrlResult(boolean hasPriceInsightsInfo) {
        PriceInsightsInfo priceInsightsInfo =
                new PriceInsightsInfo(
                        Optional.of(12345L),
                        null,
                        Optional.empty(),
                        Optional.empty(),
                        Optional.empty(),
                        null,
                        Optional.empty(),
                        0,
                        false);
        doAnswer(
                        invocation -> {
                            PriceInsightsInfoCallback callback = invocation.getArgument(1);
                            callback.onResult(
                                    invocation.getArgument(0),
                                    hasPriceInsightsInfo ? priceInsightsInfo : null);
                            return null;
                        })
                .when(mShoppingService)
                .getPriceInsightsInfoForUrl(any(), any());
    }
}
