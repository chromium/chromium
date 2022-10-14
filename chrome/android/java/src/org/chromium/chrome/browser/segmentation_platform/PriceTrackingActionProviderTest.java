// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.when;

import android.os.Handler;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController.ActionProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.ProductInfo;
import org.chromium.components.commerce.core.ShoppingService.ProductInfoCallback;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link PriceTrackingActionProvider}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceTrackingActionProviderTest {
    @Mock
    private Tab mMockTab;

    @Mock
    private ShoppingService mShoppingService;

    @Mock
    private BookmarkModel mBookmarkModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        setBookmarkModelReady();
    }

    private void setBookmarkModelReady() {
        // Setup bookmark model expectations.
        Mockito.doAnswer(invocation -> {
                   Runnable runnable = invocation.getArgument(0);
                   runnable.run();
                   return null;
               })
                .when(mBookmarkModel)
                .finishLoadingBookmarkModel(any());
    }

    private void setPriceTrackingBackendResult(boolean hasProductInfo) {
        ProductInfo testProductInfo = new ProductInfo(null, null, 0, 0, null, 0, null);
        Mockito.doAnswer(invocation -> {
                   ProductInfoCallback callback = invocation.getArgument(1);
                   callback.onResult(
                           invocation.getArgument(0), hasProductInfo ? testProductInfo : null);
                   return null;
               })
                .when(mShoppingService)
                .getProductInfoForUrl(any(), any());
    }

    private void setPageAlreadyPriceTracked(boolean alreadyPriceTracked) {
        when(mBookmarkModel.getUserBookmarkIdForTab(any())).thenReturn(null);
        org.chromium.components.power_bookmarks.PowerBookmarkMeta.Builder builder =
                org.chromium.components.power_bookmarks.PowerBookmarkMeta.newBuilder();
        builder.setShoppingSpecifics(
                org.chromium.components.power_bookmarks.ShoppingSpecifics.newBuilder()
                        .setIsPriceTracked(alreadyPriceTracked)
                        .build());
        when(mBookmarkModel.getPowerBookmarkMeta(any())).thenReturn(builder.build());
    }

    @Test
    public void priceTrackingActionShownSuccessfully() {
        List<ActionProvider> providers = new ArrayList<>();
        PriceTrackingActionProvider provider =
                new PriceTrackingActionProvider(() -> mShoppingService, () -> mBookmarkModel);
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        setPriceTrackingBackendResult(true);
        provider.getAction(mMockTab, accumulator);
        Assert.assertTrue(accumulator.hasPriceTracking());
    }

    @Test
    public void priceTrackingNotShownForAlreadyPriceTrackedPages() {
        List<ActionProvider> providers = new ArrayList<>();
        PriceTrackingActionProvider provider =
                new PriceTrackingActionProvider(() -> mShoppingService, () -> mBookmarkModel);
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        setPageAlreadyPriceTracked(true);
        setPriceTrackingBackendResult(true);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.hasPriceTracking());
    }
}
