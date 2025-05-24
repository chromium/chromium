// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;
import android.provider.Settings;

import androidx.test.filters.MediumTest;

import org.json.JSONArray;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker.SystemNotificationType;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerImpl.DismissNotificationChromeActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.IdentifierType;
import org.chromium.components.commerce.core.ManagementType;
import org.chromium.components.commerce.core.ShoppingService;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link PriceDropNotificationManager}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures(
        ChromeFeatureList.PRICE_ANNOTATIONS + ":user_managed_notification_max_number/2")
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class PriceDropNotificationManagerTest {
    private static final String ACTION_APP_NOTIFICATION_SETTINGS =
            "android.settings.APP_NOTIFICATION_SETTINGS";
    private static final String EXTRA_APP_PACKAGE = "app_package";
    private static final String EXTRA_APP_UID = "app_uid";
    private static final String ACTION_ID_VISIT_SITE = "visit_site";
    private static final String ACTION_ID_TURN_OFF_ALERT = "turn_off_alert";
    private static final String TEST_URL = "www.test.com";
    private static final String OFFER_ID = "offer_id";
    private static final String PRODUCT_CLUSTER_ID = "cluster_id";
    private static final int NOTIFICATION_ID = 123;

    private NotificationManagerProxy mMockNotificationManager;

    private PriceDropNotificationManager mPriceDropNotificationManager;
    private BookmarkModel mMockBookmarkModel;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Mock private ShoppingService mMockShoppingService;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;
    @Mock private Profile mMockProfile;
    @Captor private ArgumentCaptor<CommerceSubscription> mSubscriptionCaptor;

    @Before
    public void setUp() {
        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingServiceFactoryJniMock);
        doReturn(mMockShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());

        mMockNotificationManager = spy(NotificationManagerProxyImpl.getInstance());
        BaseNotificationManagerProxyFactory.setInstanceForTesting(mMockNotificationManager);
        mPriceDropNotificationManager = PriceDropNotificationManagerFactory.create(mMockProfile);
        BookmarkModel bookmarkModel =
                ThreadUtils.runOnUiThreadBlocking(() -> Mockito.mock(BookmarkModel.class));
        when(bookmarkModel.isBookmarkModelLoaded()).thenReturn(true);
        BookmarkModel.setInstanceForTesting(bookmarkModel);
        ProfileManager.setLastUsedProfileForTesting(mMockProfile);
    }

    @After
    public void tearDown() {
        mPriceDropNotificationManager.deleteChannelForTesting();
        NotificationProxyUtils.setNotificationEnabledForTest(null);
    }

    private void verifyClickIntent(Intent intent) {
        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals(Uri.parse(TEST_URL), intent.getData());
        assertEquals(
                DismissNotificationChromeActivity.class.getName(),
                intent.getComponent().getClassName());
        assertEquals(
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT,
                intent.getFlags());
        assertEquals(
                ContextUtils.getApplicationContext().getPackageName(),
                intent.getStringExtra(Browser.EXTRA_APPLICATION_ID));
        assertEquals(
                true,
                intent.getBooleanExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, false));
        assertEquals(
                NOTIFICATION_ID,
                intent.getIntExtra(PriceDropNotificationManagerImpl.EXTRA_NOTIFICATION_ID, 0));
    }

    @Test
    @MediumTest
    public void testCanPostNotification_FeatureDisabled() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        doReturn(false).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());
        verifyCanPostNotification(false);
    }

    @Test
    @MediumTest
    public void testCanPostNotification_NotificationDisabled() {
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        assertFalse(mPriceDropNotificationManager.areAppNotificationsEnabled());
        verifyCanPostNotification(false);
    }

    @Test
    @MediumTest
    public void testCanPostNotificaton() {
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        assertTrue(mPriceDropNotificationManager.areAppNotificationsEnabled());

        AtomicBoolean callbackComplete = new AtomicBoolean(false);
        AtomicReference<NotificationChannel> channelRef = new AtomicReference<>();
        mPriceDropNotificationManager.getNotificationChannel(
                (result) -> {
                    channelRef.set(result);
                    callbackComplete.set(true);
                });
        CriteriaHelper.pollInstrumentationThread(callbackComplete::get);
        assertNull(channelRef.get());
        verifyCanPostNotification(false);

        mPriceDropNotificationManager.createNotificationChannel();
        callbackComplete.set(false);
        mPriceDropNotificationManager.getNotificationChannel(
                (result) -> {
                    channelRef.set(result);
                    callbackComplete.set(true);
                });
        CriteriaHelper.pollInstrumentationThread(callbackComplete::get);
        assertNotNull(channelRef.get());
        assertEquals(NotificationManager.IMPORTANCE_DEFAULT, channelRef.get().getImportance());
        verifyCanPostNotification(true);
    }

    @Test
    @MediumTest
    public void testGetNotificationSettingsIntent_NotificationDisabled() {
        NotificationProxyUtils.setNotificationEnabledForTest(false);
        Intent intent = mPriceDropNotificationManager.getNotificationSettingsIntent();
        assertEquals(Settings.ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());
        assertEquals(
                ContextUtils.getApplicationContext().getPackageName(),
                intent.getStringExtra(Settings.EXTRA_APP_PACKAGE));
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags());
    }

    @Test
    @MediumTest
    public void testGetNotificationSettingsIntent_NotificationEnabled() {
        NotificationProxyUtils.setNotificationEnabledForTest(true);
        Intent intent = mPriceDropNotificationManager.getNotificationSettingsIntent();
        assertEquals(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS, intent.getAction());
        assertEquals(
                ContextUtils.getApplicationContext().getPackageName(),
                intent.getStringExtra(Settings.EXTRA_APP_PACKAGE));
        assertEquals(
                ChromeChannelDefinitions.ChannelId.PRICE_DROP_DEFAULT,
                intent.getStringExtra(Settings.EXTRA_CHANNEL_ID));
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags());
    }

    @Test
    @MediumTest
    public void testGetNotificationClickIntent() {
        verifyClickIntent(
                mPriceDropNotificationManager.getNotificationClickIntent(
                        TEST_URL, NOTIFICATION_ID));
    }

    @Test
    @MediumTest
    public void testGetNotificationActionClickIntent() {
        verifyClickIntent(
                mPriceDropNotificationManager.getNotificationActionClickIntent(
                        ACTION_ID_VISIT_SITE,
                        TEST_URL,
                        OFFER_ID,
                        PRODUCT_CLUSTER_ID,
                        NOTIFICATION_ID));
        Intent turnOffAlertIntent =
                mPriceDropNotificationManager.getNotificationActionClickIntent(
                        ACTION_ID_TURN_OFF_ALERT,
                        TEST_URL,
                        OFFER_ID,
                        PRODUCT_CLUSTER_ID,
                        NOTIFICATION_ID);
        assertNotNull(turnOffAlertIntent);
        assertEquals(
                PriceDropNotificationManagerImpl.TrampolineActivity.class.getName(),
                turnOffAlertIntent.getComponent().getClassName());
        assertEquals(
                PRODUCT_CLUSTER_ID,
                IntentUtils.safeGetStringExtra(
                        turnOffAlertIntent,
                        PriceDropNotificationManagerImpl.EXTRA_PRODUCT_CLUSTER_ID));
        assertEquals(
                OFFER_ID,
                IntentUtils.safeGetStringExtra(
                        turnOffAlertIntent, PriceDropNotificationManagerImpl.EXTRA_OFFER_ID));
        assertEquals(
                TEST_URL,
                IntentUtils.safeGetStringExtra(
                        turnOffAlertIntent,
                        PriceDropNotificationManagerImpl.EXTRA_DESTINATION_URL));
        assertEquals(
                ACTION_ID_TURN_OFF_ALERT,
                IntentUtils.safeGetStringExtra(
                        turnOffAlertIntent, PriceDropNotificationManagerImpl.EXTRA_ACTION_ID));
        assertEquals(
                NOTIFICATION_ID,
                IntentUtils.safeGetIntExtra(
                        turnOffAlertIntent,
                        PriceDropNotificationManagerImpl.EXTRA_NOTIFICATION_ID,
                        0));
    }

    @Test
    @MediumTest
    public void testOnNotificationActionClicked_TurnOffAlert() {
        String offerId = "offer_id";

        mPriceDropNotificationManager.onNotificationActionClicked(
                ACTION_ID_TURN_OFF_ALERT, TEST_URL, null, null, false);
        verify(mMockShoppingService, times(0)).unsubscribe(any(), any(Callback.class));

        mPriceDropNotificationManager.onNotificationActionClicked(
                ACTION_ID_TURN_OFF_ALERT, TEST_URL, offerId, null, false);
        verify(mMockShoppingService, times(1))
                .unsubscribe(mSubscriptionCaptor.capture(), any(Callback.class));
        assertEquals(IdentifierType.OFFER_ID, mSubscriptionCaptor.getValue().idType);
        assertEquals(offerId, mSubscriptionCaptor.getValue().id);
        assertEquals(ManagementType.CHROME_MANAGED, mSubscriptionCaptor.getValue().managementType);
        assertEquals(null, mSubscriptionCaptor.getValue().userSeenOffer);
    }

    @Test
    @MediumTest
    public void testNotificationTypeEnabled() {
        assertTrue(mPriceDropNotificationManager.isEnabled());
    }

    @Test
    @MediumTest
    public void testUpdateNotificationTimestamps() {
        SharedPreferencesManager preferencesManager = ChromeSharedPreferences.getInstance();
        int mockType = SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED;
        long mockTimestamp =
                System.currentTimeMillis()
                        - 2L
                                * PriceTrackingNotificationConfig
                                        .getNotificationTimestampsStoreWindowMs();
        JSONArray jsonArray = new JSONArray();
        jsonArray.put(mockTimestamp);
        preferencesManager.writeString(
                PriceDropNotificationManagerImpl.USER_MANAGED_TIMESTAMPS, jsonArray.toString());
        assertEquals(
                0, mPriceDropNotificationManager.updateNotificationTimestamps(mockType, false));

        mockTimestamp = System.currentTimeMillis();
        jsonArray = new JSONArray();
        jsonArray.put(mockTimestamp);
        preferencesManager.writeString(
                PriceDropNotificationManagerImpl.USER_MANAGED_TIMESTAMPS, jsonArray.toString());
        assertEquals(
                1, mPriceDropNotificationManager.updateNotificationTimestamps(mockType, false));

        assertEquals(2, mPriceDropNotificationManager.updateNotificationTimestamps(mockType, true));
        assertEquals(3, mPriceDropNotificationManager.updateNotificationTimestamps(mockType, true));

        preferencesManager.writeString(
                PriceDropNotificationManagerImpl.USER_MANAGED_TIMESTAMPS, "");
        assertEquals(
                0, mPriceDropNotificationManager.updateNotificationTimestamps(mockType, false));
        assertEquals(1, mPriceDropNotificationManager.updateNotificationTimestamps(mockType, true));
    }

    @Test
    @MediumTest
    public void testHasReachedMaxAllowedNotificationNumber() {
        int mockType = SystemNotificationType.PRICE_DROP_ALERTS_USER_MANAGED;
        assertEquals(
                false,
                mPriceDropNotificationManager.hasReachedMaxAllowedNotificationNumber(mockType));

        mPriceDropNotificationManager.updateNotificationTimestamps(mockType, true);
        assertEquals(
                false,
                mPriceDropNotificationManager.hasReachedMaxAllowedNotificationNumber(mockType));

        mPriceDropNotificationManager.updateNotificationTimestamps(mockType, true);
        assertEquals(
                true,
                mPriceDropNotificationManager.hasReachedMaxAllowedNotificationNumber(mockType));
    }

    private void verifyCanPostNotification(boolean expectation) {
        AtomicBoolean canPost = new AtomicBoolean(!expectation);
        AtomicBoolean canPostWithMetrics = new AtomicBoolean(!expectation);
        mPriceDropNotificationManager.canPostNotification(
                (result) -> {
                    canPost.set(result);
                });
        mPriceDropNotificationManager.canPostNotificationWithMetricsRecorded(
                (result) -> {
                    canPostWithMetrics.set(result);
                });
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(
                            (canPost.get() == expectation)
                                    && (canPostWithMetrics.get() == expectation),
                            is(true));
                });
    }
}
