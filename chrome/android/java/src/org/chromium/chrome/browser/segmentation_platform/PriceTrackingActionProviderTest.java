// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

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
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.ProductInfo;
import org.chromium.components.commerce.core.ShoppingService.ProductInfoCallback;
import org.chromium.url.JUnitTestGURLs;

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

    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;

    @Mock private BookmarkModel mBookmarkModel;

    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        setBookmarkModelReady();
    }

    private void setBookmarkModelReady() {
        mJniMocker.mock(PriceTrackingUtilsJni.TEST_HOOKS, mMockPriceTrackingUtilsJni);
        mJniMocker.mock(CommerceFeatureUtilsJni.TEST_HOOKS, mCommerceFeatureUtilsJniMock);

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
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());
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
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mMockTab).getUrl();
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
    public void priceTrackingNotShownForNonTrackablePages() {
        doReturn(JUnitTestGURLs.GOOGLE_URL).when(mMockTab).getUrl();
        List<ActionProvider> providers = new ArrayList<>();
        PriceTrackingActionProvider provider =
                new PriceTrackingActionProvider(
                        () -> mShoppingService, () -> mBookmarkModel, () -> mProfile);
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        // URL does not support price tracking.
        setIsUrlPriceTrackableResult(false);
        // URL is bookmarked.
        doReturn(new BookmarkId(1L, 0)).when(mBookmarkModel).getUserBookmarkIdForTab(mMockTab);
        // Bookmark has no price tracking information.
        setIsBookmarkPriceTrackedResult(false);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.hasPriceTracking());
    }

    @Test
    public void priceTrackingNotUsedForNonHttpUrls() {
        // Use a non-http(s) url (about:blank).
        doReturn(JUnitTestGURLs.ABOUT_BLANK).when(mMockTab).getUrl();
        List<ActionProvider> providers = new ArrayList<>();
        PriceTrackingActionProvider provider =
                new PriceTrackingActionProvider(
                        () -> mShoppingService, () -> mBookmarkModel, () -> mProfile);
        providers.add(provider);
        SignalAccumulator accumulator = new SignalAccumulator(new Handler(), mMockTab, providers);
        provider.getAction(mMockTab, accumulator);
        Assert.assertFalse(accumulator.hasPriceTracking());
        // Bookmark model shouldn't be loaded/queried.
        verify(mBookmarkModel, never()).finishLoadingBookmarkModel(any());
    }
}
