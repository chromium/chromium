// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData.PriceDrop;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabDataService;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabDataService.PriceChangeItem;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Test relating to {@link PriceChangeModuleMediator} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PriceChangeModuleMediatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private ShoppingPersistedTabDataService mService;
    @Mock private FaviconHelper mFaviconHelper;
    @Mock private UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock private Bitmap mFaviconBitmap;

    private PriceChangeModuleMediator mMediator;
    private SharedPreferencesManager mSharedPreferenceManager;
    private PropertyModel mModel;
    private MockTab mTab;

    private static final int FAVICON_SIZE = 1;
    private static final GURL PRODUCT_URL = new GURL("https://www.foo.com");
    private static final GURL PRODUCT_IMAGE_URL = new GURL("https://www.foo.com/image");
    private static final String PRODUCT_TITLE = "product foo";
    private static final String PRODUCT_URL_DOMAIN = "foo.com";
    private static final String CURRENT_PRICE = "$100";
    private static final String PREVIOUS_PRICE = "$150";

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);
        doReturn(mResources).when(mContext).getResources();
        doReturn(FAVICON_SIZE).when(mResources).getDimensionPixelSize(R.dimen.default_favicon_size);
        doReturn(mTabModel).when(mTabModelSelector).getModel(false);
        ShoppingPersistedTabDataService.setServiceForTesting(mService);

        mTab = new MockTab(123, mProfile);
        mModel = new PropertyModel(PriceChangeModuleProperties.ALL_KEYS);
        mMediator =
                new PriceChangeModuleMediator(
                        mContext, mModel, mProfile, mTabModelSelector, mFaviconHelper);
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();

        Map<String, Boolean> featureOverride = new HashMap<>();
        featureOverride.put(ChromeFeatureList.PRICE_CHANGE_MODULE, true);
        FeatureList.setTestFeatures(featureOverride);
    }

    @After
    public void tearDown() {
        mSharedPreferenceManager.writeStringSet(
                PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP, new HashSet<>());
    }

    @Test
    @SmallTest
    public void testShowModule_ServiceNotInitialized() {
        MockTab tab1 = new MockTab(456, mProfile);
        MockTab tab2 = new MockTab(789, mProfile);
        doReturn(2).when(mTabModel).getCount();
        doReturn(tab1).when(mTabModel).getTabAt(0);
        doReturn(tab2).when(mTabModel).getTabAt(1);
        mSharedPreferenceManager.writeStringSet(
                PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP,
                new HashSet<>(
                        new HashSet<>(
                                Arrays.asList(
                                        String.valueOf(tab1.getId()),
                                        String.valueOf(tab2.getId())))));
        doReturn(false).when(mService).isInitialized();

        mMediator.showModule();

        verify(mService).initialize(eq(new HashSet<>(Arrays.asList(tab1, tab2))));
        verify(mService).getAllShoppingPersistedTabDataWithPriceDrop(any(Callback.class));
    }

    @Test
    @SmallTest
    public void testShowModule_ServiceInitialized() {
        doReturn(true).when(mService).isInitialized();
        when(mUrlUtilitiesJniMock.getDomainAndRegistry(eq(PRODUCT_URL.getSpec()), anyBoolean()))
                .then(inv -> PRODUCT_URL_DOMAIN);
        mTab.setGurlOverrideForTesting(PRODUCT_URL);
        // Set up ShoppingPersistedTabData to be returned from service.
        ShoppingPersistedTabData data = mock(ShoppingPersistedTabData.class);
        PriceDrop priceDrop = new PriceDrop(CURRENT_PRICE, PREVIOUS_PRICE);
        when(data.getProductImageUrl()).thenReturn(PRODUCT_IMAGE_URL);
        when(data.getProductTitle()).thenReturn(PRODUCT_TITLE);
        when(data.getPriceDrop()).thenReturn(priceDrop);

        mMediator.showModule();

        // Mock return value of ShoppingPersistedTabDataService.
        ArgumentCaptor<Callback<List<PriceChangeItem>>> dataCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mService).getAllShoppingPersistedTabDataWithPriceDrop(dataCallbackCaptor.capture());
        PriceChangeItem item = new PriceChangeItem(mTab, data);
        dataCallbackCaptor.getValue().onResult(new ArrayList<>(Arrays.asList(item)));

        verify(mService, times(0)).initialize(any(Set.class));
        assertEquals(
                PRODUCT_TITLE, mModel.get(PriceChangeModuleProperties.MODULE_PRODUCT_NAME_STRING));
        assertEquals(
                PRODUCT_URL_DOMAIN, mModel.get(PriceChangeModuleProperties.MODULE_DOMAIN_STRING));
        assertEquals(
                CURRENT_PRICE, mModel.get(PriceChangeModuleProperties.MODULE_CURRENT_PRICE_STRING));
        assertEquals(
                PREVIOUS_PRICE,
                mModel.get(PriceChangeModuleProperties.MODULE_PREVIOUS_PRICE_STRING));

        // Mock return value of FaviconHelper.
        ArgumentCaptor<FaviconImageCallback> faviconCallbackCaptor =
                ArgumentCaptor.forClass(FaviconImageCallback.class);
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        eq(mProfile),
                        eq(PRODUCT_URL),
                        eq(FAVICON_SIZE),
                        faviconCallbackCaptor.capture());
        faviconCallbackCaptor.getValue().onFaviconAvailable(mFaviconBitmap, new GURL(""));

        assertEquals(mFaviconBitmap, mModel.get(PriceChangeModuleProperties.MODULE_FAVICON_BITMAP));
    }
}
