// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_change;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;
import android.view.View.OnClickListener;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureList;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
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
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcher.Params;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.shadows.ShadowAppCompatResources;
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
@Config(
        manifest = Config.NONE,
        shadows = {ShadowAppCompatResources.class})
public class PriceChangeModuleMediatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Profile mProfile;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private ShoppingPersistedTabDataService mService;
    @Mock private FaviconHelper mFaviconHelper;
    @Mock private UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock private Bitmap mFaviconBitmap;
    @Mock private Bitmap mProductImageBitmap;
    @Mock private ImageFetcher mImageFetcher;
    @Mock private ModuleDelegate mModuleDelegate;

    private Context mContext;
    private PriceChangeModuleMediator mMediator;
    private SharedPreferencesManager mSharedPreferenceManager;
    private PropertyModel mModel;
    private MockTab mTab;
    private int mFaviconSize;

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
        mTab = new MockTab(123, mProfile);
        doReturn(mTabModel).when(mTabModelSelector).getModel(false);
        doReturn(1).when(mTabModel).getCount();
        doReturn(mTab).when(mTabModel).getTabAt(0);
        doReturn(mTab).when(mTabModel).getTabById(mTab.getId());
        doReturn(true).when(mTabModelSelector).isTabStateInitialized();
        ShoppingPersistedTabDataService.setServiceForTesting(mService);

        mContext = RuntimeEnvironment.application;
        mModel = new PropertyModel(PriceChangeModuleProperties.ALL_KEYS);
        mMediator =
                new PriceChangeModuleMediator(
                        mContext,
                        mModel,
                        mProfile,
                        mTabModelSelector,
                        mFaviconHelper,
                        mImageFetcher,
                        mModuleDelegate,
                        ContextUtils.getAppSharedPreferences());
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();
        mFaviconSize = mContext.getResources().getDimensionPixelSize(R.dimen.default_favicon_size);
        PriceTrackingFeatures.setPriceTrackingEnabledForTesting(true);

        Map<String, Boolean> featureOverride = new HashMap<>();
        featureOverride.put(ChromeFeatureList.PRICE_CHANGE_MODULE, true);
        FeatureList.setTestFeatures(featureOverride);
    }

    @After
    public void tearDown() {
        mSharedPreferenceManager.writeStringSet(
                PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP, new HashSet<>());
        mSharedPreferenceManager.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, false);
        mSharedPreferenceManager.writeBoolean(PriceTrackingUtilities.TRACK_PRICES_ON_TABS, false);
    }

    @Test
    @SmallTest
    public void testShowModule_ServiceNotInitialized() {
        MockTab tab1 = new MockTab(456, mProfile);
        MockTab tab2 = new MockTab(789, mProfile);
        doReturn(2).when(mTabModel).getCount();
        doReturn(tab1).when(mTabModel).getTabAt(0);
        doReturn(tab1).when(mTabModel).getTabById(tab1.getId());
        doReturn(tab2).when(mTabModel).getTabAt(1);
        doReturn(tab2).when(mTabModel).getTabById(tab2.getId());
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
        showModuleWithInitializedService();

        assertEquals(
                mContext.getResources()
                        .getQuantityString(
                                org.chromium.chrome.browser.price_change.R.plurals
                                        .price_change_module_title,
                                1),
                mModel.get(PriceChangeModuleProperties.MODULE_TITLE));
        assertEquals(
                PRODUCT_TITLE, mModel.get(PriceChangeModuleProperties.MODULE_PRODUCT_NAME_STRING));
        assertEquals(
                PRODUCT_URL_DOMAIN, mModel.get(PriceChangeModuleProperties.MODULE_DOMAIN_STRING));
        assertEquals(
                CURRENT_PRICE, mModel.get(PriceChangeModuleProperties.MODULE_CURRENT_PRICE_STRING));
        assertEquals(
                PREVIOUS_PRICE,
                mModel.get(PriceChangeModuleProperties.MODULE_PREVIOUS_PRICE_STRING));
        assertEquals(
                mContext.getString(
                        R.string.price_change_module_accessibility_label,
                        PREVIOUS_PRICE,
                        CURRENT_PRICE,
                        PRODUCT_TITLE,
                        PRODUCT_URL_DOMAIN),
                mModel.get(PriceChangeModuleProperties.MODULE_ACCESSIBILITY_LABEL));

        verify(mModuleDelegate).onDataReady(eq(ModuleType.PRICE_CHANGE), eq(mModel));

        // Mock return value of FaviconHelper.
        ArgumentCaptor<FaviconImageCallback> faviconCallbackCaptor =
                ArgumentCaptor.forClass(FaviconImageCallback.class);
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        eq(mProfile),
                        eq(PRODUCT_URL),
                        eq(mFaviconSize),
                        faviconCallbackCaptor.capture());
        faviconCallbackCaptor.getValue().onFaviconAvailable(mFaviconBitmap, new GURL(""));

        assertEquals(mFaviconBitmap, mModel.get(PriceChangeModuleProperties.MODULE_FAVICON_BITMAP));

        // Mock return value of ImageFetcher.
        ArgumentCaptor<Callback<Bitmap>> productImageCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mImageFetcher).fetchImage(any(Params.class), productImageCallbackCaptor.capture());
        productImageCallbackCaptor.getValue().onResult(mProductImageBitmap);

        assertEquals(
                mProductImageBitmap,
                mModel.get(PriceChangeModuleProperties.MODULE_PRODUCT_IMAGE_BITMAP));

        // Check onClickListener setup.
        OnClickListener listener = mModel.get(PriceChangeModuleProperties.MODULE_ON_CLICK_LISTENER);
        listener.onClick(mock(View.class));
        verify(mModuleDelegate).onTabClicked(eq(123), eq(ModuleType.PRICE_CHANGE));
    }

    @Test
    @SmallTest
    public void testShowModule_UseDefaultFavicon() {
        showModuleWithInitializedService();

        // Mock return value of FaviconHelper to be null.
        ArgumentCaptor<FaviconImageCallback> faviconCallbackCaptor =
                ArgumentCaptor.forClass(FaviconImageCallback.class);
        verify(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        eq(mProfile),
                        eq(PRODUCT_URL),
                        eq(mFaviconSize),
                        faviconCallbackCaptor.capture());
        faviconCallbackCaptor.getValue().onFaviconAvailable(null, new GURL(""));

        // Favicon should be setup as default bitmap.
        assertNotNull(mModel.get(PriceChangeModuleProperties.MODULE_FAVICON_BITMAP));
    }

    @Test
    @SmallTest
    public void testShowModule_NoData() {
        doReturn(true).when(mService).isInitialized();

        mMediator.showModule();

        ArgumentCaptor<Callback<List<PriceChangeItem>>> dataCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mService).getAllShoppingPersistedTabDataWithPriceDrop(dataCallbackCaptor.capture());
        dataCallbackCaptor.getValue().onResult(new ArrayList<>());
        verify(mService, times(0)).initialize(any(Set.class));
        verify(mModuleDelegate).onDataFetchFailed(eq(ModuleType.PRICE_CHANGE));
    }

    @Test
    @SmallTest
    public void testShowModule_TabStateNotInitialized() {
        doReturn(false).when(mTabModelSelector).isTabStateInitialized();

        mMediator.showModule();

        verify(mService, never()).initialize(any());
        verify(mService, never()).getAllShoppingPersistedTabDataWithPriceDrop(any());
    }

    @Test
    @SmallTest
    public void testGetModuleType() {
        assertEquals(ModuleType.PRICE_CHANGE, mMediator.getModuleType());
    }

    @Test
    @SmallTest
    public void testPriceAnnotationSettingChange() {
        // Enabling the price annotation won't trigger any change.
        mSharedPreferenceManager.writeBoolean(PriceTrackingUtilities.TRACK_PRICES_ON_TABS, true);
        verify(mModuleDelegate, never()).removeModule(mMediator.getModuleType());

        // Irrelevant SharedPreferences change won't trigger any change.
        mSharedPreferenceManager.writeBoolean(
                PriceTrackingUtilities.PRICE_WELCOME_MESSAGE_CARD, false);
        verify(mModuleDelegate, never()).removeModule(mMediator.getModuleType());

        mSharedPreferenceManager.writeBoolean(PriceTrackingUtilities.TRACK_PRICES_ON_TABS, false);
        verify(mModuleDelegate).removeModule(mMediator.getModuleType());
    }

    @Test
    @SmallTest
    public void testDestroy() {
        mMediator.destroy();

        mSharedPreferenceManager.writeBoolean(PriceTrackingUtilities.TRACK_PRICES_ON_TABS, false);
        verify(mModuleDelegate, never()).removeModule(mMediator.getModuleType());
        verify(mTabModelSelector).removeObserver(eq(mMediator));
    }

    @Test
    @SmallTest
    public void testShowModule_NullTab() {
        doReturn(true).when(mService).isInitialized();

        mMediator.showModule();

        ShoppingPersistedTabData data = mock(ShoppingPersistedTabData.class);
        PriceChangeItem item = new PriceChangeItem(null, data);
        ArgumentCaptor<Callback<List<PriceChangeItem>>> dataCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mService).getAllShoppingPersistedTabDataWithPriceDrop(dataCallbackCaptor.capture());
        dataCallbackCaptor.getValue().onResult(new ArrayList<>(Arrays.asList(item)));
        verify(mService, times(0)).initialize(any(Set.class));
        verify(mModuleDelegate).onDataFetchFailed(eq(ModuleType.PRICE_CHANGE));
    }

    @Test
    @SmallTest
    public void testShowModule_TabFromOtherModel() {
        doReturn(true).when(mService).isInitialized();

        mMediator.showModule();

        ShoppingPersistedTabData data = mock(ShoppingPersistedTabData.class);
        PriceChangeItem item = new PriceChangeItem(mTab, data);
        // Mock that tab is not in the current tab model.
        doReturn(0).when(mTabModel).getCount();
        doReturn(null).when(mTabModel).getTabById(mTab.getId());
        ArgumentCaptor<Callback<List<PriceChangeItem>>> dataCallbackCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mService).getAllShoppingPersistedTabDataWithPriceDrop(dataCallbackCaptor.capture());
        dataCallbackCaptor.getValue().onResult(new ArrayList<>(Arrays.asList(item)));
        verify(mService, times(0)).initialize(any(Set.class));
        verify(mModuleDelegate).onDataFetchFailed(eq(ModuleType.PRICE_CHANGE));
    }

    @Test
    @SmallTest
    public void testOnTabStateInitialized() {
        MockTab tab1 = new MockTab(456, mProfile);
        MockTab tab2 = new MockTab(789, mProfile);
        doReturn(2).when(mTabModel).getCount();
        doReturn(tab1).when(mTabModel).getTabAt(0);
        doReturn(tab1).when(mTabModel).getTabById(tab1.getId());
        doReturn(tab2).when(mTabModel).getTabAt(1);
        doReturn(tab2).when(mTabModel).getTabById(tab2.getId());
        mSharedPreferenceManager.writeStringSet(
                PRICE_TRACKING_IDS_FOR_TABS_WITH_PRICE_DROP,
                new HashSet<>(
                        new HashSet<>(
                                Arrays.asList(
                                        String.valueOf(tab1.getId()),
                                        String.valueOf(tab2.getId())))));
        doReturn(false).when(mService).isInitialized();

        mMediator.onTabStateInitialized();

        verify(mTabModelSelector).removeObserver(eq(mMediator));
        verify(mService).initialize(eq(new HashSet<>(Arrays.asList(tab1, tab2))));
        verify(mService).getAllShoppingPersistedTabDataWithPriceDrop(any(Callback.class));
    }

    public void showModuleWithInitializedService() {
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
    }
}
