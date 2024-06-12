// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import static org.mockito.Mockito.doReturn;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
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
import org.chromium.chrome.browser.common.ChromeUrlConstants;
import org.chromium.chrome.browser.new_tab_url.DseNewTabUrlManager;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link HomepageManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        shadows = {
            HomepageManagerTest.ShadowHomepagePolicyManager.class,
            HomepageManagerTest.ShadowPartnerBrowserCustomizations.class
        })
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

    @Mock private PartnerBrowserCustomizations mPartnerBrowserCustomizations;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowPartnerBrowserCustomizations.setPartnerBrowserCustomizations(
                mPartnerBrowserCustomizations);
        DseNewTabUrlManager.resetIsEeaChoiceCountryForTesting();
    }

    @After
    public void tearDown() {
        ShadowHomepagePolicyManager.sHomepageUrl = null;
    }

    @Test
    public void testIsHomepageNonNtp() {
        HomepageManager homepageManager = HomepageManager.getInstance();

        ShadowHomepagePolicyManager.sHomepageUrl = GURL.emptyGURL();
        Assert.assertFalse(
                "Empty string should fall back to NTP.", homepageManager.isHomepageNonNtp());

        ShadowHomepagePolicyManager.sHomepageUrl = JUnitTestGURLs.EXAMPLE_URL;
        Assert.assertTrue("Random web page is not the NTP.", homepageManager.isHomepageNonNtp());

        ShadowHomepagePolicyManager.sHomepageUrl = JUnitTestGURLs.NTP_NATIVE_URL;
        Assert.assertFalse("NTP should be considered the NTP.", homepageManager.isHomepageNonNtp());
    }

    @Test
    public void testGetDefaultHomepageGurlPreferenceKeysMigration() {
        HomepageManager homepageManager = HomepageManager.getInstance();

        Mockito.doNothing()
                .when(mPartnerBrowserCustomizations)
                .setPartnerHomepageListener(ArgumentMatchers.any());
        Mockito.doReturn(false)
                .when(mPartnerBrowserCustomizations)
                .isHomepageProviderAvailableAndEnabled();

        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.DEPRECATED_HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI,
                        null);
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, null);
        Assert.assertEquals(
                ChromeUrlConstants.nativeNtpGurl(), homepageManager.getDefaultHomepageGurl());

        final GURL blueUrl = JUnitTestGURLs.BLUE_1;
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.DEPRECATED_HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI,
                        blueUrl.getSpec());
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, null);
        Assert.assertEquals(blueUrl, homepageManager.getDefaultHomepageGurl());

        // getDefaultHomepageGurl() should have forced the usage of the new pref key.
        String deprecatedKeyValue =
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys
                                        .DEPRECATED_HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI,
                                "");
        String expectedKeyValue =
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, "");
        Assert.assertEquals(blueUrl, GURL.deserialize(expectedKeyValue));
        Assert.assertEquals("", deprecatedKeyValue);

        final GURL redUrl = JUnitTestGURLs.RED_1;
        final String serializedRedGurl = redUrl.serialize();
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.DEPRECATED_HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI,
                        null);
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL,
                        serializedRedGurl);
        Assert.assertEquals(redUrl, homepageManager.getDefaultHomepageGurl());

        final GURL url1 = JUnitTestGURLs.URL_1;
        final GURL url2 = JUnitTestGURLs.URL_2;
        final String serializedGurl2 = url2.serialize();
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.DEPRECATED_HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI,
                        url1.getSpec());
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL,
                        serializedGurl2);
        Assert.assertEquals(url2, homepageManager.getDefaultHomepageGurl());
    }

    @Test
    public void testGetPrefHomepageCustomGurlPreferenceKeysMigration() {
        HomepageManager homepageManager = HomepageManager.getInstance();

        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.DEPRECATED_HOMEPAGE_CUSTOM_URI, null);
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_GURL, null);
        Assert.assertTrue(homepageManager.getPrefHomepageCustomGurl().isEmpty());

        final GURL blueUrl = JUnitTestGURLs.BLUE_1;
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.DEPRECATED_HOMEPAGE_CUSTOM_URI, blueUrl.getSpec());
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_GURL, null);
        Assert.assertEquals(blueUrl, homepageManager.getPrefHomepageCustomGurl());

        // getPrefHomepageCustomGurl() should have forced the usage of the new pref key.
        String deprecatedKeyValue =
                ChromeSharedPreferences.getInstance()
                        .readString(ChromePreferenceKeys.DEPRECATED_HOMEPAGE_CUSTOM_URI, "");
        String expectedKeyValue =
                ChromeSharedPreferences.getInstance()
                        .readString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_GURL, "");
        Assert.assertEquals(blueUrl, GURL.deserialize(expectedKeyValue));
        Assert.assertEquals("", deprecatedKeyValue);

        final GURL redUrl = JUnitTestGURLs.RED_1;
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.DEPRECATED_HOMEPAGE_CUSTOM_URI, null);
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_GURL, redUrl.serialize());
        Assert.assertEquals(redUrl, homepageManager.getPrefHomepageCustomGurl());

        final GURL url1 = JUnitTestGURLs.URL_1;
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.DEPRECATED_HOMEPAGE_CUSTOM_URI, redUrl.serialize());
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.HOMEPAGE_CUSTOM_GURL, url1.serialize());
        Assert.assertEquals(url1, homepageManager.getPrefHomepageCustomGurl());
    }

    @Test
    public void testOverrideNtpHomepage() {
        HomepageManager homepageManager = HomepageManager.getInstance();

        DseNewTabUrlManager.setIsEeaChoiceCountryForTesting(true);
        ShadowHomepagePolicyManager.sHomepageUrl = GURL.emptyGURL();
        DseNewTabUrlManager.SWAP_OUT_NTP.setForTesting(true);
        Assert.assertTrue(DseNewTabUrlManager.SWAP_OUT_NTP.getValue());

        Assert.assertNull(DseNewTabUrlManager.getDSENewTabUrl(null));
        Assert.assertEquals(ChromeUrlConstants.nativeNtpGurl(), homepageManager.getHomepageGurl());

        TemplateUrlService templateUrlService = Mockito.mock(TemplateUrlService.class);
        initializeProfile(false, templateUrlService);

        Assert.assertEquals(
                JUnitTestGURLs.SEARCH_URL.getSpec(),
                DseNewTabUrlManager.getDSENewTabUrl(templateUrlService));
        Assert.assertEquals(JUnitTestGURLs.SEARCH_URL, homepageManager.getHomepageGurl());

        ProfileManager.resetForTesting();
    }

    private void initializeProfile(boolean isOffTheRecord, TemplateUrlService templateUrlService) {
        Profile profile = Mockito.mock(Profile.class);
        doReturn(isOffTheRecord).when(profile).isOffTheRecord();

        TemplateUrl templateUrl = Mockito.mock(TemplateUrl.class);
        doReturn(templateUrl).when(templateUrlService).getDefaultSearchEngineTemplateUrl();
        doReturn(JUnitTestGURLs.SEARCH_URL.getSpec()).when(templateUrl).getNewTabURL();

        ProfileManager.setLastUsedProfileForTesting(profile);
        TemplateUrlServiceFactory.setInstanceForTesting(templateUrlService);
        ProfileManager.onProfileAdded(profile);

        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.IS_DSE_GOOGLE, false);
        Assert.assertFalse(DseNewTabUrlManager.isDefaultSearchEngineGoogle());
    }
}
