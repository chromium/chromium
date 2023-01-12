// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription;
import org.chromium.chrome.browser.subscriptions.SubscriptionsManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;

import java.util.Arrays;

/** Unit tests for {@link BookmarkSaveFlowMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowAppCompatResources.class})
public class BookmarkSaveFlowMediatorTest {
    private BookmarkSaveFlowMediator mMediator;
    private PropertyModel mPropertyModel =
            new PropertyModel(BookmarkSaveFlowProperties.ALL_PROPERTIES);

    @Mock
    Context mContext;
    @Mock
    Runnable mCloseRunnable;
    @Mock
    BookmarkModel mModel;
    @Mock
    SubscriptionsManager mSubscriptionsManager;
    @Mock
    CommerceSubscription mSubscription;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mMediator = new BookmarkSaveFlowMediator(
                mModel, mPropertyModel, mContext, mCloseRunnable, mSubscriptionsManager);
        mMediator.setSubscriptionForTesting(mSubscription);
    }

    @Test
    public void subscribedInBackground() {
        mMediator.setPriceTrackingToggleVisualsOnly(false);
        Assert.assertFalse(
                mPropertyModel.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED));
        Assert.assertEquals(R.drawable.price_tracking_disabled,
                (int) mPropertyModel.get(
                        BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON_RES));

        mMediator.onSubscribe(Arrays.asList(mSubscription));
        Assert.assertTrue(
                mPropertyModel.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED));
        Assert.assertEquals(R.drawable.price_tracking_enabled_filled,
                (int) mPropertyModel.get(
                        BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON_RES));
        Mockito.verify(mSubscriptionsManager, Mockito.never())
                .subscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
        Mockito.verify(mSubscriptionsManager, Mockito.never())
                .unsubscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
    }

    @Test
    public void unsubscribedInBackground() {
        mMediator.setPriceTrackingToggleVisualsOnly(true);
        Assert.assertTrue(
                mPropertyModel.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED));
        Assert.assertEquals(R.drawable.price_tracking_enabled_filled,
                (int) mPropertyModel.get(
                        BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON_RES));

        mMediator.onUnsubscribe(Arrays.asList(mSubscription));
        Assert.assertFalse(
                mPropertyModel.get(BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_TOGGLED));
        Assert.assertEquals(R.drawable.price_tracking_disabled,
                (int) mPropertyModel.get(
                        BookmarkSaveFlowProperties.NOTIFICATION_SWITCH_START_ICON_RES));
        Mockito.verify(mSubscriptionsManager, Mockito.never())
                .subscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
        Mockito.verify(mSubscriptionsManager, Mockito.never())
                .unsubscribe(Mockito.any(CommerceSubscription.class), Mockito.any());
    }
}
