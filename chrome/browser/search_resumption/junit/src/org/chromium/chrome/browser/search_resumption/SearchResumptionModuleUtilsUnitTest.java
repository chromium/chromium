// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.text.TextUtils;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.UserDataHost;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_resumption.SearchResumptionModuleUtils.ModuleNotShownReason;
import org.chromium.chrome.browser.search_resumption.SearchResumptionUserData.SuggestionResult;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for {@link SearchResumptionModuleUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {SearchResumptionModuleUtilsUnitTest.ShadowChromeFeatureList.class,
                SearchResumptionModuleUtilsUnitTest.ShadowSearchResumptionUserData.class})
@SuppressWarnings("DoNotMock") // Mocks GURL
public class SearchResumptionModuleUtilsUnitTest {
    /** Shadow for {@link SearchResumptionUserData} */
    @Implements(SearchResumptionUserData.class)
    static class ShadowSearchResumptionUserData {
        private static SearchResumptionUserData sSearchResumptionUserData;

        static void setSearchResumptionUserData(SearchResumptionUserData searchResumptionUserData) {
            sSearchResumptionUserData = searchResumptionUserData;
        }

        @Implementation
        public static SearchResumptionUserData getInstance() {
            return sSearchResumptionUserData;
        }
    }

    @Implements(ChromeFeatureList.class)
    static class ShadowChromeFeatureList {
        static final Map<String, Integer> sParamValues = new HashMap<>();
        static boolean sEnableScrollableMVT;
        static boolean sEnableSearchResumptionModule;

        @Implementation
        public static boolean isEnabled(String featureName) {
            if (featureName.equals(ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID)) {
                return sEnableScrollableMVT;
            }
            return featureName.equals(ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID)
                    && sEnableSearchResumptionModule;
        }

        @Implementation
        public static int getFieldTrialParamByFeatureAsInt(
                String featureName, String paramName, int defaultValue) {
            return sParamValues.containsKey(paramName)
                    ? Integer.valueOf(sParamValues.get(paramName))
                    : defaultValue;
        }
    }

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Mock
    private TemplateUrlService mTemplateUrlService;
    @Mock
    private IdentityServicesProvider mIdentityServicesProvider;
    @Mock
    private SyncService mSyncServiceMock;
    @Mock
    private IdentityManager mIdentityManager;
    @Mock
    private Profile mProfile;
    @Mock
    private Tab mTab;
    @Mock
    private Tab mTabToTrack;
    @Mock
    private GURL mGurl1;
    @Mock
    private GURL mGurl2;
    @Mock
    private CriticalPersistedTabData mCriticalPersistedTabData;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        UserDataHost tabDataHost = new UserDataHost();
        when(mTab.getUserDataHost()).thenReturn(tabDataHost);
        when(mTabToTrack.getUserDataHost()).thenReturn(tabDataHost);
        tabDataHost.setUserData(CriticalPersistedTabData.class, mCriticalPersistedTabData);

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(any());
        SyncServiceFactory.overrideForTests(mSyncServiceMock);

