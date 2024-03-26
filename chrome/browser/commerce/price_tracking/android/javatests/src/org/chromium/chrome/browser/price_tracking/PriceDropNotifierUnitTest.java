// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowPendingIntent;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.price_tracking.PriceDropNotifier.ActionData;
import org.chromium.chrome.browser.price_tracking.PriceDropNotifier.NotificationData;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.image_fetcher.ImageFetcher;

import java.util.ArrayList;
import java.util.List;

/** Unit test for {@link PriceDropNotifier}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceDropNotifierUnitTest {
    private static final String TITLE = "title";
    private static final String TEXT = "text";
    private static final String ICON_URL = "http://www.example.com/icon";
    private static final String DESTINATION_URL = "http://www.example.com/destination";
    private static final String OFFER_ID = "offer_id";
    private static final String PRODUCT_CLUSTER_ID = "cluster_id";
    private static final String ACTION_TEXT_0 = "action_text_0";
    private static final String ACTION_TEXT_1 = "action_text_1";

    static class TestPriceDropNotifier extends PriceDropNotifier {
        private final ImageFetcher mMockImageFetcher;
        private final NotificationWrapperBuilder mMockNotificationBuilder;

        TestPriceDropNotifier(
                Context context,
                Profile profile,
                ImageFetcher imageFetcher,
                NotificationWrapperBuilder notificationBuilder,
                NotificationManagerProxy notificationManager) {
            super(context, profile, notificationManager);
            mMockImageFetcher = imageFetcher;
            mMockNotificationBuilder = notificationBuilder;
        }

        @Override
        protected ImageFetcher getImageFetcher() {
            return mMockImageFetcher;
        }

        @Override
        protected NotificationWrapperBuilder getNotificationBuilder(
                @SystemNotificationType int notificationType, int notificationId) {
            return mMockNotificationBuilder;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private ImageFetcher mImageFetcher;
    @Mock private NotificationWrapperBuilder mNotificationBuilder;
    @Mock private NotificationManagerProxy mNotificationManagerProxy;
    @Mock private ChromeBrowserInitializer mChromeInitializer;
    @Mock private NotificationWrapper mNotificationWrapper;
    @Mock private PriceDropNotificationManager mPriceDropNotificationManager;

    @Captor ArgumentCaptor<Callback<Bitmap>> mBitmapCallbackCaptor;

    PriceDropNotifier mPriceDropNotifier;
    Intent mIntent;

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        mPriceDropNotifier =
                new TestPriceDropNotifier(
                        ContextUtils.getApplicationContext(),
                        mProfile,
                        mImageFetcher,
                        mNotificationBuilder,
                        mNotificationManagerProxy);
        mPriceDropNotifier.setPriceDropNotificationManagerForTesting(mPriceDropNotificationManager);
        mIntent = new Intent();
        ChromeBrowserInitializer.setForTesting(mChromeInitializer);
        when(mNotificationBuilder.buildNotificationWrapper()).thenReturn(mNotificationWrapper);
        doReturn(false)
                .when(mPriceDropNotificationManager)
                .hasReachedMaxAllowedNotificationNumber(anyInt());
        doReturn(mIntent)
                .when(mPriceDropNotificationManager)
                .getNotificationClickIntent(any(), anyInt());
        doReturn(mIntent)
                .when(mPriceDropNotificationManager)
                .getNotificationActionClickIntent(any(), any(), any(), any(), anyInt());
    }

    private void showNotification() {
        List<ActionData> actionDataList = new ArrayList<>();
        actionDataList.add(
                new ActionData(
                        PriceDropNotificationManagerImpl.ACTION_ID_VISIT_SITE, ACTION_TEXT_0));
        actionDataList.add(
                new ActionData(
                        PriceDropNotificationManagerImpl.ACTION_ID_TURN_OFF_ALERT, ACTION_TEXT_1));
        showNotification(actionDataList);
    }

    private void showNotification(List<ActionData> actionDataList) {
        PriceDropNotifier.NotificationData data =
                new NotificationData(
                        TITLE,
                        TEXT,
                        ICON_URL,
                        DESTINATION_URL,
                        OFFER_ID,
                        PRODUCT_CLUSTER_ID,
                        actionDataList);
        mPriceDropNotifier.showNotification(data);
    }

    private void invokeImageFetcherCallback(Bitmap bitmap) {
        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        mBitmapCallbackCaptor.getValue().onResult(bitmap);
    }

    private void sendPendingIntent(PendingIntent pendingIntent) {
        // Simulate to send a PendingIntent by manually starting the TrampolineActivity.
        ShadowPendingIntent shadowPendingIntent = Shadows.shadowOf(pendingIntent);
        Robolectric.buildActivity(
                        PriceDropNotificationManagerImpl.TrampolineActivity.class,
                        shadowPendingIntent.getSavedIntent())
                .create();
    }

    private void verifySetNotificationProperties() {
        verify(mNotificationBuilder, times(1)).setContentTitle(eq(TITLE));
        verify(mNotificationBuilder, times(1)).setContentText(eq(TEXT));
        verify(mNotificationBuilder, times(1)).setContentIntent(any(PendingIntentProvider.class));
        verify(mNotificationBuilder, times(1)).setSmallIcon(anyInt());
        verify(mNotificationBuilder, times(1)).setTimeoutAfter(anyLong());
        verify(mNotificationBuilder, times(1)).setAutoCancel(eq(true));
    }

    @Test
    public void testShowNotificationImageFetcherFailure() {
        showNotification(/* actionDataList= */ null);
        invokeImageFetcherCallback(null);
        verify(mNotificationBuilder, times(0)).setLargeIcon(any());
        verifySetNotificationProperties();
        verify(mNotificationManagerProxy).notify(any());
        verify(mPriceDropNotificationManager, times(1))
                .updateNotificationTimestamps(
                        eq(SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED), eq(true));
    }

    @Test
    public void testShowNotificationNoIconURL() {
        PriceDropNotifier.NotificationData data =
                new NotificationData(
                        TITLE,
                        TEXT,
                        /* iconUrl= */ null,
                        DESTINATION_URL,
                        OFFER_ID,
                        PRODUCT_CLUSTER_ID,
                        null);
        mPriceDropNotifier.showNotification(data);
        verify(mNotificationBuilder, times(0)).setLargeIcon(any());
        verify(mNotificationBuilder, times(0)).setBigPictureStyle(any(), any());
        verifySetNotificationProperties();
        verify(mNotificationManagerProxy).notify(any());
        verify(mPriceDropNotificationManager, times(1))
                .updateNotificationTimestamps(
                        eq(SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED), eq(true));
    }

    @Test
    public void testShowNotificationWithLargeIcon() {
        showNotification();
        invokeImageFetcherCallback(Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888));
        verify(mNotificationBuilder).setLargeIcon(any());
        verify(mNotificationBuilder).setBigPictureStyle(any(), eq(TEXT));
        verifySetNotificationProperties();
        verify(mNotificationManagerProxy).notify(any());
        verify(mPriceDropNotificationManager, times(1))
                .updateNotificationTimestamps(
                        eq(SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED), eq(true));
    }

    @Test
    public void testAlreadyReachedMaxNotificationNumber() {
        doReturn(true)
                .when(mPriceDropNotificationManager)
                .hasReachedMaxAllowedNotificationNumber(anyInt());
        showNotification(/* actionDataList= */ null);
        invokeImageFetcherCallback(null);
        verify(mNotificationManagerProxy, times(0)).notify(any());
        verify(mPriceDropNotificationManager, times(0))
                .updateNotificationTimestamps(
                        eq(SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED), eq(true));
    }
}
