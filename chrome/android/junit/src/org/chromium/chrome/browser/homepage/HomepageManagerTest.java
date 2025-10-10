// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.common.ChromeUrlConstants;
import org.chromium.chrome.browser.new_tab_url.DseNewTabUrlManager;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link HomepageManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {HomepageManagerTest.ShadowPartnerBrowserCustomizations.class})
public class HomepageManagerTest {
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

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private PartnerBrowserCustomizations mPartnerBrowserCustomizations;

    @Before
    public void setUp() {
        ShadowPartnerBrowserCustomizations.setPartnerBrowserCustomizations(
                mPartnerBrowserCustomizations);
        DseNewTabUrlManager.resetIsEeaChoiceCountryForTesting();
    }

    @Test
    public void testIsHomepageNonNtp() {
        HomepageManager homepageManager = HomepageManager.getInstance();

        HomepagePolicyManager.setHomepageForTesting(true, GURL.emptyGURL(), false);
        Assert.assertFalse(
                "Empty string should fall back to NTP.", homepageManager.isHomepageNonNtp());

        HomepagePolicyManager.setHomepageForTesting(true, JUnitTestGURLs.EXAMPLE_URL, false);
        Assert.assertTrue("Random web page is not the NTP.", homepageManager.isHomepageNonNtp());

        HomepagePolicyManager.setHomepageForTesting(true, JUnitTestGURLs.NTP_NATIVE_URL, false);
        Assert.assertFalse("NTP should be considered the NTP.", homepageManager.isHomepageNonNtp());

        HomepagePolicyManager.setHomepageForTesting(true, JUnitTestGURLs.EXAMPLE_URL, true);
        Assert.assertFalse("NTP policy forces NTP.", homepageManager.isHomepageNonNtp());
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
}
