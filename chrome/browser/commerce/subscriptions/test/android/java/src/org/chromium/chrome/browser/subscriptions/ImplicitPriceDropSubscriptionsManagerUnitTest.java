// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.subscriptions;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
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
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.commerce.core.CommerceSubscription;
import org.chromium.components.commerce.core.IdentifierType;
import org.chromium.components.commerce.core.ManagementType;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.commerce.core.SubscriptionType;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/** Tests for {@link ImplicitPriceDropSubscriptionsManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImplicitPriceDropSubscriptionsManagerUnitTest {
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

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
                TabModelSelector tabModelSelector, ShoppingService shoppingService) {
            super(tabModelSelector, shoppingService);
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

    @Mock TabModel mTabModel;
    @Mock TabModelSelector mTabModelSelector;
    @Mock ShoppingService mShoppingService;
    @Captor ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    @Captor ArgumentCaptor<CommerceSubscription> mSubscriptionCaptor;

    private TabImpl mTab1;
    private TabImpl mTab2;
    private CommerceSubscription mSubscription1;
    private CommerceSubscription mSubscription2;
    private TestImplicitPriceDropSubscriptionsManager mImplicitSubscriptionsManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTab1 = prepareTab(TAB1_ID, URL1, POSITION1);
        mTab2 = prepareTab(TAB2_ID, URL2, POSITION2);
        // Mock that tab1 and tab2 are both stale tabs.
        long fakeTimestamp =
                System.currentTimeMillis()
                        - TimeUnit.SECONDS.toMillis(
                                ShoppingPersistedTabData.getStaleTabThresholdSeconds())
                        + TimeUnit.DAYS.toMillis(7);
        doReturn(fakeTimestamp).when(mTab1).getTimestampMillis();
        doReturn(fakeTimestamp).when(mTab2).getTimestampMillis();
        mSubscription1 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.OFFER_ID,
                        OFFER1_ID,
                        ManagementType.CHROME_MANAGED,
                        null);
        mSubscription2 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.OFFER_ID,
                        OFFER2_ID,
                        ManagementType.CHROME_MANAGED,
                        null);
        doReturn(2).when(mTabModel).getCount();
        doReturn(mTabModel).when(mTabModelSelector).getModel(false);
        doNothing().when(mTabModel).addObserver(mTabModelObserverCaptor.capture());

        mImplicitSubscriptionsManager =
                new TestImplicitPriceDropSubscriptionsManager(mTabModelSelector, mShoppingService);
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
        mTab1 = prepareTab(TAB1_ID, URL1, POSITION1);
        mTab2 = prepareTab(TAB2_ID, URL1, POSITION2);
        long fakeTimestamp =
                System.currentTimeMillis()
                        - TimeUnit.SECONDS.toMillis(
                                ShoppingPersistedTabData.getStaleTabThresholdSeconds())
                        + TimeUnit.DAYS.toMillis(7);
        doReturn(fakeTimestamp).when(mTab1).getTimestampMillis();
        doReturn(fakeTimestamp).when(mTab2).getTimestampMillis();
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
        doReturn(
                        System.currentTimeMillis()
                                - TimeUnit.SECONDS.toMillis(
                                        ShoppingPersistedTabData.getStaleTabThresholdSeconds())
                                - TimeUnit.DAYS.toMillis(7))
                .when(mTab1)
                .getTimestampMillis();

        initializeSubscriptionsAndVerify(false, true);
        verifyEligibleSubscriptionMetrics(1, 2);
    }

    @Test
    public void testInitialSubscription_TabTooNew() {
        doReturn(System.currentTimeMillis()).when(mTab1).getTimestampMillis();

        initializeSubscriptionsAndVerify(false, true);
        verifyEligibleSubscriptionMetrics(1, 2);
    }

    @Test
    public void testTabClosure() {
        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        verify(mShoppingService, times(1))
                .unsubscribe(mSubscriptionCaptor.capture(), any(Callback.class));
        assertEquals(OFFER1_ID, mSubscriptionCaptor.getValue().id);
    }

    @Test
    public void testTabRemove() {
        mTabModelObserverCaptor.getValue().tabRemoved(mTab1);
        verify(mShoppingService, times(1))
                .unsubscribe(mSubscriptionCaptor.capture(), any(Callback.class));
        assertEquals(OFFER1_ID, mSubscriptionCaptor.getValue().id);
    }

    @Test
    public void testTabSelected() {
        mTabModelObserverCaptor.getValue().didSelectTab(mTab1, TabSelectionType.FROM_USER, TAB2_ID);
        verify(mShoppingService, times(1))
                .unsubscribe(mSubscriptionCaptor.capture(), any(Callback.class));
        assertEquals(OFFER1_ID, mSubscriptionCaptor.getValue().id);
    }

    @Test
    public void testUnsubscribe_NotUnique() {
        mTab1 = prepareTab(TAB1_ID, URL1, POSITION1);
        mTab2 = prepareTab(TAB2_ID, URL1, POSITION2);
        mImplicitSubscriptionsManager.setupForFetchOfferId(mTab1, mTab2, OFFER1_ID, OFFER2_ID);

        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        verify(mShoppingService, times(0)).unsubscribe(any(), any(Callback.class));
    }

    @Test
    public void testUnsubscribe_NoOfferId() {
        mImplicitSubscriptionsManager.setupForFetchOfferId(mTab1, mTab2, null, OFFER2_ID);

        mTabModelObserverCaptor.getValue().tabClosureCommitted(mTab1);
        verify(mShoppingService, times(0)).unsubscribe(any(), any(Callback.class));
    }

    @Test
    public void testDestroy() {
        mImplicitSubscriptionsManager.destroy();

        verify(mTabModel).removeObserver(any(TabModelObserver.class));
    }

    private TabImpl prepareTab(int id, String urlString, int position) {
        TabImpl tab = mock(TabImpl.class);
        doReturn(id).when(tab).getId();
        GURL gurl = new GURL(urlString);
        doReturn(gurl).when(tab).getUrl();
        doReturn(gurl).when(tab).getOriginalUrl();
        doReturn(tab).when(mTabModel).getTabAt(position);
        return tab;
    }

    private void initializeSubscriptionsAndVerify(
            boolean shouldSubscribeTab1, boolean shouldSubscribeTab2) {
        mImplicitSubscriptionsManager.initializeSubscriptions();
        if (shouldSubscribeTab1 && shouldSubscribeTab2) {
            verify(mShoppingService, times(2))
                    .subscribe(mSubscriptionCaptor.capture(), any(Callback.class));
            assertEquals(OFFER1_ID, mSubscriptionCaptor.getAllValues().get(0).id);
            assertEquals(OFFER2_ID, mSubscriptionCaptor.getAllValues().get(1).id);
        } else if (shouldSubscribeTab1 || shouldSubscribeTab2) {
            verify(mShoppingService, times(1))
                    .subscribe(mSubscriptionCaptor.capture(), any(Callback.class));
            assertEquals(
                    shouldSubscribeTab1 ? OFFER1_ID : OFFER2_ID, mSubscriptionCaptor.getValue().id);
        } else {
            verify(mShoppingService, times(0)).subscribe(any(), any(Callback.class));
        }
    }

    private void verifyEligibleSubscriptionMetrics(int eligibleCount, int totalCount) {
        assertThat(
                RecordHistogram.getHistogramValueCountForTesting(TAB_ELIGIBLE_HISTOGRAM, 1),
                equalTo(eligibleCount));
        assertThat(
                RecordHistogram.getHistogramTotalCountForTesting(TAB_ELIGIBLE_HISTOGRAM),
                equalTo(totalCount));
    }
}
