// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import com.google.common.primitives.UnsignedLongs;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.commerce.PriceTrackingUtilsJni;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.ShoppingService.ProductInfo;
import org.chromium.components.commerce.core.ShoppingService.ProductInfoCallback;
import org.chromium.components.commerce.core.SubscriptionsObserver;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Optional;

/** Unit tests for {@link CurrentTabPriceTrackingStateSupplier} */
@RunWith(BaseRobolectricTestRunner.class)
public class CurrentTabPriceTrackingStateSupplierUnitTest {

    @Rule public JniMocker mJniMocker = new JniMocker();

    private ObservableSupplierImpl<Tab> mTabSupplier;
    private ObservableSupplierImpl<Profile> mProfileSupplier;
    @Mock private Profile mMockProfile;
    @Mock private Tab mMockTab;
    @Mock private ShoppingService mMockShoppingService;
    @Mock PriceTrackingUtils.Natives mMockPriceTrackingUtilsJni;
    @Captor ArgumentCaptor<ProductInfoCallback> mProductInfoCallbackCaptor;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(PriceTrackingUtilsJni.TEST_HOOKS, mMockPriceTrackingUtilsJni);

        mTabSupplier = new ObservableSupplierImpl<>();
        mProfileSupplier = new ObservableSupplierImpl<>();

