// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import static org.mockito.Mockito.doReturn;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.new_tab_url.DseNewTabUrlManager;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link HomepageManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {HomepageManagerTest.ShadowHomepagePolicyManager.class,
                HomepageManagerTest.ShadowUrlUtilities.class,
                HomepageManagerTest.ShadowPartnerBrowserCustomizations.class})
public class HomepageManagerTest {
    /** Shadow for {@link HomepagePolicyManager}. */
    @Implements(HomepagePolicyManager.class)
    public static class ShadowHomepagePolicyManager {
        static GURL sHomepageUrl;

        @Implementation
        public static boolean isHomepageManagedByPolicy() {
            return true;
        }

        @Implementation
        public static GURL getHomepageUrl() {
            return sHomepageUrl;
        }
    }

    @Implements(UrlUtilities.class)
    static class ShadowUrlUtilities {
        @Implementation
        public static boolean isNTPUrl(String url) {
            return JUnitTestGURLs.NTP_NATIVE_URL.getSpec().equals(url);
        }
    }

    @Implements(PartnerBrowserCustomizations.class)
    static class ShadowPartnerBrowserCustomizations {
        private static PartnerBrowserCustomizations sPartnerBrowserCustomizations;

        @Implementation
        public static PartnerBrowserCustomizations getInstance() {
            return sPartnerBrowserCustomizations;
        }

        static void setPartnerBrowserCustomizations(
                PartnerBrowserCustomizations partnerBrowserCustomizations) {
            sPartnerBrowserCustomizations = partnerBrowserCustomizations;
        }
    }
    @Rule
    public Features.JUnitProcessor mFeaturesProcessor = new Features.JUnitProcessor();
    @Mock
    private PartnerBrowserCustomizations mPartnerBrowserCustomizations;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowPartnerBrowserCustomizations.setPartnerBrowserCustomizations(
                mPartnerBrowserCustomizations);
    }

    @After
    public void tearDown() {
        ShadowHomepagePolicyManager.sHomepageUrl = null;
    }

    @Test
    @SmallTest
    public void testIsHomepageNonNtp() {
        ShadowHomepagePolicyManager.sHomepageUrl = GURL.emptyGURL();
        Assert.assertFalse(
                "Empty string should fall back to NTP.", HomepageManager.isHomepageNonNtp());

        ShadowHomepagePolicyManager.sHomepageUrl = JUnitTestGURLs.EXAMPLE_URL;
        Assert.assertTrue("Random web page is not the NTP.", HomepageManager.isHomepageNonNtp());

        ShadowHomepagePolicyManager.sHomepageUrl = JUnitTestGURLs.NTP_NATIVE_URL;
        Assert.assertFalse("NTP should be considered the NTP.", HomepageManager.isHomepageNonNtp());
    }

    @Test
    @SmallTest
    public void testGetDefaultHomepageUri() {
        Mockito.doNothing()
                .when(mPartnerBrowserCustomizations)
                .setPartnerHomepageListener(ArgumentMatchers.any());
        Mockito.doReturn(false)
                .when(mPartnerBrowserCustomizations)
                .isHomepageProviderAvailableAndEnabled();

        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI, null);
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, null);
        Assert.assertEquals(UrlConstants.NTP_URL, HomepageManager.getDefaultHomepageUri());

        final String blueUrl = JUnitTestGURLs.BLUE_1.getSpec();
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI, blueUrl);
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, null);
        Assert.assertEquals(blueUrl, HomepageManager.getDefaultHomepageUri());

        final GURL redUrl = JUnitTestGURLs.RED_1;
        final String serializedRedGurl = redUrl.serialize();
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI, null);
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, serializedRedGurl);
        Assert.assertEquals(redUrl.getSpec(), HomepageManager.getDefaultHomepageUri());

        final GURL url1 = JUnitTestGURLs.URL_1;
        final GURL url2 = JUnitTestGURLs.URL_2;
        final String serializedGurl2 = url2.serialize();
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI, url1.getSpec());
        SharedPreferencesManager.getInstance().writeString(
                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, serializedGurl2);
        Assert.assertEquals(url2.getSpec(), HomepageManager.getDefaultHomepageUri());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.NEW_TAB_SEARCH_ENGINE_URL_ANDROID})
    public void testOverrideNtpHomepage() {
        ShadowHomepagePolicyManager.sHomepageUrl = GURL.emptyGURL();

        Assert.assertNull(DseNewTabUrlManager.getDSENewTabUrl(null));
        Assert.assertEquals(UrlConstants.NTP_URL, HomepageManager.getHomepageUri());

        TemplateUrlService templateUrlService = Mockito.mock(TemplateUrlService.class);
        initializeProfile(false, templateUrlService);

        HomepageManager.getHomepageUri();
        Assert.assertEquals(JUnitTestGURLs.SEARCH_URL.getSpec(),
                DseNewTabUrlManager.getDSENewTabUrl(templateUrlService));
        Assert.assertEquals(JUnitTestGURLs.SEARCH_URL.getSpec(), HomepageManager.getHomepageUri());

        ProfileManager.resetForTesting();
    }

    private void initializeProfile(boolean isOffTheRecord, TemplateUrlService templateUrlService) {
        Profile profile = Mockito.mock(Profile.class);
        doReturn(isOffTheRecord).when(profile).isOffTheRecord();

        TemplateUrl templateUrl = Mockito.mock(TemplateUrl.class);
        doReturn(templateUrl).when(templateUrlService).getDefaultSearchEngineTemplateUrl();
        doReturn(JUnitTestGURLs.SEARCH_URL.getSpec()).when(templateUrl).getNewTabURL();

        Profile.setLastUsedProfileForTesting(profile);
        TemplateUrlServiceFactory.setInstanceForTesting(templateUrlService);
        ProfileManager.onProfileAdded(profile);

        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.IS_DSE_GOOGLE, false);
        Assert.assertFalse(DseNewTabUrlManager.isDefaultSearchEngineGoogle());
    }
}
