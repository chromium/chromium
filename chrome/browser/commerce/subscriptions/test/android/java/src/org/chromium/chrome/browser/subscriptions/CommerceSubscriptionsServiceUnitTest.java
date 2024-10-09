// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerImpl;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.notifications.MockNotificationManagerProxy;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link CommerceSubscriptionsService}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CommerceSubscriptionsServiceUnitTest {

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private ShoppingService mShoppingService;
    @Mock TabModelSelector mTabModelSelector;
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private IdentityServicesProvider mIdentityServicesProvider;
    @Mock private UserPrefs.Natives mUserPrefsJni;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;

    @Captor
    private ArgumentCaptor<PauseResumeWithNativeObserver> mPauseResumeWithNativeObserverCaptor;

    private CommerceSubscriptionsService mService;
    private SharedPreferencesManager mSharedPreferencesManager;
    private MockNotificationManagerProxy mMockNotificationManager;
    private PriceDropNotificationManager mPriceDropNotificationManager;
    private FeatureList.TestValues mTestValues;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(CommerceFeatureUtilsJni.TEST_HOOKS, mCommerceFeatureUtilsJniMock);
        doReturn(true).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        mJniMocker.mock(ShoppingServiceFactoryJni.TEST_HOOKS, mShoppingServiceFactoryJniMock);
        doReturn(mShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());

        doNothing().when(mActivityLifecycleDispatcher).register(any());
        mSharedPreferencesManager = ChromeSharedPreferences.getInstance();
        mSharedPreferencesManager.writeLong(
                CommerceSubscriptionsService.CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP,
                System.currentTimeMillis()
                        - TimeUnit.SECONDS.toMillis(
                                CommerceSubscriptionsServiceConfig.getStaleTabLowerBoundSeconds()));
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);

        mTestValues = new FeatureList.TestValues();
        mTestValues.addFeatureFlagOverride(ChromeFeatureList.COMMERCE_PRICE_TRACKING, true);
        FeatureList.setTestValues(mTestValues);

        mMockNotificationManager = new MockNotificationManagerProxy();
        mMockNotificationManager.setNotificationsEnabled(false);
        PriceDropNotificationManagerImpl.setNotificationManagerForTesting(mMockNotificationManager);

        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJni);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);

        mPriceDropNotificationManager = PriceDropNotificationManagerFactory.create(mProfile);
        mService =
                new CommerceSubscriptionsService(mShoppingService, mPriceDropNotificationManager);
    }

    @After
    public void tearDown() {
        PriceDropNotificationManagerImpl.setNotificationManagerForTesting(null);
    }

    @Test
    @SmallTest
    public void testInitDeferredStartupForActivity() {
        mService.initDeferredStartupForActivity(mTabModelSelector, mActivityLifecycleDispatcher);
        verify(mActivityLifecycleDispatcher, times(1))
                .register(mPauseResumeWithNativeObserverCaptor.capture());

        mService.destroy();
        verify(mActivityLifecycleDispatcher, times(1))
                .unregister(eq(mPauseResumeWithNativeObserverCaptor.getValue()));
    }

    @Test
    @SmallTest
    public void testOnResume() {
        setupTestOnResume();
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        PriceDropNotificationManagerImpl.NOTIFICATION_ENABLED_HISTOGRAM),
                equalTo(1));
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        PriceDropNotificationManagerImpl
                                .NOTIFICATION_CHROME_MANAGED_COUNT_HISTOGRAM),
                equalTo(1));
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        PriceDropNotificationManagerImpl.NOTIFICATION_USER_MANAGED_COUNT_HISTOGRAM),
                equalTo(1));
    }

    @Test
    @SmallTest
    public void testOnResume_FeatureDisabled() {
        doReturn(false).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());

        setupTestOnResume();
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        PriceDropNotificationManagerImpl.NOTIFICATION_ENABLED_HISTOGRAM),
                equalTo(0));
    }

    @Test
    @SmallTest
    public void testOnResume_TooFrequent() {
        mSharedPreferencesManager.writeLong(
                CommerceSubscriptionsService.CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP,
                System.currentTimeMillis());

        setupTestOnResume();
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(
                        PriceDropNotificationManagerImpl.NOTIFICATION_ENABLED_HISTOGRAM),
                equalTo(0));
    }

    private void setupTestOnResume() {
        mService.initDeferredStartupForActivity(mTabModelSelector, mActivityLifecycleDispatcher);
        verify(mActivityLifecycleDispatcher, times(1))
                .register(mPauseResumeWithNativeObserverCaptor.capture());
        mPauseResumeWithNativeObserverCaptor.getValue().onResumeWithNative();
    }
}