        ShoppingServiceFactory.setShoppingServiceForTesting(mMockShoppingService);
    }

    private ProductInfo createProductInfoWithId(long productId) {
        return new ProductInfo(
                "Product",
                GURL.emptyGURL(),
                Optional.of(productId),
                Optional.empty(),
                "USD",
                1000,
                "US",
                Optional.empty());
    }

    @Test
    public void testDestroyBeforeProfile() {
        CurrentTabPriceTrackingStateSupplier supplier =
                new CurrentTabPriceTrackingStateSupplier(mTabSupplier, mProfileSupplier);
        assertTrue(mProfileSupplier.hasObservers());

        supplier.destroy();
        // TODO(https://crbug.com/352082581): Enable this once observer is fixed.
        // assertFalse(mProfileSupplier.hasObservers());
        verifyNoInteractions(mMockShoppingService);
    }

    @Test
    public void testDestroyAfterProfile() {
        CurrentTabPriceTrackingStateSupplier supplier =
                new CurrentTabPriceTrackingStateSupplier(mTabSupplier, mProfileSupplier);
        assertTrue(mProfileSupplier.hasObservers());
        mProfileSupplier.set(mMockProfile);
        verify(mMockShoppingService).addSubscriptionsObserver(any());

        supplier.destroy();
        // TODO(https://crbug.com/352082581): Enable this once observer is fixed.
        // assertFalse(mProfileSupplier.hasObservers());
        verify(mMockShoppingService).removeSubscriptionsObserver(any());
    }

    @Test
    public void testWithEmptySuppliers() {
        Callback<Boolean> mockCallback = mock(Callback.class);

        var supplier = new CurrentTabPriceTrackingStateSupplier(mTabSupplier, mProfileSupplier);
        supplier.addObserver(mockCallback);

        verify(mockCallback, never()).onResult(anyBoolean());
        assertFalse(supplier.get());
    }

    @Test
    public void testWithTabWithoutProductInfo() {
        Callback<Boolean> mockCallback = mock(Callback.class);
        when(mMockTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_CAT);

        var supplier = new CurrentTabPriceTrackingStateSupplier(mTabSupplier, mProfileSupplier);
        supplier.addObserver(mockCallback);

        mProfileSupplier.set(mMockProfile);
        mTabSupplier.set(mMockTab);

        verify(mMockShoppingService)
                .getProductInfoForUrl(
                        eq(JUnitTestGURLs.GOOGLE_URL_CAT), mProductInfoCallbackCaptor.capture());
        // Return no product info for the current tab.
        mProductInfoCallbackCaptor.getValue().onResult(JUnitTestGURLs.GOOGLE_URL_CAT, null);

        // Supplier shouldn't invoke the callback.
        verify(mockCallback, never()).onResult(anyBoolean());
        assertFalse(supplier.get());
    }

    @Test
    public void testWithTabWithProductInfo_untracked() {
        Callback<Boolean> mockCallback = mock(Callback.class);
        long productClusterId = 1234L;
        ShoppingService.ProductInfo productInfo = createProductInfoWithId(productClusterId);

        when(mMockTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_CAT);
        // Set ShoppingService to return product info for the current tab.

        ArgumentCaptor<CommerceSubscription> commerceSubscriptionArgumentCaptor =
                ArgumentCaptor.forClass(CommerceSubscription.class);
        ArgumentCaptor<Callback<Boolean>> shoppingServiceCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);

        var supplier = new CurrentTabPriceTrackingStateSupplier(mTabSupplier, mProfileSupplier);
        supplier.addObserver(mockCallback);

        mProfileSupplier.set(mMockProfile);
        mTabSupplier.set(mMockTab);

        verify(mMockShoppingService)
                .getProductInfoForUrl(
                        eq(JUnitTestGURLs.GOOGLE_URL_CAT), mProductInfoCallbackCaptor.capture());
        // Return product info for the current tab.
        mProductInfoCallbackCaptor.getValue().onResult(JUnitTestGURLs.GOOGLE_URL_CAT, productInfo);

        // Ensure ShoppingService is called to check if the current product info is tracked.
        verify(mMockShoppingService)
                .isSubscribed(
                        commerceSubscriptionArgumentCaptor.capture(),
                        shoppingServiceCallbackCaptor.capture());
        // Ensure ShoppingService was called with the correct product ID.
        assertEquals(
                UnsignedLongs.toString(productClusterId),
                commerceSubscriptionArgumentCaptor.getValue().id);
        // Set ShoppingService to return false on the callback to isSubscribed.
        shoppingServiceCallbackCaptor.getValue().onResult(false);

        // Supplier shouldn't invoke the callback.
        verify(mockCallback, never()).onResult(anyBoolean());
        assertFalse(supplier.get());
    }

    @Test
    public void testWithTabWithProductInfo_tracked() {
        Callback<Boolean> mockCallback = mock(Callback.class);
        long productClusterId = 1234L;
        ShoppingService.ProductInfo productInfo = createProductInfoWithId(productClusterId);

        when(mMockTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_CAT);

        ArgumentCaptor<Callback<Boolean>> shoppingServiceCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);

        var supplier = new CurrentTabPriceTrackingStateSupplier(mTabSupplier, mProfileSupplier);
        supplier.addObserver(mockCallback);

        mProfileSupplier.set(mMockProfile);
        mTabSupplier.set(mMockTab);

        verify(mMockShoppingService)
                .getProductInfoForUrl(
                        eq(JUnitTestGURLs.GOOGLE_URL_CAT), mProductInfoCallbackCaptor.capture());
        // Return product info for the current tab.
        mProductInfoCallbackCaptor.getValue().onResult(JUnitTestGURLs.GOOGLE_URL_CAT, productInfo);

        verify(mMockShoppingService).isSubscribed(any(), shoppingServiceCallbackCaptor.capture());
        // Set ShoppingService to return true on the callback to isSubscribed.
        shoppingServiceCallbackCaptor.getValue().onResult(true);

        // Supplier should invoke callback.
        verify(mockCallback).onResult(true);
        // Supplier value should now be true.
        assertTrue(supplier.get());
    }

    @Test
    public void testWithTabWithProductInfo_untrackedAndThenTracked() {
        Callback<Boolean> mockCallback = mock(Callback.class);
        long productClusterId = 1234L;
        ShoppingService.ProductInfo productInfo = createProductInfoWithId(productClusterId);

        when(mMockTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_CAT);

        ArgumentCaptor<SubscriptionsObserver> subscriptionsObserverArgumentCaptor =
                ArgumentCaptor.forClass(SubscriptionsObserver.class);
        ArgumentCaptor<CommerceSubscription> commerceSubscriptionArgumentCaptor =
                ArgumentCaptor.forClass(CommerceSubscription.class);
        ArgumentCaptor<Callback<Boolean>> shoppingServiceCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);

        var supplier = new CurrentTabPriceTrackingStateSupplier(mTabSupplier, mProfileSupplier);
        supplier.addObserver(mockCallback);

        mProfileSupplier.set(mMockProfile);
        mTabSupplier.set(mMockTab);

        verify(mMockShoppingService)
                .getProductInfoForUrl(
                        eq(JUnitTestGURLs.GOOGLE_URL_CAT), mProductInfoCallbackCaptor.capture());
        // Return product info for the current tab.
        mProductInfoCallbackCaptor.getValue().onResult(JUnitTestGURLs.GOOGLE_URL_CAT, productInfo);

        verify(mMockShoppingService)
                .isSubscribed(
                        commerceSubscriptionArgumentCaptor.capture(),
                        shoppingServiceCallbackCaptor.capture());
        // Ensure the supplier is observing for subscription changes.
        verify(mMockShoppingService)
                .addSubscriptionsObserver(subscriptionsObserverArgumentCaptor.capture());
        // Set ShoppingService to return false on the callback to isSubscribed, indicating that the
        // product is not tracked when the tab loaded.
        shoppingServiceCallbackCaptor.getValue().onResult(false);
        // Invoke subscription change listener notifying that the product is now subscribed to.
        subscriptionsObserverArgumentCaptor
                .getValue()
                .onSubscribe(commerceSubscriptionArgumentCaptor.getValue(), true);

        // Supplier should invoke callback.
        verify(mockCallback).onResult(true);
        // Supplier value should now be true.
        assertTrue(supplier.get());
    }

    @Test
    public void testWithTabWithProductInfo_trackedAndThenUnTracked() {
        Callback<Boolean> mockCallback = mock(Callback.class);
        long productClusterId = 1234L;
        ShoppingService.ProductInfo productInfo = createProductInfoWithId(productClusterId);

        when(mMockTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_CAT);

        ArgumentCaptor<SubscriptionsObserver> subscriptionsObserverArgumentCaptor =
                ArgumentCaptor.forClass(SubscriptionsObserver.class);
        ArgumentCaptor<CommerceSubscription> commerceSubscriptionArgumentCaptor =
                ArgumentCaptor.forClass(CommerceSubscription.class);
        ArgumentCaptor<Callback<Boolean>> shoppingServiceCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);

        var supplier = new CurrentTabPriceTrackingStateSupplier(mTabSupplier, mProfileSupplier);
        supplier.addObserver(mockCallback);

        mProfileSupplier.set(mMockProfile);
        mTabSupplier.set(mMockTab);

        verify(mMockShoppingService)
                .getProductInfoForUrl(
                        eq(JUnitTestGURLs.GOOGLE_URL_CAT), mProductInfoCallbackCaptor.capture());
        // Return product info for the current tab.
        mProductInfoCallbackCaptor.getValue().onResult(JUnitTestGURLs.GOOGLE_URL_CAT, productInfo);

        verify(mMockShoppingService)
                .isSubscribed(
                        commerceSubscriptionArgumentCaptor.capture(),
                        shoppingServiceCallbackCaptor.capture());
        verify(mMockShoppingService)
                .addSubscriptionsObserver(subscriptionsObserverArgumentCaptor.capture());
        // Set ShoppingService to return true on the callback to isSubscribed, indicating that the
        // product is tracked when the tab loaded.
        shoppingServiceCallbackCaptor.getValue().onResult(true);

        // Invoke subscription change listener notifying that the product is now unsubscribed to.
        subscriptionsObserverArgumentCaptor
                .getValue()
                .onUnsubscribe(commerceSubscriptionArgumentCaptor.getValue(), true);

        // Supplier callback should have been called twice, once on start indicating the product was
        // tracked then again indicating the product is no longer tracked.
        verify(mockCallback).onResult(true);
        verify(mockCallback).onResult(false);
        // Supplier value should now be false.
        assertFalse(supplier.get());
    }

    @Test
    public void testWithTabWithProductInfo_tabChangesWhileLoading() {
        Tab anotherTab = mock(Tab.class);
        Callback<Boolean> mockCallback = mock(Callback.class);
        long productClusterId = 1234L;
        ShoppingService.ProductInfo productInfo = createProductInfoWithId(productClusterId);

        when(mMockTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_CAT);
        when(anotherTab.getUrl()).thenReturn(JUnitTestGURLs.GOOGLE_URL_DOG);

        ArgumentCaptor<Callback<Boolean>> shoppingServiceCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);

        var supplier = new CurrentTabPriceTrackingStateSupplier(mTabSupplier, mProfileSupplier);
        supplier.addObserver(mockCallback);

        mProfileSupplier.set(mMockProfile);
        mTabSupplier.set(mMockTab);

        verify(mMockShoppingService)
                .getProductInfoForUrl(
                        eq(JUnitTestGURLs.GOOGLE_URL_CAT), mProductInfoCallbackCaptor.capture());
        // Return product info for the current tab.
        mProductInfoCallbackCaptor.getValue().onResult(JUnitTestGURLs.GOOGLE_URL_CAT, productInfo);

        verify(mMockShoppingService).isSubscribed(any(), shoppingServiceCallbackCaptor.capture());

        // Change current tab.
        mTabSupplier.set(anotherTab);

        // Set ShoppingService to return true on the callback to isSubscribed.
        shoppingServiceCallbackCaptor.getValue().onResult(true);

        // Supplier shouldn't invoke callback, because the result of isSubscribed doesn't correspond
        // with the current tab.
        verify(mockCallback, never()).onResult(anyBoolean());
        assertFalse(supplier.get());
    }
}
