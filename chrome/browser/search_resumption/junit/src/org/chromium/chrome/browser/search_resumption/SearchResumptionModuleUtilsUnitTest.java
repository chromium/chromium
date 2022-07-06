// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for {@link SearchResumptionModuleUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {SearchResumptionModuleUtilsUnitTest.ShadowCriticalPersistedTabData.class,
                SearchResumptionModuleUtilsUnitTest.ShadowChromeFeatureList.class})
@SuppressWarnings("DoNotMock") // Mocks GURL
public class SearchResumptionModuleUtilsUnitTest {
    /** Shadow for {@link CriticalPersistedTabData}. */
    @Implements(CriticalPersistedTabData.class)
    static class ShadowCriticalPersistedTabData {
        private static CriticalPersistedTabData sCriticalPersistedTabData;

        static void setCriticalPersistedTabData(CriticalPersistedTabData criticalPersistedTabData) {
            sCriticalPersistedTabData = criticalPersistedTabData;
        }

        @Resetter
        static void reset() {
            sCriticalPersistedTabData = null;
        }

        @Implementation
        public static CriticalPersistedTabData from(Tab tab) {
            return sCriticalPersistedTabData;
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
    private IdentityManager mIdentityManager;
    @Mock
    private Profile mProfile;
    @Mock
    private Tab mTab;
    @Mock
    private Tab mTabToTrack;
    @Mock
    private GURL mURL;
    @Mock
    private CriticalPersistedTabData mCriticalPersistedTabData;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        doReturn(mIdentityManager).when(mIdentityServicesProvider).getIdentityManager(any());

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
        Assert.assertFalse(
                SearchResumptionModuleUtils.shouldShowSearchResumptionModule(mProfile, mTab));

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(false).when(mIdentityManager).hasPrimaryAccount(anyInt());
        Assert.assertFalse(
                SearchResumptionModuleUtils.shouldShowSearchResumptionModule(mProfile, mTab));

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(anyInt());
        doReturn(true).when(mTab).canGoForward();
        Assert.assertFalse(
                SearchResumptionModuleUtils.shouldShowSearchResumptionModule(mProfile, mTab));

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(true).when(mIdentityManager).hasPrimaryAccount(anyInt());
        doReturn(false).when(mTab).canGoForward();
        Assert.assertTrue(
                SearchResumptionModuleUtils.shouldShowSearchResumptionModule(mProfile, mTab));
    }

    @Test
    @SmallTest
    public void testIsTabToTrackValid() {
        doReturn(true).when(mTabToTrack).isNativePage();
        Assert.assertFalse(SearchResumptionModuleUtils.isTabToTrackValid(mTabToTrack));

        doReturn(false).when(mTabToTrack).isNativePage();
        doReturn(true).when(mTabToTrack).isIncognito();
        Assert.assertFalse(SearchResumptionModuleUtils.isTabToTrackValid(mTabToTrack));

        doReturn(mURL).when(mTabToTrack).getUrl();
        doReturn(false).when(mTabToTrack).isNativePage();
        doReturn(false).when(mTabToTrack).isIncognito();
        doReturn(true).when(mURL).isEmpty();
        Assert.assertFalse(SearchResumptionModuleUtils.isTabToTrackValid(mTabToTrack));

        doReturn(false).when(mTabToTrack).isNativePage();
        doReturn(false).when(mTabToTrack).isIncognito();
        doReturn(false).when(mURL).isEmpty();
        doReturn(false).when(mURL).isValid();
        Assert.assertFalse(SearchResumptionModuleUtils.isTabToTrackValid(mTabToTrack));

        doReturn(false).when(mTabToTrack).isNativePage();
        doReturn(false).when(mTabToTrack).isIncognito();
        doReturn(false).when(mURL).isEmpty();
        doReturn(true).when(mURL).isValid();
        ShadowCriticalPersistedTabData.setCriticalPersistedTabData(mCriticalPersistedTabData);
        long lastVisitedTimestampMs = 0;
        doReturn(lastVisitedTimestampMs).when(mCriticalPersistedTabData).getTimestampMillis();
        int expirationTimeSeconds = 1;
        ShadowChromeFeatureList.sParamValues.put(
                SearchResumptionModuleUtils.TAB_EXPIRATION_TIME_PARAM, expirationTimeSeconds);
        Assert.assertFalse(SearchResumptionModuleUtils.isTabToTrackValid(mTabToTrack));

        doReturn(false).when(mTabToTrack).isNativePage();
        doReturn(false).when(mTabToTrack).isIncognito();
        doReturn(false).when(mURL).isEmpty();
        doReturn(true).when(mURL).isValid();
        expirationTimeSeconds = (int) (System.currentTimeMillis() / 1000) + 60; // one more minute
        ShadowChromeFeatureList.sParamValues.put(
                SearchResumptionModuleUtils.TAB_EXPIRATION_TIME_PARAM, expirationTimeSeconds);
        Assert.assertEquals(expirationTimeSeconds,
                ShadowChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                        ChromeFeatureList.SEARCH_RESUMPTION_MODULE_ANDROID,
                        SearchResumptionModuleUtils.TAB_EXPIRATION_TIME_PARAM, 0));
        Assert.assertTrue(SearchResumptionModuleUtils.isTabToTrackValid(mTabToTrack));

        ShadowCriticalPersistedTabData.reset();
    }
}
