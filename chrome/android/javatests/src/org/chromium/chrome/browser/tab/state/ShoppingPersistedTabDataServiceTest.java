// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP;
import static org.chromium.chrome.browser.tab.state.ShoppingPersistedTabDataService.isDataEligibleForPriceDrop;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactoryJni;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashSet;
import java.util.concurrent.TimeoutException;

/** Test relating to {@link ShoppingPersistedTabDataService} */
@RunWith(BaseJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "force-fieldtrials=Study/Group"
})
public class ShoppingPersistedTabDataServiceTest {
    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule public JniMocker mMocker = new JniMocker();

    @Mock protected OptimizationGuideBridgeFactory.Natives mOptimizationGuideBridgeFactoryJniMock;
    @Mock protected OptimizationGuideBridge mOptimizationGuideBridgeMock;

    @Mock protected Profile mProfileMock;

    static final long ONE_SECOND = 1000;
    static final long HALF_SECOND = 500;

    private ShoppingPersistedTabDataService mService;
    private SharedPreferencesManager mSharedPrefsManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(
                OptimizationGuideBridgeFactoryJni.TEST_HOOKS,
                mOptimizationGuideBridgeFactoryJniMock);
        doReturn(mOptimizationGuideBridgeMock)
                .when(mOptimizationGuideBridgeFactoryJniMock)
                .getForProfile(mProfileMock);

        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponse(
                mOptimizationGuideBridgeMock,
                OptimizationType.SHOPPING_PAGE_PREDICTOR,
                OptimizationGuideDecision.TRUE,
                null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PersistedTabDataConfiguration.setUseTestConfig(true);
                });
        ProfileManager.setLastUsedProfileForTesting(mProfileMock);
        mService = new ShoppingPersistedTabDataService();
        mSharedPrefsManager = ChromeSharedPreferences.getInstance();
        ShoppingPersistedTabData.enablePriceTrackingWithOptimizationGuideForTesting();
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(false);
    }

    @After
    public void tearDown() {
        mSharedPrefsManager.writeStringSet(
                PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP, new HashSet<>());
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
        Assert.assertFalse(isDataEligibleForPriceDrop(null));

        MockTab tab = new MockTab(ShoppingPersistedTabDataTestUtils.TAB_ID, mProfileMock);
        ShoppingPersistedTabData shoppingPersistedTabData = new ShoppingPersistedTabData(tab);
        Assert.assertFalse(isDataEligibleForPriceDrop(shoppingPersistedTabData));

        shoppingPersistedTabData.setPriceMicros(ShoppingPersistedTabDataTestUtils.LOW_PRICE_MICROS);
        shoppingPersistedTabData.setPreviousPriceMicros(
                ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS);
        shoppingPersistedTabData.setCurrencyCode(
                ShoppingPersistedTabDataTestUtils.GREAT_BRITAIN_CURRENCY_CODE);
        GURL url = new GURL("https://www.google.com");
        shoppingPersistedTabData.setPriceDropGurl(url);
        tab.setGurlOverrideForTesting(url);
        Assert.assertNotNull(shoppingPersistedTabData.getPriceDrop());
        Assert.assertFalse(isDataEligibleForPriceDrop(shoppingPersistedTabData));

        shoppingPersistedTabData.setProductImageUrl(
                new GURL(ShoppingPersistedTabDataTestUtils.FAKE_PRODUCT_IMAGE_URL));
        Assert.assertFalse(isDataEligibleForPriceDrop(shoppingPersistedTabData));

        shoppingPersistedTabData.setProductTitle(
                ShoppingPersistedTabDataTestUtils.FAKE_PRODUCT_TITLE);
        Assert.assertTrue(isDataEligibleForPriceDrop(shoppingPersistedTabData));
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testNotifyPriceDropStatus() {
        Tab tab1 = new MockTab(123, mProfileMock);
        Tab tab2 = new MockTab(456, mProfileMock);
        Tab tab3 = mock(Tab.class);
        doReturn(true).when(tab3).isDestroyed();
        doReturn(789).when(tab3).getId();

        Assert.assertFalse(mService.isInitialized());
        Assert.assertEquals(new HashSet<>(), mService.getTabsWithPriceDropForTesting());
        Assert.assertEquals(
                new HashSet<>(),
                mSharedPrefsManager.readStringSet(PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP));

        mService.notifyPriceDropStatus(tab1, true);
        mService.notifyPriceDropStatus(tab2, true);
        mService.notifyPriceDropStatus(tab3, true);
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(tab1, tab2)),
                mService.getTabsWithPriceDropForTesting());
        Assert.assertEquals(
                new HashSet<>(
                        Arrays.asList(String.valueOf(tab1.getId()), String.valueOf(tab2.getId()))),
                mSharedPrefsManager.readStringSet(PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP));
        Assert.assertTrue(mService.isInitialized());

        mService.notifyPriceDropStatus(tab1, false);
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(tab2)), mService.getTabsWithPriceDropForTesting());
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(String.valueOf(tab2.getId()))),
                mSharedPrefsManager.readStringSet(PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP));
        Assert.assertTrue(mService.isInitialized());

        mService.notifyPriceDropStatus(tab2, false);
        Assert.assertEquals(0, mService.getTabsWithPriceDropForTesting().size());
        Assert.assertEquals(
                0,
                mSharedPrefsManager
                        .readStringSet(PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP)
                        .size());
        Assert.assertTrue(mService.isInitialized());
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testInitializeService() {
        Assert.assertFalse(mService.isInitialized());
        Assert.assertEquals(0, mService.getTabsWithPriceDropForTesting().size());

        Tab tab1 = new MockTab(123, mProfileMock);
        mService.initialize(new HashSet<>(Arrays.asList(tab1)));

        Assert.assertTrue(mService.isInitialized());
        Assert.assertEquals(
                new HashSet<>(Arrays.asList(tab1)), mService.getTabsWithPriceDropForTesting());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.PRICE_CHANGE_MODULE})
    @CommandLineFlags.Add({
        "force-fieldtrial-params=Study.Group:return_empty_price_drops_until_init/false"
    })
    public void testGetAllShoppingPersistedTabDataWithPriceDrop() throws TimeoutException {
        // tab1 is not eligible as there is no price drop.
        ProfileManager.setLastUsedProfileForTesting(mProfileMock);
        MockTab tab1 = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(123, mProfileMock);
        GURL url1 = ShoppingPersistedTabDataTestUtils.DEFAULT_GURL;
        tab1.setGurlOverrideForTesting(url1);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponseForURL(
                url1,
                mOptimizationGuideBridgeMock,
                OptimizationType.PRICE_TRACKING,
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_INITIAL);

        // tab2 is eligible.
        MockTab tab2 = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(456, mProfileMock);
        GURL url2 = ShoppingPersistedTabDataTestUtils.GURL_FOO;
        tab2.setGurlOverrideForTesting(url2);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponseForURL(
                url2,
                mOptimizationGuideBridgeMock,
                OptimizationType.PRICE_TRACKING,
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE);

        // tab3 is eligible.
        MockTab tab3 = ShoppingPersistedTabDataTestUtils.createTabOnUiThread(789, mProfileMock);
        GURL url3 = ShoppingPersistedTabDataTestUtils.GURL_BAR;
        tab3.setGurlOverrideForTesting(url3);
        ShoppingPersistedTabDataTestUtils.mockOptimizationGuideResponseForURL(
                url3,
                mOptimizationGuideBridgeMock,
                OptimizationType.PRICE_TRACKING,
                ShoppingPersistedTabDataTestUtils.MockPriceTrackingResponse
                        .BUYABLE_PRODUCT_AND_PRODUCT_UPDATE_TWO);

        // Set up the recency to be tab1 > tab3 > tab2.
        long currentTimeStamp = System.currentTimeMillis();
        tab1.setTimestampMillis(currentTimeStamp);
        tab2.setTimestampMillis(currentTimeStamp - ONE_SECOND);
        tab3.setTimestampMillis(currentTimeStamp - HALF_SECOND);

        CallbackHelper widened = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mService.initialize(new HashSet<>(Arrays.asList(tab1, tab2, tab3)));
                    ShoppingPersistedTabData.onDeferredStartup();
                    mService.getAllShoppingPersistedTabDataWithPriceDrop(
                            result -> {
                                Assert.assertEquals(2, result.size());

                                Assert.assertEquals(tab3, result.get(0).getTab());
                                Assert.assertEquals(
                                        ShoppingPersistedTabDataTestUtils.HIGH_PRICE_MICROS,
                                        result.get(0)
                                                .getData()
                                                .getPriceDropDataForTesting()
                                                .previousPriceMicros);
                                Assert.assertEquals(
                                        ShoppingPersistedTabDataTestUtils.LOW_PRICE_MICROS,
                                        result.get(0)
                                                .getData()
                                                .getPriceDropDataForTesting()
                                                .priceMicros);
                                Assert.assertEquals(
                                        ShoppingPersistedTabDataTestUtils
                                                .FAKE_PRODUCT_IMAGE_URL_TWO,
                                        result.get(0).getData().getProductImageUrl().getSpec());
                                Assert.assertEquals(
                                        ShoppingPersistedTabDataTestUtils.FAKE_PRODUCT_TITLE_TWO,
                                        result.get(0).getData().getProductTitle());

                                Assert.assertEquals(tab2, result.get(1).getTab());
                                Assert.assertEquals(
                                        ShoppingPersistedTabDataTestUtils.PRICE_MICROS,
                                        result.get(1)
                                                .getData()
                                                .getPriceDropDataForTesting()
                                                .previousPriceMicros);
                                Assert.assertEquals(
                                        ShoppingPersistedTabDataTestUtils.UPDATED_PRICE_MICROS,
                                        result.get(1)
                                                .getData()
                                                .getPriceDropDataForTesting()
                                                .priceMicros);
                                Assert.assertEquals(
                                        ShoppingPersistedTabDataTestUtils.FAKE_PRODUCT_IMAGE_URL,
                                        result.get(1).getData().getProductImageUrl().getSpec());
                                Assert.assertEquals(
                                        ShoppingPersistedTabDataTestUtils.FAKE_PRODUCT_TITLE,
                                        result.get(1).getData().getProductTitle());
                                widened.notifyCalled();
                            });
                });
        widened.waitForOnly();
    }
}
