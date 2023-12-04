// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import static org.chromium.chrome.browser.tab.state.ShoppingPersistedTabDataService.isDataEligibleForPriceDrop;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashSet;

/** Test relating to {@link ShoppingPersistedTabDataService} */
@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
public class ShoppingPersistedTabDataServiceTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Mock protected Profile mProfileMock;

    private ShoppingPersistedTabDataService mService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(false).when(mProfileMock).isOffTheRecord();
        mService = new ShoppingPersistedTabDataService();
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testGetService() {
        Profile anotherProfile = mock(Profile.class);
        ShoppingPersistedTabDataService serviceOne =
                ShoppingPersistedTabDataService.getForProfile(mProfileMock);
        ShoppingPersistedTabDataService serviceTwo =
                ShoppingPersistedTabDataService.getForProfile(anotherProfile);
        Assert.assertNotEquals(serviceOne, serviceTwo);
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testIsShoppingPersistedTabDataEligible() {
        Assert.assertFalse(isDataEligibleForPriceDrop((null)));

        MockTab tab = new MockTab(ShoppingPersistedTabDataTestUtils.TAB_ID, mProfileMock);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        Assert.assertFalse(isDataEligibleForPriceDrop((shoppingPersistedTabData)));

        shoppingPersistedTabData.setPriceMicros(ShoppingPersistedTabDataTestUtils.LOW_PRICE_MICROS);
        shoppingPersistedTabData.setPreviousPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS);
        shoppingPersistedTabData.setCurrencyCode(
                ShoppingPersistedTabDataTestUtils.GREAT_BRITAIN_CURRENCY_CODE);
        GURL url = new GURL("https://www.google.com");
        shoppingPersistedTabData.setPriceDropGurl(url);
        tab.setGurlOverrideForTesting(url);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertFalse(isDataEligibleForPriceDrop((shoppingPersistedTabData)));

        shoppingPersistedTabData.setProductImageUrl(
                new GURL(ShoppingPersistedTabDataTestUtils.FAKE_PRODUCT_IMAGE_URL));
        Assert.assertFalse(isDataEligibleForPriceDrop((shoppingPersistedTabData)));

        shoppingPersistedTabData.setProductTitle(
                ShoppingPersistedTabDataTestUtils.FAKE_PRODUCT_TITLE);
        Assert.assertTrue(isDataEligibleForPriceDrop((shoppingPersistedTabData)));
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testNotifyPriceDropStatus() {
        Tab tab1 = new MockTab(123, mProfileMock);
        Tab tab2 = new MockTab(456, mProfileMock);

        mService.notifyPriceDropStatus(tab1, true);
        mService.notifyPriceDropStatus(tab2, true);
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(tab1, tab2)),
                mService.getTabsWithPriceDropForTesting());

        mService.notifyPriceDropStatus(tab1, false);
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(tab2)), mService.getTabsWithPriceDropForTesting());

        mService.notifyPriceDropStatus(tab2, false);
        Assert.assertEquals(0, mService.getTabsWithPriceDropForTesting().size());
    }
}
