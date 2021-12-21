// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.PendingIntent;
import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Ignore;
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
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowPendingIntent;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.ActionType;
import org.chromium.chrome.browser.price_tracking.PriceDropNotifier.ActionData;
import org.chromium.chrome.browser.price_tracking.PriceDropNotifier.NotificationData;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.image_fetcher.ImageFetcher;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit test for {@link PriceDropNotifier}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {PriceDropNotifierUnitTest.ShadowPriceDropNotificationManager.class})
public class PriceDropNotifierUnitTest {
    private static final String TITLE = "title";
    private static final String TEXT = "text";
    private static final String ICON_URL = "http://www.example.com/icon";
    private static final String DESTINATION_URL = "http://www.example.com/destination";
    private static final String OFFER_ID = "offer_id";
    private static final String ACTION_TEXT_0 = "action_text_0";
    private static final String ACTION_TEXT_1 = "action_text_1";

    /**
     * Shadow class for {@link PriceDropNotificationManager}.
     */
    @Implements(PriceDropNotificationManager.class)
    public static class ShadowPriceDropNotificationManager {
        static String sLastActionId;
        static String sLastUrl;
        static String sLastOfferId;

        public ShadowPriceDropNotificationManager() {}

        @Resetter
        public static void reset() {
            sLastActionId = null;
            sLastUrl = null;
            sLastOfferId = null;
        }

        @Implementation
        public void onNotificationActionClicked(
                String actionId, String url, @Nullable String offerId) {
            sLastActionId = actionId;
            sLastUrl = url;
            sLastOfferId = offerId;
        }
    }

    static class TestPriceDropNotifier extends PriceDropNotifier {
        private final ImageFetcher mMockImageFetcher;

        TestPriceDropNotifier(Context context, ImageFetcher imageFetcher,
                NotificationBuilderFactory notificationBuilderFactory,
                NotificationManagerProxy notificationManager) {
            super(context, notificationBuilderFactory, notificationManager);
            mMockImageFetcher = imageFetcher;
        }

        @Override
        protected ImageFetcher getImageFetcher() {
            return mMockImageFetcher;
        }
    }

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    private ImageFetcher mImageFetcher;
    @Mock
    private PriceDropNotifier.NotificationBuilderFactory mNotificationBuilderFactory;
    @Mock
    private NotificationWrapperBuilder mNotificationBuilder;
    @Mock
    private NotificationManagerProxy mNotificationManagerProxy;
    @Mock
    private ChromeBrowserInitializer mChromeInitializer;
    @Mock
    private NotificationWrapper mNotificationWrapper;

    @Captor
    ArgumentCaptor<Callback<Bitmap>> mBitmapCallbackCaptor;

    @Captor
    ArgumentCaptor<PendingIntentProvider> mPendingIntentProviderCaptor;

