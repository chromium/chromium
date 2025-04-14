// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.IdentifierType;
import org.chromium.components.commerce.core.ManagementType;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ProductPrice;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;

/** Tests for PowerBookmarkUtils. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PowerBookmarkUtilsTest {
    @Before
    public void setup() {
        BookmarkModel bookmarkModel =
                ThreadUtils.runOnUiThreadBlocking(() -> Mockito.mock(BookmarkModel.class));
        when(bookmarkModel.isBookmarkModelLoaded()).thenReturn(true);
    }

    @Test
    @SmallTest
    public void testCreateCommerceSubscriptionForPowerBookmarkMeta() {
        ShoppingSpecifics specifics =
                ShoppingSpecifics.newBuilder()
                        .setProductClusterId(123)
                        .setOfferId(456)
                        .setCountryCode("us")
                        .setLocale("en-US")
                        .setCurrentPrice(ProductPrice.newBuilder().setAmountMicros(100).build())
                        .build();
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder().setShoppingSpecifics(specifics).build();
        CommerceSubscription subscription =
                PowerBookmarkUtils.createCommerceSubscriptionForPowerBookmarkMeta(meta);

        Assert.assertEquals(IdentifierType.PRODUCT_CLUSTER_ID, subscription.idType);
        Assert.assertEquals(ManagementType.USER_MANAGED, subscription.managementType);
        Assert.assertEquals("123", subscription.id);
        Assert.assertEquals("456", subscription.userSeenOffer.offerId);
        Assert.assertEquals(100L, subscription.userSeenOffer.userSeenPrice);
        Assert.assertEquals("us", subscription.userSeenOffer.countryCode);
        Assert.assertEquals("en-US", subscription.userSeenOffer.locale);
    }

    @Test
    @SmallTest
    public void testCreateCommerceSubscriptionForShoppingSpecifics() {
        ShoppingSpecifics specifics =
                ShoppingSpecifics.newBuilder()
                        .setProductClusterId(123)
                        .setOfferId(456)
                        .setCountryCode("us")
                        .setLocale("en-US")
                        .setCurrentPrice(ProductPrice.newBuilder().setAmountMicros(100).build())
                        .build();
        CommerceSubscription subscription =
                PowerBookmarkUtils.createCommerceSubscriptionForShoppingSpecifics(specifics);

        Assert.assertEquals(IdentifierType.PRODUCT_CLUSTER_ID, subscription.idType);
        Assert.assertEquals(ManagementType.USER_MANAGED, subscription.managementType);
        Assert.assertEquals("123", subscription.id);
        Assert.assertEquals("456", subscription.userSeenOffer.offerId);
        Assert.assertEquals(100L, subscription.userSeenOffer.userSeenPrice);
        Assert.assertEquals("us", subscription.userSeenOffer.countryCode);
        Assert.assertEquals("en-US", subscription.userSeenOffer.locale);
    }
}
