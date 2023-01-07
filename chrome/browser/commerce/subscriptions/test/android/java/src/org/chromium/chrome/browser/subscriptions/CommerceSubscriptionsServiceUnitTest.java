// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerImpl;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.CommerceSubscriptionType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscriptionsMetrics.AccountWaaStatus;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Unit tests for {@link CommerceSubscriptionsService}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CommerceSubscriptionsServiceUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private SubscriptionsManagerImpl mSubscriptionsManager;
    @Mock
    private IdentityManager mIdentityManager;
    @Mock
    private PrimaryAccountChangeEvent mChangeEvent;
    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    private ImplicitPriceDropSubscriptionsManager mImplicitSubscriptionsManager;
    @Mock
    private Profile mProfile;
    @Mock
    private PrefService mPrefService;
    @Mock
    private IdentityServicesProvider mIdentityServicesProvider;
    @Mock
    private UserPrefs.Natives mUserPrefsJni;
    @Captor
    private ArgumentCaptor<IdentityManager.Observer> mIdentityManagerObserverCaptor;
    @Captor
    private ArgumentCaptor<PauseResumeWithNativeObserver> mPauseResumeWithNativeObserverCaptor;
    @Captor
    private ArgumentCaptor<Callback<List<CommerceSubscription>>> mLocalSubscriptionsCallbackCaptor;

    private CommerceSubscriptionsService mService;
    private SharedPreferencesManager mSharedPreferencesManager;
    private MockNotificationManagerProxy mMockNotificationManager;
    private PriceDropNotificationManager mPriceDropNotificationManager;
    private FeatureList.TestValues mTestValues;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        UmaRecorderHolder.resetForTesting();

        doNothing().when(mActivityLifecycleDispatcher).register(any());
        doNothing().when(mSubscriptionsManager).getSubscriptions(anyString(), anyBoolean(), any());
        mSharedPreferencesManager = SharedPreferencesManager.getInstance();
        mSharedPreferencesManager.writeLong(
                CommerceSubscriptionsService.CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP,
                System.currentTimeMillis()
                        - TimeUnit.SECONDS.toMillis(
                                CommerceSubscriptionsServiceConfig.getStaleTabLowerBoundSeconds()));
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);

        mTestValues = new FeatureList.TestValues();
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING, true);
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                PriceTrackingFeatures.PRICE_NOTIFICATION_PARAM, "true");
        FeatureList.setTestValues(mTestValues);

        mMockNotificationManager = new MockNotificationManagerProxy();
        mMockNotificationManager.setNotificationsEnabled(false);
        PriceDropNotificationManagerImpl.setNotificationManagerForTesting(mMockNotificationManager);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJni);
        Profile.setLastUsedProfileForTesting(mProfile);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);

        mPriceDropNotificationManager = PriceDropNotificationManagerFactory.create();
        mService = new CommerceSubscriptionsService(
                mSubscriptionsManager, mIdentityManager, mPriceDropNotificationManager);
        verify(mIdentityManager, times(1)).addObserver(mIdentityManagerObserverCaptor.capture());
        mService.setImplicitSubscriptionsManagerForTesting(mImplicitSubscriptionsManager);
    }

    @After
    public void tearDown() {
        PriceDropNotificationManagerImpl.setNotificationManagerForTesting(null);
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mService.setImplicitSubscriptionsManagerForTesting(null);
        mService.destroy();
        verify(mIdentityManager, times(1))
                .removeObserver(eq(mIdentityManagerObserverCaptor.getValue()));
    }

    @Test
    @SmallTest
    public void testInitDeferredStartupForActivity() {
        mService.initDeferredStartupForActivity(mTabModelSelector, mActivityLifecycleDispatcher);
        verify(mActivityLifecycleDispatcher, times(1))
                .register(mPauseResumeWithNativeObserverCaptor.capture());

        mService.destroy();
        verify(mIdentityManager, times(1))
                .removeObserver(eq(mIdentityManagerObserverCaptor.getValue()));
        verify(mActivityLifecycleDispatcher, times(1))
                .unregister(eq(mPauseResumeWithNativeObserverCaptor.getValue()));
        verify(mImplicitSubscriptionsManager, times(1)).destroy();
    }

    @Test
    @SmallTest
    public void testOnPrimaryAccountChanged() {
        mIdentityManagerObserverCaptor.getValue().onPrimaryAccountChanged(mChangeEvent);
        verify(mSubscriptionsManager, times(1)).onIdentityChanged();
    }

    @Test
    @SmallTest
    public void testOnResume() {
        setupTestOnResume();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           PriceDropNotificationManagerImpl.NOTIFICATION_ENABLED_HISTOGRAM),
                equalTo(1));
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           PriceDropNotificationManagerImpl
                                   .NOTIFICATION_CHROME_MANAGED_COUNT_HISTOGRAM),
                equalTo(1));
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        PriceDropNotificationManagerImpl.NOTIFICATION_USER_MANAGED_COUNT_HISTOGRAM),
                equalTo(1));
        verify(mSubscriptionsManager, times(1))
                .getSubscriptions(eq(CommerceSubscriptionType.PRICE_TRACK), eq(false),
                        mLocalSubscriptionsCallbackCaptor.capture());
        verify(mImplicitSubscriptionsManager, times(1)).initializeSubscriptions();

        CommerceSubscription subscription1 = new CommerceSubscription(
                CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK, "offer_id_1",
                CommerceSubscription.SubscriptionManagementType.CHROME_MANAGED,
                CommerceSubscription.TrackingIdType.OFFER_ID);
        CommerceSubscription subscription2 =
                new CommerceSubscription(CommerceSubscription.CommerceSubscriptionType.PRICE_TRACK,
                        "offer_id_2", CommerceSubscription.SubscriptionManagementType.USER_MANAGED,
                        CommerceSubscription.TrackingIdType.PRODUCT_CLUSTER_ID);

        mLocalSubscriptionsCallbackCaptor.getValue().onResult(
                new ArrayList<>(Arrays.asList(subscription1, subscription1, subscription2)));
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        CommerceSubscriptionsMetrics.SUBSCRIPTION_CHROME_MANAGED_COUNT_HISTOGRAM),
                equalTo(1));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           CommerceSubscriptionsMetrics.SUBSCRIPTION_CHROME_MANAGED_COUNT_HISTOGRAM,
                           2),
                equalTo(1));
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           CommerceSubscriptionsMetrics.SUBSCRIPTION_USER_MANAGED_COUNT_HISTOGRAM),
                equalTo(1));
        assertThat(
                RecordHistogram.getHistogramValueCountForTesting(
                        CommerceSubscriptionsMetrics.SUBSCRIPTION_USER_MANAGED_COUNT_HISTOGRAM, 1),
                equalTo(1));
    }

    @Test
    @SmallTest
    public void testOnResume_FeatureDisabled() {
        mTestValues.addFieldTrialParamOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING,
                PriceTrackingFeatures.PRICE_NOTIFICATION_PARAM, "false");
        FeatureList.setTestValues(mTestValues);

        setupTestOnResume();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           PriceDropNotificationManagerImpl.NOTIFICATION_ENABLED_HISTOGRAM),
                equalTo(0));
        verify(mSubscriptionsManager, times(0)).getSubscriptions(anyString(), anyBoolean(), any());
        verify(mImplicitSubscriptionsManager, times(0)).initializeSubscriptions();
    }

    @Test
    @SmallTest
    public void testOnResume_TooFrequent() {
        mSharedPreferencesManager.writeLong(
                CommerceSubscriptionsService.CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP,
                System.currentTimeMillis());

        setupTestOnResume();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           PriceDropNotificationManagerImpl.NOTIFICATION_ENABLED_HISTOGRAM),
                equalTo(0));
        verify(mSubscriptionsManager, times(0)).getSubscriptions(anyString(), anyBoolean(), any());
        verify(mImplicitSubscriptionsManager, times(0)).initializeSubscriptions();
    }

    @Test
    @SmallTest
    public void testRecordAccountWaaStatus_SignOut() {
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(false);

        setupTestOnResume();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           CommerceSubscriptionsMetrics.ACCOUNT_WAA_STATUS_HISTOGRAM),
                equalTo(1));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           CommerceSubscriptionsMetrics.ACCOUNT_WAA_STATUS_HISTOGRAM,
                           AccountWaaStatus.SIGN_OUT),
                equalTo(1));
    }

    @Test
    @SmallTest
    public void testRecordAccountWaaStatus_SignInWaaDisabled() {
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mPrefService.getBoolean(Pref.WEB_AND_APP_ACTIVITY_ENABLED_FOR_SHOPPING))
                .thenReturn(false);

        setupTestOnResume();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           CommerceSubscriptionsMetrics.ACCOUNT_WAA_STATUS_HISTOGRAM),
                equalTo(1));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           CommerceSubscriptionsMetrics.ACCOUNT_WAA_STATUS_HISTOGRAM,
                           AccountWaaStatus.SIGN_IN_WAA_DISABLED),
                equalTo(1));
    }

    @Test
    @SmallTest
    public void testRecordAccountWaaStatus_SignInWaaEnabled() {
        when(mIdentityManager.hasPrimaryAccount(anyInt())).thenReturn(true);
        when(mPrefService.getBoolean(Pref.WEB_AND_APP_ACTIVITY_ENABLED_FOR_SHOPPING))
                .thenReturn(true);

        setupTestOnResume();
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           CommerceSubscriptionsMetrics.ACCOUNT_WAA_STATUS_HISTOGRAM),
                equalTo(1));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           CommerceSubscriptionsMetrics.ACCOUNT_WAA_STATUS_HISTOGRAM,
                           AccountWaaStatus.SIGN_IN_WAA_ENABLED),
                equalTo(1));
    }

    private void setupTestOnResume() {
        mService.initDeferredStartupForActivity(mTabModelSelector, mActivityLifecycleDispatcher);
        verify(mActivityLifecycleDispatcher, times(1))
                .register(mPauseResumeWithNativeObserverCaptor.capture());
        mPauseResumeWithNativeObserverCaptor.getValue().onResumeWithNative();
    }
}