    PriceDropNotifier mPriceDropNotifier;

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;
        mPriceDropNotifier = new TestPriceDropNotifier(ContextUtils.getApplicationContext(),
                mImageFetcher, mNotificationBuilderFactory, mNotificationManagerProxy);
        ChromeBrowserInitializer.setForTesting(mChromeInitializer);
        when(mNotificationBuilderFactory.createNotificationBuilder())
                .thenReturn(mNotificationBuilder);
        when(mNotificationBuilder.buildNotificationWrapper()).thenReturn(mNotificationWrapper);
    }

    @After
    public void tearDown() {
        mPriceDropNotifier = null;
        ChromeBrowserInitializer.setForTesting(null);
        ShadowPriceDropNotificationManager.reset();
    }

    private void showNotification() {
        List<ActionData> actionDataList = new ArrayList<>();
        actionDataList.add(
                new ActionData(PriceDropNotificationManager.ACTION_ID_VISIT_SITE, ACTION_TEXT_0));
        actionDataList.add(new ActionData(
                PriceDropNotificationManager.ACTION_ID_TURN_OFF_ALERT, ACTION_TEXT_1));
        showNotification(actionDataList);
    }

    private void showNotification(List<ActionData> actionDataList) {
        PriceDropNotifier.NotificationData data = new NotificationData(
                TITLE, TEXT, ICON_URL, DESTINATION_URL, OFFER_ID, actionDataList);
        mPriceDropNotifier.showNotification(data);
    }

    private void invokeImageFetcherCallback(Bitmap bitmap) {
        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        mBitmapCallbackCaptor.getValue().onResult(bitmap);
    }

    private void sendPendingIntent(PendingIntent pendingIntent) {
        // Simulate to send a PendingIntent by manually starting the TrampolineActivity.
        ShadowPendingIntent shadowPendingIntent = Shadows.shadowOf(pendingIntent);
        Robolectric
                .buildActivity(PriceDropNotificationManager.TrampolineActivity.class,
                        shadowPendingIntent.getSavedIntent())
                .create();
    }

    private void verifySetNotificationProperties() {
        verify(mNotificationBuilder, times(1)).setContentTitle(eq(TITLE));
        verify(mNotificationBuilder, times(1)).setContentText(eq(TEXT));
        verify(mNotificationBuilder, times(1)).setContentIntent(any(PendingIntentProvider.class));
        verify(mNotificationBuilder, times(1)).setSmallIcon(anyInt());
        verify(mNotificationBuilder, times(1)).setTimeoutAfter(anyLong());
    }

    @Test
    public void testShowNotificationImageFetcherFailure() {
        showNotification(/*actionDataList=*/null);
        invokeImageFetcherCallback(null);
        verify(mNotificationBuilder, times(0)).setLargeIcon(any());
        verifySetNotificationProperties();
        verify(mNotificationManagerProxy).notify(any());
    }

    @Test
    public void testShowNotificationNoIconURL() {
        PriceDropNotifier.NotificationData data = new NotificationData(
                TITLE, TEXT, /*iconUrl=*/null, DESTINATION_URL, OFFER_ID, null);
        mPriceDropNotifier.showNotification(data);
        verify(mNotificationBuilder, times(0)).setLargeIcon(any());
        verify(mNotificationBuilder, times(0)).setBigPictureStyle(any(), any());
        verifySetNotificationProperties();
        verify(mNotificationManagerProxy).notify(any());
    }

    @Test
    public void testShowNotificationWithLargeIcon() {
        showNotification();
        invokeImageFetcherCallback(Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888));
        verify(mNotificationBuilder).setLargeIcon(any());
        verify(mNotificationBuilder).setBigPictureStyle(any(), eq(TEXT));
        verifySetNotificationProperties();
        verify(mNotificationManagerProxy).notify(any());
    }

    // TODO(xingliu): Figure out why test framework crashes and enable this.
    @Ignore
    @Test
    public void testNotificationTurnOffAlertClick() {
        doAnswer(invocation -> {
            Runnable task = invocation.getArgument(0);
            task.run();
            return null;
        })
                .when(mChromeInitializer)
                .runNowOrAfterFullBrowserStarted(any());
        showNotification();
        invokeImageFetcherCallback(null);
        verify(mNotificationBuilder)
                .addAction(eq(0), eq(ACTION_TEXT_1), mPendingIntentProviderCaptor.capture(),
                        eq(ActionType.PRICE_DROP_TURN_OFF_ALERT));
        verify(mNotificationBuilder)
                .addAction(eq(0), eq(ACTION_TEXT_0), any(), eq(ActionType.PRICE_DROP_VISIT_SITE));

        sendPendingIntent(mPendingIntentProviderCaptor.getValue().getPendingIntent());
        Assert.assertEquals(PriceDropNotificationManager.ACTION_ID_TURN_OFF_ALERT,
                ShadowPriceDropNotificationManager.sLastActionId);
        Assert.assertEquals(DESTINATION_URL, ShadowPriceDropNotificationManager.sLastUrl);
        Assert.assertEquals(OFFER_ID, ShadowPriceDropNotificationManager.sLastOfferId);
    }
}