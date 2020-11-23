// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcher;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointFetcherJni;
import org.chromium.chrome.browser.endpoint_fetcher.EndpointResponse;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Locale;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Test relating to {@link ShoppingPersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ShoppingPersistedTabDataTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public JniMocker mMocker = new JniMocker();

    private static final int TAB_ID = 1;
    private static final boolean IS_INCOGNITO = false;

    @Mock
    EndpointFetcher.Natives mEndpointFetcherJniMock;

    // Tracks if the endpoint fetcher has been called once or not
    private boolean mCalledOnce;

    private static final long PRICE_MICROS = 123456789012345L;
    private static final long UPDATED_PRICE_MICROS = 287000000L;
    private static final long HIGH_PRICE_MICROS = 141000000L;
    private static final long LOW_PRICE_MICROS = 100000000L;
    private static final String HIGH_PRICE_FORMATTED = "$141";
    private static final String LOW_PRICE_FORMATTED = "$100";

    private static final String EMPTY_PRICE = "";
    private static final String ENDPOINT_RESPONSE_INITIAL =
            "{\"representations\" : [{\"type\" : \"SHOPPING\", \"productTitle\" : \"Book of Pie\","
            + String.format(Locale.US, "\"price\" : %d, \"currency\" : \"USD\"}]}", PRICE_MICROS);

    private static final String ENDPOINT_RESPONSE_UPDATE =
            "{\"representations\" : [{\"type\" : \"SHOPPING\", \"productTitle\" : \"Book of Pie\","
            + String.format(
                    Locale.US, "\"price\" : %d, \"currency\" : \"USD\"}]}", UPDATED_PRICE_MICROS);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(EndpointFetcherJni.TEST_HOOKS, mEndpointFetcherJniMock);
        PersistedTabDataConfiguration.setUseTestConfig(true);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testShoppingProto() {
        Tab tab = new MockTab(TAB_ID, IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        shoppingPersistedTabData.setPriceMicros(PRICE_MICROS, null);
        byte[] serialized = shoppingPersistedTabData.serialize();
        ShoppingPersistedTabData deserialized = new ShoppingPersistedTabData(tab);
        deserialized.deserialize(serialized);
        Assert.assertEquals(PRICE_MICROS, deserialized.getPriceMicros());
        Assert.assertEquals(
                ShoppingPersistedTabData.NO_PRICE_KNOWN, deserialized.getPreviousPriceMicros());
    }

    @SmallTest
    @Test
    public void testShoppingPriceChange() {
        shoppingPriceChange(createTabOnUiThread(TAB_ID, IS_INCOGNITO));
    }

    @SmallTest
    @Test
    public void testShoppingPriceChangeExtraFetchAfterChange() {
        Tab tab = createTabOnUiThread(TAB_ID, IS_INCOGNITO);
        long mLastPriceChangeTimeMs = shoppingPriceChange(tab);
        final Semaphore updateTtlSemaphore = new Semaphore(0);
        // Set TimeToLive such that a refetch will be forced
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PersistedTabData.from(tab, ShoppingPersistedTabData.class).setTimeToLiveMs(-1000);
            updateTtlSemaphore.release();
        });
        acquireSemaphore(updateTtlSemaphore);
        final Semaphore semaphore = new Semaphore(0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                verifyEndpointFetcherCalled(3);
                Assert.assertEquals(
                        UPDATED_PRICE_MICROS, shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(
                        PRICE_MICROS, shoppingPersistedTabData.getPreviousPriceMicros());
                Assert.assertEquals(mLastPriceChangeTimeMs,
                        shoppingPersistedTabData.getLastPriceChangeTimeMs());
                semaphore.release();
            });
        });
        acquireSemaphore(semaphore);
    }

    private long shoppingPriceChange(Tab tab) {
        final Semaphore initialSemaphore = new Semaphore(0);
        final Semaphore updateSemaphore = new Semaphore(0);
        mockEndpointResponse();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                verifyEndpointFetcherCalled(1);
                Assert.assertEquals(PRICE_MICROS, shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabData.NO_PRICE_KNOWN,
                        shoppingPersistedTabData.getPreviousPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabData.NO_TRANSITIONS_OCCURRED,
                        shoppingPersistedTabData.getLastPriceChangeTimeMs());
                // By setting time to live to be a negative number, an update
                // will be forced in the subsequent call
                shoppingPersistedTabData.setTimeToLiveMs(-1000);
                initialSemaphore.release();
            });
        });
        acquireSemaphore(initialSemaphore);
        long firstUpdateTime = getTimeLastUpdatedOnUiThread(tab);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (updatedShoppingPersistedTabData) -> {
                verifyEndpointFetcherCalled(2);
                Assert.assertEquals(
                        UPDATED_PRICE_MICROS, updatedShoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(
                        PRICE_MICROS, updatedShoppingPersistedTabData.getPreviousPriceMicros());
                Assert.assertTrue(firstUpdateTime
                        < updatedShoppingPersistedTabData.getLastPriceChangeTimeMs());
                updateSemaphore.release();
            });
        });
        acquireSemaphore(updateSemaphore);
        return getTimeLastUpdatedOnUiThread(tab);
    }

    @SmallTest
    @Test
    public void testNoRefetch() {
        final Semaphore initialSemaphore = new Semaphore(0);
        final Semaphore updateSemaphore = new Semaphore(0);
        Tab tab = createTabOnUiThread(TAB_ID, IS_INCOGNITO);
        mockEndpointResponse();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertEquals(PRICE_MICROS, shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabData.NO_PRICE_KNOWN,
                        shoppingPersistedTabData.getPreviousPriceMicros());
                // By setting time to live to be a negative number, an update
                // will be forced in the subsequent call
                initialSemaphore.release();
            });
        });
        acquireSemaphore(initialSemaphore);
        verifyEndpointFetcherCalled(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ShoppingPersistedTabData.from(tab, (shoppingPersistedTabData) -> {
                Assert.assertEquals(PRICE_MICROS, shoppingPersistedTabData.getPriceMicros());
                Assert.assertEquals(ShoppingPersistedTabData.NO_PRICE_KNOWN,
                        shoppingPersistedTabData.getPreviousPriceMicros());

                // By setting time to live to be a negative number, an update
                // will be forced in the subsequent call
                updateSemaphore.release();
            });
        });
        acquireSemaphore(updateSemaphore);
        // EndpointFetcher should not have been called a second time - because we haven't passed the
        // time to live
        verifyEndpointFetcherCalled(1);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDrop() {
        Tab tab = createTabOnUiThread(TAB_ID, IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        // Prices unknown is not a price drop
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());

        // Same price is not a price drop
        shoppingPersistedTabData.setPriceMicros(HIGH_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(HIGH_PRICE_MICROS);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());

        // Lower -> Higher price is not a price drop
        shoppingPersistedTabData.setPriceMicros(HIGH_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(LOW_PRICE_MICROS);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());

        // Actual price drop (Higher -> Lower)
        shoppingPersistedTabData.setPriceMicros(LOW_PRICE_MICROS, null);
        shoppingPersistedTabData.setPreviousPriceMicros(HIGH_PRICE_MICROS);
        ShoppingPersistedTabData.PriceDrop priceDrop = shoppingPersistedTabData.getPriceDrop();
        Assert.assertEquals(LOW_PRICE_FORMATTED, priceDrop.price);
        Assert.assertEquals(HIGH_PRICE_FORMATTED, priceDrop.previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterSamePrice() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $10 -> $10 is not a price drop (same price)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(10_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterNoFormattedPriceDifference() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $10.40 -> $10 (which would be displayed $10 -> $10 is not a price drop)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10_400_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(10_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAbsoluteDifferenceTooSmall() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $2 -> $1 ($1 price drop - less than $2. Not big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(1_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(2_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterPriceIncrease() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $9.33 -> $9.66 price increase is not a price drop
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(9_330_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(9_660_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterPercentageDropNotEnough() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $50 -> $46 (8% price drop (less than 10%) not big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(50_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(46_000_000L);
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop1() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $10 -> $7 (30% and $3 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(7_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$7.00", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$10", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop2() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $15.72 -> $4.80 (70% and $10.92 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(15_720_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(4_800_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$4.80", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$16", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop3() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $20 -> $10 (50% and $10 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(20_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(10_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$10", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$20", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop4() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $30 -> $27 (10% and $3 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(30_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(27_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$27", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$30", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop5() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $30.65 -> $25.50 (17% and $5.15 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(30_650_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(25_500_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$26", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$31", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop6() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $9.65 -> $3.80 (40% and $5.85 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(9_650_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(3_800_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$3.80", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$9.65", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop7() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $9.33 -> $0.90 (96% price drop and $8.43 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(9_330_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(900_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$0.90", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$9.33", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop8() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $20 -> $18 (10% price drop and $2 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(20_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(18_000_000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$18", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$20", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testPriceDropFilterAllowedPriceDrop9() {
        ShoppingPersistedTabData shoppingPersistedTabData =
                new ShoppingPersistedTabData(createTabOnUiThread(TAB_ID, IS_INCOGNITO));

        // $2 -> $0 ($2 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(2_000_000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(0L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$0.00", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$2.00", shoppingPersistedTabData.getPriceDrop().previousPrice);
    }

    @UiThreadTest
    @SmallTest
    @Test
    public void testStalePriceDrop() {
        Tab tab = createTabOnUiThread(TAB_ID, IS_INCOGNITO);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        // $10 -> $5 (50% and $5 price drop is big enough)
        shoppingPersistedTabData.setPreviousPriceMicrosForTesting(10000000L);
        shoppingPersistedTabData.setPriceMicrosForTesting(5000000L);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertEquals("$5.00", shoppingPersistedTabData.getPriceDrop().price);
        Assert.assertEquals("$10", shoppingPersistedTabData.getPriceDrop().previousPrice);

        // Price drops greater than a week old are removed
        shoppingPersistedTabData.setLastPriceChangeTimeMsForTesting(
                System.currentTimeMillis() - TimeUnit.DAYS.toMillis(8));
        Assert.assertNull(shoppingPersistedTabData.getPriceDrop());
    }

    private void verifyEndpointFetcherCalled(int numTimes) {
        verify(mEndpointFetcherJniMock, times(numTimes))
                .nativeFetchOAuth(any(Profile.class), anyString(), anyString(), anyString(),
                        anyString(), any(String[].class), anyString(), anyLong(),
                        any(Callback.class));
    }

    private static Tab createTabOnUiThread(int tabId, boolean isIncognito) {
        AtomicReference<Tab> res = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { res.set(MockTab.createAndInitialize(TAB_ID, IS_INCOGNITO)); });
        return res.get();
    }

    private static long getTimeLastUpdatedOnUiThread(Tab tab) {
        AtomicReference<Long> res = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            res.set(PersistedTabData.from(tab, ShoppingPersistedTabData.class)
                            .getLastPriceChangeTimeMs());
        });
        return res.get();
    }

    private void mockEndpointResponse() {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                Callback callback = (Callback) invocation.getArguments()[8];
                String res = mCalledOnce ? ENDPOINT_RESPONSE_UPDATE : ENDPOINT_RESPONSE_INITIAL;
                mCalledOnce = true;
                callback.onResult(new EndpointResponse(res));
                return null;
            }
        })
                .when(mEndpointFetcherJniMock)
                .nativeFetchOAuth(any(Profile.class), anyString(), anyString(), anyString(),
                        anyString(), any(String[].class), anyString(), anyLong(),
                        any(Callback.class));
    }

    private static void acquireSemaphore(Semaphore semaphore) {
        try {
            semaphore.acquire();
        } catch (InterruptedException e) {
            // Throw Runtime exception to make catching InterruptedException unnecessary
            throw new RuntimeException(e);
        }
    }
}