        ShadowChromeFeatureList.sEnableScrollableMVT = true;
        ShadowChromeFeatureList.sEnableSearchResumptionModule = true;
    }

    @After
    public void tearDown() {
        ShadowChromeFeatureList.sEnableScrollableMVT = false;
        ShadowChromeFeatureList.sEnableSearchResumptionModule = false;
        ShadowChromeFeatureList.sParamValues.clear();
    }

    @Test
    @SmallTest
    // clang-format off
    public void testShouldShowSearchResumptionModule() {
        // clang-format on
        Assert.assertTrue(
                ChromeFeatureList.isEnabled(ChromeFeatureList.SHOW_SCROLLABLE_MVT_ON_NTP_ANDROID));
        Assert.assertTrue(
                ChromeFeatureList.isEnabled(ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID));

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        Assert.assertFalse(SearchResumptionModuleUtils.shouldShowSearchResumptionModule(mProfile));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_NOT_SHOW,
                        ModuleNotShownReason.DEFAULT_ENGINE_NOT_GOOGLE));

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        when(mSyncServiceMock.hasKeepEverythingSynced()).thenReturn(false);
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(anyInt());
        Assert.assertFalse(SearchResumptionModuleUtils.shouldShowSearchResumptionModule(mProfile));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_NOT_SHOW,
                        ModuleNotShownReason.NOT_SYNC));

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        when(mSyncServiceMock.hasKeepEverythingSynced()).thenReturn(true);
        doReturn(false).when(mIdentityManager).hasPrimaryAccount(anyInt());
        Assert.assertFalse(SearchResumptionModuleUtils.shouldShowSearchResumptionModule(mProfile));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_NOT_SHOW,
                        ModuleNotShownReason.NOT_SIGN_IN));
    }

    @Test
    @SmallTest
    public void testIsTabToTrackValid() {
        doReturn(true).when(mTabToTrack).isNativePage();
        Assert.assertFalse(SearchResumptionModuleUtils.isTabToTrackValid(mTabToTrack));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_NOT_SHOW,
                        ModuleNotShownReason.TAB_NOT_VALID));

        doReturn(false).when(mTabToTrack).isNativePage();
        doReturn(true).when(mTabToTrack).isIncognito();
        Assert.assertFalse(SearchResumptionModuleUtils.isTabToTrackValid(mTabToTrack));
        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_NOT_SHOW,
                        ModuleNotShownReason.TAB_NOT_VALID));

        doReturn(mGurl1).when(mTabToTrack).getUrl();
        doReturn(false).when(mTabToTrack).isNativePage();
        doReturn(false).when(mTabToTrack).isIncognito();
        doReturn(true).when(mGurl1).isEmpty();
        Assert.assertFalse(SearchResumptionModuleUtils.isTabToTrackValid(mTabToTrack));
        Assert.assertEquals(3,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_NOT_SHOW,
                        ModuleNotShownReason.TAB_NOT_VALID));

        doReturn(false).when(mTabToTrack).isNativePage();
        doReturn(false).when(mTabToTrack).isIncognito();
        doReturn(false).when(mGurl1).isEmpty();
        doReturn(false).when(mGurl1).isValid();
        Assert.assertFalse(SearchResumptionModuleUtils.isTabToTrackValid(mTabToTrack));
        Assert.assertEquals(4,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_NOT_SHOW,
                        ModuleNotShownReason.TAB_NOT_VALID));

        doReturn(false).when(mTabToTrack).isNativePage();
        doReturn(false).when(mTabToTrack).isIncognito();
        doReturn(false).when(mGurl1).isEmpty();
        doReturn(true).when(mGurl1).isValid();
        long lastVisitedTimestampMs = 0;
        doReturn(lastVisitedTimestampMs).when(mCriticalPersistedTabData).getTimestampMillis();
        int expirationTimeSeconds = 1;
        ShadowChromeFeatureList.sParamValues.put(
                SearchResumptionModuleUtils.TAB_EXPIRATION_TIME_PARAM, expirationTimeSeconds);
        Assert.assertFalse(SearchResumptionModuleUtils.isTabToTrackValid(mTabToTrack));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        SearchResumptionModuleUtils.UMA_MODULE_NOT_SHOW,
                        ModuleNotShownReason.TAB_EXPIRED));

        doReturn(false).when(mTabToTrack).isNativePage();
        doReturn(false).when(mTabToTrack).isIncognito();
        doReturn(false).when(mGurl1).isEmpty();
        doReturn(true).when(mGurl1).isValid();
        expirationTimeSeconds = (int) (System.currentTimeMillis() / 1000) + 60; // one more minute
        ShadowChromeFeatureList.sParamValues.put(
                SearchResumptionModuleUtils.TAB_EXPIRATION_TIME_PARAM, expirationTimeSeconds);
        Assert.assertEquals(expirationTimeSeconds,
                ShadowChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID,
                        SearchResumptionModuleUtils.TAB_EXPIRATION_TIME_PARAM, 0));
        Assert.assertTrue(SearchResumptionModuleUtils.isTabToTrackValid(mTabToTrack));
    }

    @Test
    @SmallTest
    // clang-format off
    public void testMayGetCachedResults() {
        // clang-format on
        doReturn(false).when(mTab).canGoForward();
        Assert.assertNull(SearchResumptionModuleUtils.mayGetCachedResults(mTab, mTabToTrack));

        doReturn(true).when(mTab).canGoForward();
        SearchResumptionUserData searchResumptionUserData =
                Mockito.mock(SearchResumptionUserData.class);
        ShadowSearchResumptionUserData.setSearchResumptionUserData(searchResumptionUserData);
        doReturn(null).when(searchResumptionUserData).getCachedSuggestions(mTab);
        Assert.assertNull(SearchResumptionModuleUtils.mayGetCachedResults(mTab, mTabToTrack));

        SuggestionResult result = Mockito.mock(SuggestionResult.class);
        doReturn(result).when(searchResumptionUserData).getCachedSuggestions(mTab);
        String url1 = "foo.com";
        String url2 = "bar.bom";
        doReturn(url1).when(mGurl1).getSpec();
        doReturn(url2).when(mGurl2).getSpec();
        doReturn(mGurl1).when(result).getLastUrlToTrack();
        doReturn(mGurl2).when(mTabToTrack).getUrl();
        Assert.assertFalse(TextUtils.equals(
                result.getLastUrlToTrack().getSpec(), mTabToTrack.getUrl().getSpec()));
        Assert.assertNull(SearchResumptionModuleUtils.mayGetCachedResults(mTab, mTabToTrack));

        doReturn(mGurl1).when(mTabToTrack).getUrl();
        Assert.assertTrue(TextUtils.equals(
                result.getLastUrlToTrack().getSpec(), mTabToTrack.getUrl().getSpec()));
        Assert.assertEquals(
                result, SearchResumptionModuleUtils.mayGetCachedResults(mTab, mTabToTrack));

        ShadowSearchResumptionUserData.setSearchResumptionUserData(null);
    }
}
