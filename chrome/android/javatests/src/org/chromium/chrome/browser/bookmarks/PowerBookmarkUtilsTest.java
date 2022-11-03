// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import com.google.common.collect.ImmutableMap;
import com.google.common.primitives.UnsignedLongs;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription;
import org.chromium.chrome.browser.subscriptions.SubscriptionsManager;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.PowerBookmarkType;
import org.chromium.components.power_bookmarks.ProductPrice;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;

import java.util.ArrayList;
import java.util.List;

/** Tests for PowerBookmarkUtils. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PowerBookmarkUtilsTest {
    @Mock
    private BookmarkModel mMockBookmarkModel;

    @Mock
    private SubscriptionsManager mMockSubscriptionsManager;

    @Captor
    private ArgumentCaptor<Callback<List<CommerceSubscription>>> mGetSubscriptionsCallbackCaptor;

    @Captor
    private ArgumentCaptor<List<CommerceSubscription>> mUnsubscribeListCaptor;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        when(mMockBookmarkModel.isBookmarkModelLoaded()).thenReturn(true);
    }

    // Ensure a product that is not tracked locally is unsubscribed on the backend.
    @Test
    @SmallTest
    public void testNotTrackedLocally() {
        FeatureList.setTestFeatures(
                ImmutableMap.of(ChromeFeatureList.SHOPPING_LIST_ENABLE_DESYNC_RESOLUTION, true));
        ArrayList<BookmarkId> searchIds = new ArrayList<>();
        BookmarkId bookmark = setUpBookmarkWithMetaInModel(0, "1234", false);
        searchIds.add(bookmark);

        when(mMockBookmarkModel.searchBookmarks(
                     anyString(), any(), eq(PowerBookmarkType.SHOPPING), anyInt()))
                .thenReturn(searchIds);

        PowerBookmarkUtils.validateBookmarkedCommerceSubscriptions(
                mMockBookmarkModel, mMockSubscriptionsManager);

        verify(mMockSubscriptionsManager)
                .getSubscriptions(eq(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK),
                        anyBoolean(), mGetSubscriptionsCallbackCaptor.capture());

        ArrayList<CommerceSubscription> subscriptions = new ArrayList<>();
        CommerceSubscription subscription = buildSubscription("1234");
        subscriptions.add(subscription);

        mGetSubscriptionsCallbackCaptor.getValue().onResult(subscriptions);

        verify(mMockSubscriptionsManager).unsubscribe(mUnsubscribeListCaptor.capture(), any());

        Assert.assertTrue(
                "The list of unsibscribed items did not contain the correct subscription!",
                mUnsubscribeListCaptor.getValue().contains(subscription));

        // No bookmark meta updates should have occurred.
        verify(mMockBookmarkModel, times(0)).setPowerBookmarkMeta(any(), any());
    }

    // If a subscription doesn't have a bookmark, the subscription should be removed.
    @Test
    @SmallTest
    public void testNoBookmarks() {
        FeatureList.setTestFeatures(
                ImmutableMap.of(ChromeFeatureList.SHOPPING_LIST_ENABLE_DESYNC_RESOLUTION, true));
        ArrayList<BookmarkId> searchIds = new ArrayList<>();

        when(mMockBookmarkModel.searchBookmarks(
                     anyString(), any(), eq(PowerBookmarkType.SHOPPING), anyInt()))
                .thenReturn(searchIds);

        PowerBookmarkUtils.validateBookmarkedCommerceSubscriptions(
                mMockBookmarkModel, mMockSubscriptionsManager);

        verify(mMockSubscriptionsManager)
                .getSubscriptions(eq(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK),
                        anyBoolean(), mGetSubscriptionsCallbackCaptor.capture());

        ArrayList<CommerceSubscription> subscriptions = new ArrayList<>();
        CommerceSubscription subscription = buildSubscription("1234");
        subscriptions.add(subscription);

        mGetSubscriptionsCallbackCaptor.getValue().onResult(subscriptions);

        verify(mMockSubscriptionsManager).unsubscribe(mUnsubscribeListCaptor.capture(), any());

        Assert.assertTrue(
                "The list of unsibscribed items did not contain the correct subscription!",
                mUnsubscribeListCaptor.getValue().contains(subscription));
    }

    // If a bookmark is tracked locally but there is no subscription, unset the flag in the
    // bookmark.
    @Test
    @SmallTest
    public void testNoSubscriptionForBookmark() {
        FeatureList.setTestFeatures(
                ImmutableMap.of(ChromeFeatureList.SHOPPING_LIST_ENABLE_DESYNC_RESOLUTION, true));
        ArrayList<BookmarkId> searchIds = new ArrayList<>();
        BookmarkId bookmark = setUpBookmarkWithMetaInModel(0, "1234", true);
        searchIds.add(bookmark);

        when(mMockBookmarkModel.searchBookmarks(
                     anyString(), any(), eq(PowerBookmarkType.SHOPPING), anyInt()))
                .thenReturn(searchIds);

        PowerBookmarkUtils.validateBookmarkedCommerceSubscriptions(
                mMockBookmarkModel, mMockSubscriptionsManager);

        verify(mMockSubscriptionsManager)
                .getSubscriptions(eq(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK),
                        anyBoolean(), mGetSubscriptionsCallbackCaptor.capture());

        ArrayList<CommerceSubscription> subscriptions = new ArrayList<>();

        mGetSubscriptionsCallbackCaptor.getValue().onResult(subscriptions);

        verify(mMockBookmarkModel).setPowerBookmarkMeta(eq(bookmark), any());

        // Unsubscribe should have never been invoked.
        verify(mMockSubscriptionsManager, times(0)).unsubscribe(any(List.class), any());
    }

    // Ensure no bookmark updates or unsubscribe events occur if everything is aligned.
    @Test
    @SmallTest
    public void testBookmarksAndSubscriptionsAligned() {
        FeatureList.setTestFeatures(
                ImmutableMap.of(ChromeFeatureList.SHOPPING_LIST_ENABLE_DESYNC_RESOLUTION, true));
        ArrayList<BookmarkId> searchIds = new ArrayList<>();
        BookmarkId bookmark1 = setUpBookmarkWithMetaInModel(0, "1234", true);
        searchIds.add(bookmark1);
        BookmarkId bookmark2 = setUpBookmarkWithMetaInModel(1, "5678", false);
        searchIds.add(bookmark2);

        when(mMockBookmarkModel.searchBookmarks(
                     anyString(), any(), eq(PowerBookmarkType.SHOPPING), anyInt()))
                .thenReturn(searchIds);

        PowerBookmarkUtils.validateBookmarkedCommerceSubscriptions(
                mMockBookmarkModel, mMockSubscriptionsManager);

        verify(mMockSubscriptionsManager)
                .getSubscriptions(eq(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK),
                        anyBoolean(), mGetSubscriptionsCallbackCaptor.capture());

        ArrayList<CommerceSubscription> subscriptions = new ArrayList<>();
        CommerceSubscription subscription = buildSubscription("1234");
        subscriptions.add(subscription);

        mGetSubscriptionsCallbackCaptor.getValue().onResult(subscriptions);

        // Unsubscribe should have never been invoked.
        verify(mMockSubscriptionsManager, times(0)).unsubscribe(any(List.class), any());

        // No bookmark meta updates should have occurred.
        verify(mMockBookmarkModel, times(0)).setPowerBookmarkMeta(any(), any());
    }

    @Test
    @SmallTest
    public void testCreateCommerceSubscriptionForPowerBookmarkMeta() {
        ShoppingSpecifics specifics =
                ShoppingSpecifics.newBuilder()
                        .setProductClusterId(123)
                        .setOfferId(456)
                        .setCountryCode("us")
                        .setCurrentPrice(ProductPrice.newBuilder().setAmountMicros(100).build())
                        .build();
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder().setShoppingSpecifics(specifics).build();
        CommerceSubscription subscription =
                PowerBookmarkUtils.createCommerceSubscriptionForPowerBookmarkMeta(meta);

        Assert.assertEquals(CommerceSubscription.TrackingIdType.PRODUCT_CLUSTER_ID,
                subscription.getTrackingIdType());
        Assert.assertEquals(CommerceSubscription.SubscriptionManagementType.USER_MANAGED,
                subscription.getManagementType());
        Assert.assertEquals("123", subscription.getTrackingId());
        Assert.assertEquals("456", subscription.getSeenOffer().offerId);
        Assert.assertEquals("100", subscription.getSeenOffer().currentPrice);
        Assert.assertEquals("us", subscription.getSeenOffer().countryCode);
    }

    /**
     * @param clusterId The cluster ID the subscription should be created with.
     * @return A user-managed subscription with the specified ID.
     */
    private CommerceSubscription buildSubscription(String clusterId) {
        return new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                clusterId, CommerceSubscription.SubscriptionManagementType.USER_MANAGED,
                CommerceSubscription.TrackingIdType.PRODUCT_CLUSTER_ID);
    }

    /**
     * @param clusterId The product's cluster ID.
     * @param isPriceTracked Whether the product is price tracked.
     * @return Power bookmark meta for shopping.
     */
    private PowerBookmarkMeta buildPowerBookmarkMeta(String clusterId, boolean isPriceTracked) {
        ShoppingSpecifics specifics =
                ShoppingSpecifics.newBuilder()
                        .setIsPriceTracked(isPriceTracked)
                        .setProductClusterId(UnsignedLongs.parseUnsignedLong(clusterId))
                        .build();
        return PowerBookmarkMeta.newBuilder().setShoppingSpecifics(specifics).build();
    }

    /**
     * Create a mock bookmark and set up the mock model to have shopping meta for it.
     * @param bookmarkId The bookmark's ID.
     * @param clusterId The cluster ID for the product.
     * @param isPriceTracked Whether the product is price tracked.
     * @return The bookmark that was created.
     */
    private BookmarkId setUpBookmarkWithMetaInModel(
            long bookmarkId, String clusterId, boolean isPriceTracked) {
        BookmarkId bookmark = new BookmarkId(bookmarkId, BookmarkType.NORMAL);
        when(mMockBookmarkModel.getPowerBookmarkMeta(bookmark))
                .thenReturn(buildPowerBookmarkMeta(clusterId, isPriceTracked));
        return bookmark;
    }
}
