// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.IdentifierType;
import org.chromium.components.commerce.core.ManagementType;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.SubscriptionType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;

/** Unit tests for {@link BookmarkSaveFlowMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
@DisableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
public class BookmarkSaveFlowMediatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public final Features.JUnitProcessor mProcessor = new Features.JUnitProcessor();

    private BookmarkSaveFlowMediator mMediator;
    private PropertyModel mPropertyModel =
            new PropertyModel(ImprovedBookmarkSaveFlowProperties.ALL_KEYS);

    @Mock private Context mContext;
    @Mock private Runnable mCloseRunnable;
    @Mock private BookmarkModel mModel;
    @Mock private ShoppingService mShoppingService;
    @Mock private CommerceSubscription mSubscription;
    @Mock private BookmarkImageFetcher mBookmarkImageFetcher;
    @Mock private Profile mProfile;

    @Before
    public void setup() {
        mMediator =
                new BookmarkSaveFlowMediator(
                        mModel,
                        mPropertyModel,
                        mContext,
                        mCloseRunnable,
                        mShoppingService,
                        mBookmarkImageFetcher,
                        mProfile);
        mMediator.setSubscriptionForTesting(mSubscription);
    }

    @Test
    public void testSubscribedInBackground() {
        mMediator.setPriceTrackingToggleVisualsOnly(false);
        Assert.assertFalse(
                mPropertyModel.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED));
        Assert.assertEquals(
                R.drawable.price_tracking_disabled,
                (int)
                        mPropertyModel.get(
                                BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON_RES));

        mMediator.onSubscribe(mSubscription, true);
        Assert.assertTrue(
                mPropertyModel.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED));
        Assert.assertEquals(
                R.drawable.price_tracking_enabled_filled,
                (int)
                        mPropertyModel.get(
                                BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON_RES));
        Mockito.verify(mShoppingService, Mockito.never())
                .subscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
        Mockito.verify(mShoppingService, Mockito.never())
                .unsubscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testSubscribedInBackground_improved() {
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

    // Ensure the toggle logic still works when the subscription object changes but has identical
    // information.
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
                mPropertyModel.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED));
        Assert.assertEquals(
                R.drawable.price_tracking_disabled,
                (int)
                        mPropertyModel.get(
                                BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON_RES));

        mMediator.onSubscribe(clone, true);
        Assert.assertTrue(
                mPropertyModel.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED));
        Assert.assertEquals(
                R.drawable.price_tracking_enabled_filled,
                (int)
                        mPropertyModel.get(
                                BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON_RES));
        Mockito.verify(mShoppingService, Mockito.never())
                .subscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
        Mockito.verify(mShoppingService, Mockito.never())
                .unsubscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testSubscribed_differentObjects_improved() {
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
                mPropertyModel.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED));
        Assert.assertEquals(
                R.drawable.price_tracking_enabled_filled,
                (int)
                        mPropertyModel.get(
                                BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON_RES));

        mMediator.onUnsubscribe(mSubscription, true);
        Assert.assertFalse(
                mPropertyModel.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED));
        Assert.assertEquals(
                R.drawable.price_tracking_disabled,
                (int)
                        mPropertyModel.get(
                                BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON_RES));
        Mockito.verify(mShoppingService, Mockito.never())
                .subscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
        Mockito.verify(mShoppingService, Mockito.never())
                .unsubscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_IMPROVED_BOOKMARKS)
    public void testUnsubscribedInBackground_improved() {
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
}
