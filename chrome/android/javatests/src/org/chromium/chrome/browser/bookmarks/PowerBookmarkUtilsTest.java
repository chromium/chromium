// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import com.google.common.primitives.UnsignedLongs;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ProductPrice;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;

/** Tests for PowerBookmarkUtils. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PowerBookmarkUtilsTest {
    @Mock
    private BookmarkModel mMockBookmarkModel;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        when(mMockBookmarkModel.isBookmarkModelLoaded()).thenReturn(true);
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
