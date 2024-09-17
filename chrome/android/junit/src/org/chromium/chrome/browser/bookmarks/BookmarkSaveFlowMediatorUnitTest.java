// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.ContextThemeWrapper;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.commerce.PriceTrackingUtils;
import org.chromium.chrome.browser.commerce.PriceTrackingUtilsJni;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.IdentifierType;
import org.chromium.components.commerce.core.ManagementType;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.SubscriptionType;
import org.chromium.components.commerce.core.SubscriptionsObserver;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.ProductPrice;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;
import org.chromium.url.GURL;

/** Unit tests for {@link BookmarkSaveFlowMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class BookmarkSaveFlowMediatorUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    private BookmarkSaveFlowMediator mMediator;
    private PropertyModel mPropertyModel =
            new PropertyModel(ImprovedBookmarkSaveFlowProperties.ALL_KEYS);

    private Context mContext;
    @Mock private Runnable mCloseRunnable;
    @Mock private BookmarkModel mModel;
    @Mock private ShoppingService mShoppingService;
    @Mock private CommerceSubscription mSubscription;
    @Mock private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock private Profile mProfile;
    @Mock private IdentityManager mIdentityManager;
    @Mock private PriceTrackingUtils.Natives mMockPriceTrackingUtilsJni;
    @Mock private PriceDropNotificationManager mMockNotificationManager;
    @Captor private ArgumentCaptor<SubscriptionsObserver> mSubscriptionsObserverCaptor;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(PriceTrackingUtilsJni.TEST_HOOKS, mMockPriceTrackingUtilsJni);
        PriceDropNotificationManagerFactory.setInstanceForTesting(mMockNotificationManager);
        mMediator =
                new BookmarkSaveFlowMediator(
                        mModel,
                        mPropertyModel,
                        mContext,
                        mCloseRunnable,
                        mShoppingService,
                        mBookmarkImageFetcher,
                        mProfile,
                        mIdentityManager);
        mMediator.setSubscriptionForTesting(mSubscription);
        Mockito.verify(mShoppingService)
                .addSubscriptionsObserver(mSubscriptionsObserverCaptor.capture());
    }

    @Test
    public void testShow() {
        BookmarkId bookmarkId = new BookmarkId(0, 0);
        BookmarkItem item =
                new BookmarkItem(
                        bookmarkId,
                        "",
                        new GURL("http://example.com"),
                        false,
                        new BookmarkId(1, 0),
                        true,
                        false,
                        0,
                        false,
                        0,
                        false);
        Mockito.doReturn(item).when(mModel).getBookmarkById(Mockito.any());
        Mockito.doReturn("title").when(mModel).getBookmarkTitle(Mockito.any());

        mMediator.show(bookmarkId, null, /* fromExplicitTrackUi= */ false, false, false);

        Mockito.verify(mMockPriceTrackingUtilsJni, Mockito.never())
                .setPriceTrackingStateForBookmark(
                        Mockito.any(),
                        Mockito.anyLong(),
                        Mockito.anyBoolean(),
                        Mockito.any(),
                        Mockito.anyBoolean());
    }

    // Tests related to price-tracking bookmarks.

    @Test
    public void testShow_FromExplicitPriceTracking() {
        BookmarkId bookmarkId = new BookmarkId(0, 0);
        PowerBookmarkMeta meta = PowerBookmarkMeta.newBuilder().build();
        BookmarkItem item =
                new BookmarkItem(
                        bookmarkId,
                        "",
                        new GURL("http://example.com"),
                        false,
                        new BookmarkId(1, 0),
                        true,
                        false,
                        0,
                        false,
                        0,
                        false);
        Mockito.doReturn(item).when(mModel).getBookmarkById(Mockito.any());
        Mockito.doReturn("title").when(mModel).getBookmarkTitle(Mockito.any());

        mMediator.show(bookmarkId, meta, /* fromExplicitTrackUi= */ true, false, false);

        Mockito.verify(mMockPriceTrackingUtilsJni)
                .setPriceTrackingStateForBookmark(
                        Mockito.any(),
                        Mockito.anyLong(),
                        Mockito.eq(true),
                        Mockito.any(),
                        Mockito.anyBoolean());
    }

    @Test
    public void testSubscribedInBackground() {
        mMediator.setPriceTrackingToggleVisualsOnly(false);
        Assert.assertFalse(
                mPropertyModel.get(
                        ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED));

        mMediator.onSubscribe(mSubscription, true);
        Assert.assertTrue(
                mPropertyModel.get(
                        ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED));
        Mockito.verify(mShoppingService, Mockito.never())
                .subscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
        Mockito.verify(mShoppingService, Mockito.never())
                .unsubscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
    }

    @Test
    public void testSubscribed_differentObjects() {
        String clusterId = "1234";
        CommerceSubscription original =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        clusterId,
                        ManagementType.USER_MANAGED,
                        null);
        CommerceSubscription clone =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        clusterId,
                        ManagementType.USER_MANAGED,
                        null);

        mMediator.setSubscriptionForTesting(original);

        mMediator.setPriceTrackingToggleVisualsOnly(false);
        Assert.assertFalse(
                mPropertyModel.get(
                        ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED));

        mMediator.onSubscribe(clone, true);
        Assert.assertTrue(
                mPropertyModel.get(
                        ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED));
        Mockito.verify(mShoppingService, Mockito.never())
                .subscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
        Mockito.verify(mShoppingService, Mockito.never())
                .unsubscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
    }

    @Test
    public void testUnsubscribedInBackground() {
        mMediator.setPriceTrackingToggleVisualsOnly(true);
        Assert.assertTrue(
                mPropertyModel.get(
                        ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED));

        mMediator.onUnsubscribe(mSubscription, true);
        Assert.assertFalse(
                mPropertyModel.get(
                        ImprovedBookmarkSaveFlowProperties.PRICE_TRACKING_SWITCH_CHECKED));
        Mockito.verify(mShoppingService, Mockito.never())
                .subscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
        Mockito.verify(mShoppingService, Mockito.never())
                .unsubscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
    }

    @Test
    public void subscriptionCreatesNotificationChannel() {
        BookmarkId bookmarkId = new BookmarkId(0, 0);
        ShoppingSpecifics specifics =
                ShoppingSpecifics.newBuilder()
                        .setProductClusterId(123)
                        .setOfferId(456)
                        .setCountryCode("us")
                        .setCurrentPrice(ProductPrice.newBuilder().setAmountMicros(100).build())
                        .build();
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder().setShoppingSpecifics(specifics).build();
        CommerceSubscription sub =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        /* id= */ "123",
                        ManagementType.USER_MANAGED,
                        /* userSeenOffer= */ null);
        mMediator.setSubscriptionForTesting(sub);
        BookmarkItem item =
                new BookmarkItem(
                        bookmarkId,
                        /* title= */ "",
                        new GURL("http://example.com"),
                        /* isFolder= */ false,
                        /* parentId= */ new BookmarkId(1, 0),
                        /* isEditable= */ true,
                        /* isManaged= */ false,
                        /* dateAdded= */ 0,
                        /* read= */ false,
                        /* dateLastOpened= */ 0,
                        /* isAccountBookmark= */ false);
        Mockito.doReturn(item).when(mModel).getBookmarkById(Mockito.any());
        Mockito.doReturn("title").when(mModel).getBookmarkTitle(Mockito.any());

        mMediator.show(bookmarkId, meta, /* fromExplicitTrackUi= */ true, false, false);

        // We should see a call to subscribe to the product.
        Mockito.verify(mMockPriceTrackingUtilsJni)
                .setPriceTrackingStateForBookmark(
                        Mockito.any(),
                        Mockito.anyLong(),
                        Mockito.eq(true),
                        Mockito.any(),
                        Mockito.anyBoolean());

        // Simulate a successful subscription.
        mSubscriptionsObserverCaptor.getValue().onSubscribe(sub, true);

        Mockito.verify(mMockNotificationManager).createNotificationChannel();
    }
}
