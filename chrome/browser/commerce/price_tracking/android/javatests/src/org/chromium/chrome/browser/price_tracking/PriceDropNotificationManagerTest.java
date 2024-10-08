// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.NotificationManager;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.provider.Browser;
import android.provider.Settings;

import androidx.test.filters.MediumTest;

import org.json.JSONArray;
import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
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
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.IdentifierType;
import org.chromium.components.commerce.core.ManagementType;
import org.chromium.components.commerce.core.ShoppingService;

/** Tests for {@link PriceDropNotificationManager}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=" + ChromeFeatureList.COMMERCE_PRICE_TRACKING + "<Study",
    "force-fieldtrials=Study/Group",
    "force-fieldtrial-params=Study.Group:user_managed_notification_max_number/2"
})
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

    private MockNotificationManagerProxy mMockNotificationManager;
    private PriceDropNotificationManager mPriceDropNotificationManager;

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Mock private ShoppingService mMockShoppingService;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;
    @Mock private BookmarkModel mMockBookmarkModel;
    @Mock private Profile mMockProfile;
    @Captor private ArgumentCaptor<CommerceSubscription> mSubscriptionCaptor;

    @Before
    public void setUp() {
        mJniMocker.mock(CommerceFeatureUtilsJni.TEST_HOOKS, mCommerceFeatureUtilsJniMock);
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        mJniMocker.mock(ShoppingServiceFactoryJni.TEST_HOOKS, mShoppingServiceFactoryJniMock);
        doReturn(mMockShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());

        mMockNotificationManager = new MockNotificationManagerProxy();
        PriceDropNotificationManagerImpl.setNotificationManagerForTesting(mMockNotificationManager);
        mPriceDropNotificationManager = PriceDropNotificationManagerFactory.create(mMockProfile);
        when(mMockBookmarkModel.isBookmarkModelLoaded()).thenReturn(true);
        BookmarkModel.setInstanceForTesting(mMockBookmarkModel);
        ProfileManager.setLastUsedProfileForTesting(mMockProfile);
    }

    @After
    public void tearDown() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mPriceDropNotificationManager.deleteChannelForTesting();
        }
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
        mMockNotificationManager.setNotificationsEnabled(true);
        doReturn(false).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());
        assertFalse(mPriceDropNotificationManager.canPostNotification());
        assertFalse(mPriceDropNotificationManager.canPostNotificationWithMetricsRecorded());
    }

    @Test
    @MediumTest
    public void testCanPostNotification_NotificationDisabled() {
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        mMockNotificationManager.setNotificationsEnabled(false);
        assertFalse(mPriceDropNotificationManager.areAppNotificationsEnabled());
        assertFalse(mPriceDropNotificationManager.canPostNotification());
        assertFalse(mPriceDropNotificationManager.canPostNotificationWithMetricsRecorded());
    }

    @Test
    @MediumTest
    public void testCanPostNotificaton() {
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        mMockNotificationManager.setNotificationsEnabled(true);
        assertTrue(mPriceDropNotificationManager.areAppNotificationsEnabled());

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            assertTrue(mPriceDropNotificationManager.canPostNotification());
            assertTrue(mPriceDropNotificationManager.canPostNotificationWithMetricsRecorded());
        } else {
            assertNull(mPriceDropNotificationManager.getNotificationChannel());
            assertFalse(mPriceDropNotificationManager.canPostNotification());
            assertFalse(mPriceDropNotificationManager.canPostNotificationWithMetricsRecorded());

            mPriceDropNotificationManager.createNotificationChannel();
            assertNotNull(mPriceDropNotificationManager.getNotificationChannel());
            assertEquals(
                    NotificationManager.IMPORTANCE_DEFAULT,
                    mPriceDropNotificationManager.getNotificationChannel().getImportance());

            assertTrue(mPriceDropNotificationManager.canPostNotification());
            assertTrue(mPriceDropNotificationManager.canPostNotificationWithMetricsRecorded());
        }
    }

    @Test
    @MediumTest
    public void testGetNotificationSettingsIntent_NotificationDisabled() {
        mMockNotificationManager.setNotificationsEnabled(false);
        Intent intent = mPriceDropNotificationManager.getNotificationSettingsIntent();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            assertEquals(ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());
            assertEquals(
                    ContextUtils.getApplicationContext().getPackageName(),
                    intent.getStringExtra(EXTRA_APP_PACKAGE));
            assertEquals(
                    ContextUtils.getApplicationContext().getApplicationInfo().uid,
                    intent.getIntExtra(EXTRA_APP_UID, 0));
        } else {
            assertEquals(Settings.ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());
            assertEquals(
                    ContextUtils.getApplicationContext().getPackageName(),
                    intent.getStringExtra(Settings.EXTRA_APP_PACKAGE));
        }
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags());
    }

    @Test
    @MediumTest
    public void testGetNotificationSettingsIntent_NotificationEnabled() {
        mMockNotificationManager.setNotificationsEnabled(true);
        Intent intent = mPriceDropNotificationManager.getNotificationSettingsIntent();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            assertEquals(ACTION_APP_NOTIFICATION_SETTINGS, intent.getAction());
            assertEquals(
                    ContextUtils.getApplicationContext().getPackageName(),
                    intent.getStringExtra(EXTRA_APP_PACKAGE));
            assertEquals(
                    ContextUtils.getApplicationContext().getApplicationInfo().uid,
                    intent.getIntExtra(EXTRA_APP_UID, 0));
        } else {
            assertEquals(Settings.ACTION_CHANNEL_NOTIFICATION_SETTINGS, intent.getAction());
            assertEquals(
                    ContextUtils.getApplicationContext().getPackageName(),
                    intent.getStringExtra(Settings.EXTRA_APP_PACKAGE));
            assertEquals(
                    ChromeChannelDefinitions.ChannelId.PRICE_DROP_DEFAULT,
                    intent.getStringExtra(Settings.EXTRA_CHANNEL_ID));
        }
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
}
