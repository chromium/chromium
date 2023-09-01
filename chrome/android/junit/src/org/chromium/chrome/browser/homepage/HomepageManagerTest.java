// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import androidx.test.filters.SmallTest;

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
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
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

    @Mock
    private PartnerBrowserCustomizations mPartnerBrowserCustomizations;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowPartnerBrowserCustomizations.setPartnerBrowserCustomizations(
                mPartnerBrowserCustomizations);
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
}
