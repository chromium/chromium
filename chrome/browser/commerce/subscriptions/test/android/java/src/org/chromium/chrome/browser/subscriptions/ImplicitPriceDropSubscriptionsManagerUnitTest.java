// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.CommerceSubscriptionType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.SubscriptionManagementType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.TrackingIdType;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link ImplicitPriceDropSubscriptionsManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImplicitPriceDropSubscriptionsManagerUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static final int TAB1_ID = 456;
    private static final int TAB2_ID = 789;
    private static final String URL1 = "www.foo.com";
    private static final String URL2 = "www.bar.com";
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final String OFFER1_ID = "offer_foo";
    private static final String OFFER2_ID = "offer_bar";

    @Mock
    TabModel mTabModel;
    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    SubscriptionsManagerImpl mSubscriptionsManager;
    @Mock
    DeferredStartupHandler mDeferredStartupHandler;
    @Mock
    CriticalPersistedTabData mCriticalPersistedTabData1;
    @Mock
    CriticalPersistedTabData mCriticalPersistedTabData2;
    @Mock
    ShoppingPersistedTabData mShoppingPersistedTabData1;
    @Mock
    ShoppingPersistedTabData mShoppingPersistedTabData2;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor
    ArgumentCaptor<PauseResumeWithNativeObserver> mPauseResumeWithNativeObserverCaptor;
    @Captor
    ArgumentCaptor<CommerceSubscription> mSubscriptionCaptor;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private CommerceSubscription mSubscription1;
    private CommerceSubscription mSubscription2;
    private ImplicitPriceDropSubscriptionsManager mImplicitSubscriptionsManager;
    private SharedPreferencesManager mSharedPreferencesManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTab1 = prepareTab(
                TAB1_ID, URL1, POSITION1, mCriticalPersistedTabData1, mShoppingPersistedTabData1);
        mTab2 = prepareTab(
                TAB2_ID, URL2, POSITION2, mCriticalPersistedTabData2, mShoppingPersistedTabData2);
        // Mock that tab1 and tab2 both have offer ID and are stale tabs.
        doReturn(OFFER1_ID).when(mShoppingPersistedTabData1).getMainOfferId();
        doReturn(OFFER2_ID).when(mShoppingPersistedTabData2).getMainOfferId();
        long fakeTimestamp = System.currentTimeMillis()
                - TimeUnit.SECONDS.toMillis(
                        ShoppingPersistedTabData.STALE_TAB_THRESHOLD_SECONDS.getValue())
                + TimeUnit.DAYS.toMillis(7);
        doReturn(fakeTimestamp).when(mCriticalPersistedTabData1).getTimestampMillis();
        doReturn(fakeTimestamp).when(mCriticalPersistedTabData2).getTimestampMillis();
        mSubscription1 = new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK,
                ShoppingPersistedTabData.from(mTab1).getMainOfferId(),
                SubscriptionManagementType.CHROME_MANAGED, TrackingIdType.OFFER_ID);
        mSubscription2 = new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK,
                ShoppingPersistedTabData.from(mTab2).getMainOfferId(),
                SubscriptionManagementType.CHROME_MANAGED, TrackingIdType.OFFER_ID);
        doReturn(2).when(mTabModel).getCount();
        doReturn(mTabModel).when(mTabModelSelector).getModel(false);
        doNothing().when(mTabModel).addObserver(mTabModelObserverCaptor.capture());
        doNothing()
                .when(mActivityLifecycleDispatcher)
                .register(mPauseResumeWithNativeObserverCaptor.capture());
        DeferredStartupHandler.setInstanceForTests(mDeferredStartupHandler);
        mSharedPreferencesManager = SharedPreferencesManager.getInstance();
        mSharedPreferencesManager.writeLong(
                ImplicitPriceDropSubscriptionsManager.CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP,
                System.currentTimeMillis()
                        - ImplicitPriceDropSubscriptionsManager
                                  .CHROME_MANAGED_SUBSCRIPTIONS_TIME_THRESHOLD_MS);

        mImplicitSubscriptionsManager = new ImplicitPriceDropSubscriptionsManager(
                mTabModelSelector, mActivityLifecycleDispatcher, mSubscriptionsManager);
    }

    @After
    public void tearDown() {
        DeferredStartupHandler.setInstanceForTests(null);
    }

    @Test
    public void testInitialSetup() {
        verify(mTabModel).addObserver(any(TabModelObserver.class));
        verify(mDeferredStartupHandler).addDeferredTask(any(Runnable.class));
        verify(mActivityLifecycleDispatcher).register(any(PauseResumeWithNativeObserver.class));
    }

    @Test
    public void testInitialSubscription_AllUnique() {
        doReturn(2).when(mTabModel).getCount();

        mImplicitSubscriptionsManager.initializeSubscriptions();

        verify(mSubscriptionsManager)
                .subscribe(eq(new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2))));
    }

    @Test
    public void testInitialSubscription_WithDuplicateURL() {
        mTab1 = prepareTab(
                TAB1_ID, URL1, POSITION1, mCriticalPersistedTabData1, mShoppingPersistedTabData1);
        mTab2 = prepareTab(
                TAB2_ID, URL1, POSITION2, mCriticalPersistedTabData2, mShoppingPersistedTabData2);

        mImplicitSubscriptionsManager.initializeSubscriptions();

        verify(mSubscriptionsManager).subscribe(eq(new ArrayList<>(Arrays.asList(mSubscription2))));
    }

    @Test
    public void testInitialSubscription_NoOfferID() {
        doReturn("").when(mShoppingPersistedTabData1).getMainOfferId();

        mImplicitSubscriptionsManager.initializeSubscriptions();

        verify(mSubscriptionsManager).subscribe(eq(new ArrayList<>(Arrays.asList(mSubscription2))));
    }

    @Test
    public void testInitialSubscription_TabTooOld() {
        doReturn(System.currentTimeMillis()
                - TimeUnit.SECONDS.toMillis(
                        ShoppingPersistedTabData.STALE_TAB_THRESHOLD_SECONDS.getValue())
                - TimeUnit.DAYS.toMillis(7))
                .when(mCriticalPersistedTabData1)
                .getTimestampMillis();

        mImplicitSubscriptionsManager.initializeSubscriptions();

        verify(mSubscriptionsManager).subscribe(eq(new ArrayList<>(Arrays.asList(mSubscription2))));
    }

    @Test
    public void testInitialSubscription_TabTooNew() {
        doReturn(System.currentTimeMillis()).when(mCriticalPersistedTabData1).getTimestampMillis();

        mImplicitSubscriptionsManager.initializeSubscriptions();

        verify(mSubscriptionsManager).subscribe(eq(new ArrayList<>(Arrays.asList(mSubscription2))));
    }

    @Test
    public void testInitialSubscription_TooFrequentSubscription() {
        mSharedPreferencesManager.writeLong(
                ImplicitPriceDropSubscriptionsManager.CHROME_MANAGED_SUBSCRIPTIONS_TIMESTAMP,
                System.currentTimeMillis());

        mImplicitSubscriptionsManager.initializeSubscriptions();

        verify(mSubscriptionsManager, times(0)).subscribe(any(List.class));
    }

    @Test
    public void testInitialSubscription_OnResume() {
        mPauseResumeWithNativeObserverCaptor.getValue().onResumeWithNative();

        verify(mSubscriptionsManager)
                .subscribe(eq(new ArrayList<>(Arrays.asList(mSubscription1, mSubscription2))));
    }

    @Test
    public void testTabClosure() {
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);

        verify(mSubscriptionsManager, times(1)).unsubscribe(mSubscriptionCaptor.capture());
        assertThat(mSubscriptionCaptor.getAllValues().get(0).getTrackingId(),
                equalTo(String.valueOf(OFFER1_ID)));
    }

    @Test
    public void testTabRemove() {
        mTabModelObserverCaptor.getValue().tabRemoved(mTab1);

        verify(mSubscriptionsManager, times(1)).unsubscribe(mSubscriptionCaptor.capture());
        assertThat(mSubscriptionCaptor.getAllValues().get(0).getTrackingId(),
                equalTo(String.valueOf(OFFER1_ID)));
    }

    @Test
    public void testUnsubscribe_NotUnique() {
        mTab1 = prepareTab(
                TAB1_ID, URL1, POSITION1, mCriticalPersistedTabData1, mShoppingPersistedTabData1);
        mTab2 = prepareTab(
                TAB2_ID, URL1, POSITION2, mCriticalPersistedTabData2, mShoppingPersistedTabData2);

        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);

        verify(mSubscriptionsManager, never()).unsubscribe(mSubscriptionCaptor.capture());
    }

    @Test
    public void testDestroy() {
        mImplicitSubscriptionsManager.destroy();

        verify(mTabModel).removeObserver(any(TabModelObserver.class));
        verify(mActivityLifecycleDispatcher).unregister(any(PauseResumeWithNativeObserver.class));
    }

    private TabImpl prepareTab(int id, String urlString, int position,
            CriticalPersistedTabData criticalPersistedTabData,
            ShoppingPersistedTabData shoppingPersistedTabData) {
        TabImpl tab = mock(TabImpl.class);
        doReturn(id).when(tab).getId();
        doReturn(urlString).when(tab).getUrlString();
        GURL gurl = mock(GURL.class);
        doReturn(urlString).when(gurl).getSpec();
        doReturn(gurl).when(tab).getOriginalUrl();
        doReturn(tab).when(mTabModel).getTabAt(position);
        UserDataHost userDataHost = new UserDataHost();
        userDataHost.setUserData(CriticalPersistedTabData.class, criticalPersistedTabData);
        userDataHost.setUserData(ShoppingPersistedTabData.class, shoppingPersistedTabData);
        doReturn(userDataHost).when(tab).getUserDataHost();
        return tab;
    }
}
