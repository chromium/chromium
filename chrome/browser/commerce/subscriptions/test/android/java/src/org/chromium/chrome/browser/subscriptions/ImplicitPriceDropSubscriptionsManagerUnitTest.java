// Copyright 2021 The Chromium Authors
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
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
import org.chromium.base.UserDataHost;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.CommerceSubscriptionType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.SubscriptionManagementType;
import org.chromium.chrome.browser.subscriptions.CommerceSubscription.TrackingIdType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.url.GURL;

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
    private static final String URL1 = "http://www.foo.com";
    private static final String URL2 = "http://www.bar.com";
    private static final int POSITION1 = 0;
    private static final int POSITION2 = 1;
    private static final String OFFER1_ID = "offer_foo";
    private static final String OFFER2_ID = "offer_bar";
    private static final String TAB_ELIGIBLE_HISTOGRAM = "Commerce.Subscriptions.TabEligible";

    static class TestImplicitPriceDropSubscriptionsManager
            extends ImplicitPriceDropSubscriptionsManager {
        private TabImpl mTab1;
        private TabImpl mTab2;
        private String mMockTab1OfferId;
        private String mMockTab2OfferId;

        TestImplicitPriceDropSubscriptionsManager(
                TabModelSelector tabModelSelector, SubscriptionsManagerImpl subscriptionsManager) {
            super(tabModelSelector, subscriptionsManager);
        }

        @Override
        protected void fetchOfferId(Tab tab, Callback<String> callback) {
            if (mTab1.equals(tab)) {
                callback.onResult(mMockTab1OfferId);
            } else if (mTab2.equals(tab)) {
                callback.onResult(mMockTab2OfferId);
            } else {
                assert false : "Unsupported tab";
            }
        }

        void setupForFetchOfferId(
                TabImpl tab1, TabImpl tab2, String mockTab1OfferId, String mockTab2OfferId) {
            mTab1 = tab1;
            mTab2 = tab2;
            mMockTab1OfferId = mockTab1OfferId;
            mMockTab2OfferId = mockTab2OfferId;
        }
    }

    @Mock
    TabModel mTabModel;
    @Mock
    TabModelSelector mTabModelSelector;
    @Mock
    SubscriptionsManagerImpl mSubscriptionsManager;
    @Mock
    CriticalPersistedTabData mCriticalPersistedTabData1;
    @Mock
    CriticalPersistedTabData mCriticalPersistedTabData2;
    @Captor
    ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private CommerceSubscription mSubscription1;
    private CommerceSubscription mSubscription2;
    private TestImplicitPriceDropSubscriptionsManager mImplicitSubscriptionsManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTab1 = prepareTab(TAB1_ID, URL1, POSITION1, mCriticalPersistedTabData1);
        mTab2 = prepareTab(TAB2_ID, URL2, POSITION2, mCriticalPersistedTabData2);
        // Mock that tab1 and tab2 are both stale tabs.
        long fakeTimestamp = System.currentTimeMillis()
                - TimeUnit.SECONDS.toMillis(ShoppingPersistedTabData.getStaleTabThresholdSeconds())
                + TimeUnit.DAYS.toMillis(7);
        doReturn(fakeTimestamp).when(mCriticalPersistedTabData1).getTimestampMillis();
        doReturn(fakeTimestamp).when(mCriticalPersistedTabData2).getTimestampMillis();
        mSubscription1 = new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK, OFFER1_ID,
                SubscriptionManagementType.CHROME_MANAGED, TrackingIdType.OFFER_ID);
        mSubscription2 = new CommerceSubscription(CommerceSubscriptionType.PRICE_TRACK, OFFER2_ID,
                SubscriptionManagementType.CHROME_MANAGED, TrackingIdType.OFFER_ID);
        doReturn(2).when(mTabModel).getCount();
        doReturn(mTabModel).when(mTabModelSelector).getModel(false);
        doNothing().when(mTabModel).addObserver(mTabModelObserverCaptor.capture());

        mImplicitSubscriptionsManager = new TestImplicitPriceDropSubscriptionsManager(
                mTabModelSelector, mSubscriptionsManager);
        mImplicitSubscriptionsManager.setupForFetchOfferId(mTab1, mTab2, OFFER1_ID, OFFER2_ID);
    }

    @Test
    public void testInitialSetup() {
        verify(mTabModel).addObserver(any(TabModelObserver.class));
    }

    @Test
    public void testInitialSubscription_AllUnique() {
        initializeSubscriptionsAndVerify(true, true);
        verifyEligibleSubscriptionMetrics(2, 2);
    }

    @Test
    public void testInitialSubscription_WithDuplicateURL() {
        mTab1 = prepareTab(TAB1_ID, URL1, POSITION1, mCriticalPersistedTabData1);
        mTab2 = prepareTab(TAB2_ID, URL1, POSITION2, mCriticalPersistedTabData2);
        mImplicitSubscriptionsManager.setupForFetchOfferId(mTab1, mTab2, OFFER1_ID, OFFER2_ID);

        initializeSubscriptionsAndVerify(true, false);
        verifyEligibleSubscriptionMetrics(2, 2);
    }

    @Test
    public void testInitialSubscription_NoOfferID() {
        mImplicitSubscriptionsManager.setupForFetchOfferId(mTab1, mTab2, OFFER1_ID, null);
        initializeSubscriptionsAndVerify(true, false);
        verifyEligibleSubscriptionMetrics(1, 2);
    }

    @Test
    public void testInitialSubscription_TabTooOld() {
        doReturn(System.currentTimeMillis()
                - TimeUnit.SECONDS.toMillis(ShoppingPersistedTabData.getStaleTabThresholdSeconds())
                - TimeUnit.DAYS.toMillis(7))
                .when(mCriticalPersistedTabData1)
                .getTimestampMillis();

        initializeSubscriptionsAndVerify(false, true);
        verifyEligibleSubscriptionMetrics(1, 2);
    }

    @Test
    public void testInitialSubscription_TabTooNew() {
        doReturn(System.currentTimeMillis()).when(mCriticalPersistedTabData1).getTimestampMillis();

        initializeSubscriptionsAndVerify(false, true);
        verifyEligibleSubscriptionMetrics(1, 2);
    }

    @Test
    public void testTabClosure() {
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        verify(mSubscriptionsManager, times(1))
                .unsubscribe(eq(mSubscription1), any(Callback.class));
    }

    @Test
    public void testTabRemove() {
        mTabModelObserverCaptor.getValue().tabRemoved(mTab1);
        verify(mSubscriptionsManager, times(1))
                .unsubscribe(eq(mSubscription1), any(Callback.class));
    }

    @Test
    public void testTabSelected() {
        mTabModelObserverCaptor.getValue().didSelectTab(mTab1, TabSelectionType.FROM_USER, TAB2_ID);
        verify(mSubscriptionsManager, times(1))
                .unsubscribe(eq(mSubscription1), any(Callback.class));
    }

    @Test
    public void testUnsubscribe_NotUnique() {
        mTab1 = prepareTab(TAB1_ID, URL1, POSITION1, mCriticalPersistedTabData1);
        mTab2 = prepareTab(TAB2_ID, URL1, POSITION2, mCriticalPersistedTabData2);
        mImplicitSubscriptionsManager.setupForFetchOfferId(mTab1, mTab2, OFFER1_ID, OFFER2_ID);

        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        verify(mSubscriptionsManager, times(0))
                .unsubscribe(eq(mSubscription1), any(Callback.class));
    }

    @Test
    public void testUnsubscribe_NoOfferId() {
        mImplicitSubscriptionsManager.setupForFetchOfferId(mTab1, mTab2, null, OFFER2_ID);

        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        verify(mSubscriptionsManager, times(0))
                .unsubscribe(eq(mSubscription1), any(Callback.class));
    }

    @Test
    public void testDestroy() {
        mImplicitSubscriptionsManager.destroy();

        verify(mTabModel).removeObserver(any(TabModelObserver.class));
    }

    private TabImpl prepareTab(int id, String urlString, int position,
            CriticalPersistedTabData criticalPersistedTabData) {
        TabImpl tab = mock(TabImpl.class);
        doReturn(id).when(tab).getId();
        GURL gurl = new GURL(urlString);
        doReturn(gurl).when(tab).getUrl();
        doReturn(gurl).when(tab).getOriginalUrl();
        doReturn(tab).when(mTabModel).getTabAt(position);
        UserDataHost userDataHost = new UserDataHost();
        userDataHost.setUserData(CriticalPersistedTabData.class, criticalPersistedTabData);
        doReturn(userDataHost).when(tab).getUserDataHost();
        return tab;
    }

    private void initializeSubscriptionsAndVerify(
            boolean shouldSubscribeTab1, boolean shouldSubscribeTab2) {
        mImplicitSubscriptionsManager.initializeSubscriptions();
        verify(mSubscriptionsManager, times(shouldSubscribeTab1 ? 1 : 0))
                .subscribe(eq(mSubscription1), any(Callback.class));
        verify(mSubscriptionsManager, times(shouldSubscribeTab2 ? 1 : 0))
                .subscribe(eq(mSubscription2), any(Callback.class));
    }

    private void verifyEligibleSubscriptionMetrics(int eligibleCount, int totalCount) {
        assertThat(RecordHistogram.getHistogramValueCountForTesting(TAB_ELIGIBLE_HISTOGRAM, 1),
                equalTo(eligibleCount));
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(TAB_ELIGIBLE_HISTOGRAM),
                equalTo(totalCount));
    }
}
