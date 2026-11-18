// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

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

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.ActionType;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.price_tracking.PriceDropNotifier.ActionData;
import org.chromium.chrome.browser.price_tracking.PriceDropNotifier.NotificationData;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.profiles.ProfileTokenUtils;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.image_fetcher.ImageFetcher;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

/** Unit test for {@link PriceDropNotifier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PriceDropNotifierUnitTest {
    private static final String TITLE = "title";
    private static final String TEXT = "text";
    private static final String ICON_URL = "http://www.example.com/icon";
    private static final String DESTINATION_URL = "http://www.example.com/destination";
    private static final String OFFER_ID = "offer_id";
    private static final String OTHER_OFFER_ID = "other_offer_id";
    private static final byte[] PROFILE_TOKEN = "profile_token".getBytes(StandardCharsets.UTF_8);
    private static final byte[] OTHER_PROFILE_TOKEN =
            "other_profile_token".getBytes(StandardCharsets.UTF_8);
    private static final String PRODUCT_CLUSTER_ID = "cluster_id";
    private static final String ACTION_TEXT_0 = "action_text_0";
    private static final String ACTION_TEXT_1 = "action_text_1";

    static class TestPriceDropNotifier extends PriceDropNotifier {
        private final ImageFetcher mMockImageFetcher;
        private final NotificationWrapperBuilder mMockNotificationBuilder;

        TestPriceDropNotifier(
                Profile profile,
                ImageFetcher imageFetcher,
                NotificationWrapperBuilder notificationBuilder) {
            super(profile);
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
    @Mock private Profile mOtherProfile;
    @Mock private ProfileResolver.Natives mProfileResolverNatives;
    @Mock private ImageFetcher mImageFetcher;
    @Mock private NotificationWrapperBuilder mNotificationBuilder;
    @Mock private NotificationManagerProxy mNotificationManagerProxy;
    @Mock private ChromeBrowserInitializer mChromeInitializer;
    @Mock private NotificationWrapper mNotificationWrapper;
    @Mock private PriceDropNotificationManager mPriceDropNotificationManager;

    @Captor ArgumentCaptor<Callback<Bitmap>> mBitmapCallbackCaptor;
    @Captor ArgumentCaptor<PendingIntentProvider> mPendingIntentProviderCaptor;

    PriceDropNotifier mPriceDropNotifier;
    Intent mIntent;

    @Before
    public void setUp() {
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mNotificationManagerProxy);
        mPriceDropNotifier =
                new TestPriceDropNotifier(mProfile, mImageFetcher, mNotificationBuilder);
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

    private void verifySetNotificationProperties() {
        verify(mNotificationBuilder, times(1)).setContentTitle(eq(TITLE));
        verify(mNotificationBuilder, times(1)).setContentText(eq(TEXT));
        verify(mNotificationBuilder, times(1)).setContentIntent(any(PendingIntentProvider.class));
        verify(mNotificationBuilder, times(1)).setSmallIcon(anyInt());
        verify(mNotificationBuilder, times(1)).setTimeoutAfter(anyLong());
        verify(mNotificationBuilder, times(1)).setAutoCancel(eq(true));
        verify(mNotificationManagerProxy)
                .cancel(PriceDropNotifier.NOTIFICATION_TAG, OFFER_ID.hashCode());
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

    @Test
    public void testTurnOffAlertIntentsDoNotAliasAcrossNotificationsAndProfiles() {
        List<Intent> turnOffIntents =
                showTurnOffAlertNotificationsAndGetIntents(
                        mProfile, OFFER_ID, mProfile, OTHER_OFFER_ID);

        // Within the same Profile, the intents share the same component and profile-scoped
        // identifier, so the per-notification requestCode keeps the second notification from
        // overwriting the first notification's extras.
        assertTrue(turnOffIntents.get(0).filterEquals(turnOffIntents.get(1)));
        assertEquals(
                OFFER_ID,
                IntentUtils.safeGetStringExtra(
                        turnOffIntents.get(0), PriceDropNotificationManagerImpl.EXTRA_OFFER_ID));
        assertEquals(
                OTHER_OFFER_ID,
                IntentUtils.safeGetStringExtra(
                        turnOffIntents.get(1), PriceDropNotificationManagerImpl.EXTRA_OFFER_ID));
        List<PendingIntentProvider> providers = mPendingIntentProviderCaptor.getAllValues();
        assertEquals(OFFER_ID.hashCode(), providers.get(0).getRequestCode());
        assertEquals(OTHER_OFFER_ID.hashCode(), providers.get(1).getRequestCode());
    }

    @Test
    public void testSameOfferInTwoProfilesKeepsPerProfileTurnOffIntents() {
        List<Intent> turnOffIntents =
                showTurnOffAlertNotificationsAndGetIntents(
                        mProfile, OFFER_ID, mOtherProfile, OFFER_ID);

        // Both notifications use the same notification id (requestCode), so the per-Profile tag
        // and profile-scoped intent identifier keep the second Profile's notification from
        // overwriting the first one's.
        assertFalse(turnOffIntents.get(0).filterEquals(turnOffIntents.get(1)));
        verifyProfileTokens(turnOffIntents);
        String tag = PriceDropNotifier.getNotificationTag(mProfile);
        String otherTag = PriceDropNotifier.getNotificationTag(mOtherProfile);
        assertNotEquals(tag, otherTag);
        assertEquals(
                tag,
                ProfileTokenUtils.getNotificationTagFromIntent(
                        PriceDropNotifier.NOTIFICATION_TAG, turnOffIntents.get(0)));
        assertEquals(
                otherTag,
                ProfileTokenUtils.getNotificationTagFromIntent(
                        PriceDropNotifier.NOTIFICATION_TAG, turnOffIntents.get(1)));
    }

    /**
     * Posts a notification with a "turn off alert" action for {@code firstOfferId} in {@code
     * firstProfile} and one for {@code secondOfferId} in {@code secondProfile}, using the real
     * {@link PriceDropNotificationManagerImpl}, and returns the intents saved in the two action
     * PendingIntents (in posting order).
     */
    private List<Intent> showTurnOffAlertNotificationsAndGetIntents(
            Profile firstProfile,
            String firstOfferId,
            Profile secondProfile,
            String secondOfferId) {
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        when(mProfileResolverNatives.tokenizeProfileToBytes(mProfile)).thenReturn(PROFILE_TOKEN);
        when(mProfileResolverNatives.tokenizeProfileToBytes(mOtherProfile))
                .thenReturn(OTHER_PROFILE_TOKEN);

        List<ActionData> actions = new ArrayList<>();
        actions.add(
                new ActionData(
                        PriceDropNotificationManagerImpl.ACTION_ID_TURN_OFF_ALERT, ACTION_TEXT_1));
        new TestPriceDropNotifier(firstProfile, mImageFetcher, mNotificationBuilder)
                .showNotification(
                        new NotificationData(
                                TITLE,
                                TEXT,
                                /* iconUrl= */ null,
                                DESTINATION_URL,
                                firstOfferId,
                                /* productClusterId= */ null,
                                actions));
        new TestPriceDropNotifier(secondProfile, mImageFetcher, mNotificationBuilder)
                .showNotification(
                        new NotificationData(
                                TITLE,
                                TEXT,
                                /* iconUrl= */ null,
                                DESTINATION_URL,
                                secondOfferId,
                                /* productClusterId= */ null,
                                actions));

        verify(mNotificationBuilder, times(2))
                .addAction(
                        anyInt(),
                        any(),
                        mPendingIntentProviderCaptor.capture(),
                        eq(ActionType.PRICE_DROP_TURN_OFF_ALERT));
        List<PendingIntentProvider> providers = mPendingIntentProviderCaptor.getAllValues();
        assertNotEquals(providers.get(0).getPendingIntent(), providers.get(1).getPendingIntent());
        List<Intent> intents = new ArrayList<>();
        for (PendingIntentProvider provider : providers) {
            intents.add(shadowOf(provider.getPendingIntent()).getSavedIntent());
        }
        return intents;
    }

    /** Verifies that each intent still carries the token of the Profile that posted it. */
    private void verifyProfileTokens(List<Intent> turnOffIntents) {
        ProfileTokenUtils.retrieveProfileFromIntent(turnOffIntents.get(0), (profile) -> {});
        verify(mProfileResolverNatives).resolveProfileFromBytes(aryEq(PROFILE_TOKEN), any());
        ProfileTokenUtils.retrieveProfileFromIntent(turnOffIntents.get(1), (profile) -> {});
        verify(mProfileResolverNatives).resolveProfileFromBytes(aryEq(OTHER_PROFILE_TOKEN), any());
    }
}
