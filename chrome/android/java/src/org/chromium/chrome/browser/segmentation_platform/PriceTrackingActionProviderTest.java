// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import android.os.Handler;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.commerce.PriceTrackingUtilsJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController.ActionProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.ProductInfo;
import org.chromium.components.commerce.core.ShoppingService.ProductInfoCallback;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/** Unit tests for {@link PriceTrackingActionProvider} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceTrackingActionProviderTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock PriceTrackingUtils.Natives mMockPriceTrackingUtilsJni;

    @Mock private Tab mMockTab;

    @Mock private ShoppingService mShoppingService;

    @Mock private BookmarkModel mBookmarkModel;

    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        setBookmarkModelReady();
    }

    private void setBookmarkModelReady() {
        mJniMocker.mock(PriceTrackingUtilsJni.TEST_HOOKS, mMockPriceTrackingUtilsJni);

        // Setup bookmark model expectations.
        Mockito.doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mBookmarkModel)
                .finishLoadingBookmarkModel(any());
    }

    private void setIsUrlPriceTrackableResult(boolean hasProductInfo) {
        ProductInfo testProductInfo =
                new ProductInfo(
                        null,
                        null,
                        Optional.of(12345L),
                        Optional.empty(),
                        null,
                        0,
                        null,
                        Optional.empty());
        Mockito.doReturn(true).when(mShoppingService).isShoppingListEligible();
        Mockito.doAnswer(
                        invocation -> {
                            ProductInfoCallback callback = invocation.getArgument(1);
                            callback.onResult(
                                    invocation.getArgument(0),
                                    hasProductInfo ? testProductInfo : null);
                            return null;
                        })
                .when(mShoppingService)
                .getProductInfoForUrl(any(), any());
    }

    private void setIsBookmarkPriceTrackedResult(boolean isBookmarkPriceTracked) {
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            ((Callback<Boolean>) invocation.getArgument(2))
                                    .onResult(isBookmarkPriceTracked);
                            return null;
                        })
                .when(mMockPriceTrackingUtilsJni)
                .isBookmarkPriceTracked(any(Profile.class), anyLong(), any());
    }

    @Test
    public void priceTrackingActionShownSuccessfully() {
        List<ActionProvider> providers = new ArrayList<>();
        PriceTrackingActionProvider provider =
                new PriceTrackingActionProvider(
                        () -> mShoppingService, () -> mBookmarkModel, () -> mProfile);
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        setIsUrlPriceTrackableResult(true);
        provider.getAction(mMockTab, accumulator);
        Assert.assertTrue(accumulator.hasPriceTracking());
    }

    @Test
    public void priceTrackingNotShownForAlreadyPriceTrackedPages() {
        List<ActionProvider> providers = new ArrayList<>();
        PriceTrackingActionProvider provider =
                new PriceTrackingActionProvider(
                        () -> mShoppingService, () -> mBookmarkModel, () -> mProfile);
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        // URL supports price tracking.
        setIsUrlPriceTrackableResult(true);
        Profile.setLastUsedProfileForTesting(mProfile);
        // URL is already bookmarked.
        doReturn(new BookmarkId(1L, 0)).when(mBookmarkModel).getUserBookmarkIdForTab(mMockTab);
        // Bookmark has price tracking information.
        setIsBookmarkPriceTrackedResult(true);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.hasPriceTracking());
    }

    @Test
    public void priceTrackingNotShownForNonTrackablePages() {
        List<ActionProvider> providers = new ArrayList<>();
        PriceTrackingActionProvider provider =
                new PriceTrackingActionProvider(
                        () -> mShoppingService, () -> mBookmarkModel, () -> mProfile);
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        // URL does not support price tracking.
        setIsUrlPriceTrackableResult(false);
        Profile.setLastUsedProfileForTesting(mProfile);
        // URL is bookmarked.
        doReturn(new BookmarkId(1L, 0)).when(mBookmarkModel).getUserBookmarkIdForTab(mMockTab);
        // Bookmark has no price tracking information.
        setIsBookmarkPriceTrackedResult(false);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.hasPriceTracking());
    }
}
