// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.url.GURL;

/** Unit tests for {@link PartnerBrowserCustomizations}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class PartnerBrowserCustomizationsUnitTest {
    private static final String TEST_HOMEPAGE = "http://example.com/";

    private static class CustomizationProviderDelegateTestImpl
            implements CustomizationProviderDelegate {
        String mHomepage = TEST_HOMEPAGE;

        @Override
        public String getHomepage() {
            return mHomepage;
        }

        @Override
        public boolean isIncognitoModeDisabled() {
            return true;
        }

        @Override
        public boolean isBookmarksEditingDisabled() {
            return true;
        }

        public void setHomepage(String homepage) {
            mHomepage = homepage;
        }
    }

    @Before
    public void setUp() {
        PartnerBrowserCustomizations.destroy();
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @SmallTest
    @Test
    public void testRefreshHomepage() {
        PartnerBrowserCustomizations partnerBrowserCustomizations =
                PartnerBrowserCustomizations.getInstance();
        CustomizationProviderDelegateTestImpl delegate =
                new CustomizationProviderDelegateTestImpl();

        delegate.setHomepage(null);
        partnerBrowserCustomizations.refreshHomepage(delegate);
        String serializedGurl =
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, "");
        Assert.assertEquals("", GURL.deserialize(serializedGurl).getSpec());

        delegate.setHomepage(TEST_HOMEPAGE);
        partnerBrowserCustomizations.refreshHomepage(delegate);
        serializedGurl =
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, "");
        Assert.assertEquals(TEST_HOMEPAGE, GURL.deserialize(serializedGurl).getSpec());

        delegate.setHomepage("about://newtab");
        partnerBrowserCustomizations.refreshHomepage(delegate);
        serializedGurl =
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, "");
        Assert.assertEquals(
                UrlConstants.NTP_NON_NATIVE_URL, GURL.deserialize(serializedGurl).getSpec());

        delegate.setHomepage("about:newtab");
        partnerBrowserCustomizations.refreshHomepage(delegate);
        serializedGurl =
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, "");
        Assert.assertEquals(
                UrlConstants.NTP_NON_NATIVE_URL, GURL.deserialize(serializedGurl).getSpec());

        delegate.setHomepage("about:newtab/path#fragment");
        partnerBrowserCustomizations.refreshHomepage(delegate);
        serializedGurl =
                ChromeSharedPreferences.getInstance()
                        .readString(
                                ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, "");
        Assert.assertEquals(
                UrlConstants.NTP_NON_NATIVE_URL + "path#fragment",
                GURL.deserialize(serializedGurl).getSpec());
    }

    @SmallTest
    @Test
    public void testGetHomepageUrl() {
        PartnerBrowserCustomizations partnerBrowserCustomizations =
                PartnerBrowserCustomizations.getInstance();
        Assert.assertEquals(null, partnerBrowserCustomizations.getHomePageUrl());

        partnerBrowserCustomizations.refreshHomepage(new CustomizationProviderDelegateTestImpl());
        Assert.assertEquals(TEST_HOMEPAGE, partnerBrowserCustomizations.getHomePageUrl().getSpec());
    }

    @SmallTest
    @Test
    public void testIsIncognitoModeDisabled() {
        PartnerBrowserCustomizations partnerBrowserCustomizations =
                PartnerBrowserCustomizations.getInstance();
        Assert.assertEquals(false, partnerBrowserCustomizations.isIncognitoModeDisabled());

        partnerBrowserCustomizations.refreshIncognitoModeDisabled(
                new CustomizationProviderDelegateTestImpl());
        Assert.assertEquals(true, partnerBrowserCustomizations.isIncognitoModeDisabled());
    }

    @SmallTest
    @Test
    public void testIsBookmarksEditingDisabled() {
        PartnerBrowserCustomizations partnerBrowserCustomizations =
                PartnerBrowserCustomizations.getInstance();
        Assert.assertEquals(false, partnerBrowserCustomizations.isBookmarksEditingDisabled());

        partnerBrowserCustomizations.refreshBookmarksEditingDisabled(
                new CustomizationProviderDelegateTestImpl());
        Assert.assertEquals(true, partnerBrowserCustomizations.isBookmarksEditingDisabled());
    }

    @Feature({"Homepage"})
    @SmallTest
    @Test
    public void testIsValidHomepage() {
        Assert.assertTrue(
                PartnerBrowserCustomizations.isValidHomepage(
                        new GURL("chrome-native://newtab/path#fragment")));
        Assert.assertTrue(
                PartnerBrowserCustomizations.isValidHomepage(new GURL("chrome-native://newtab/")));
        Assert.assertTrue(
                PartnerBrowserCustomizations.isValidHomepage(new GURL("chrome-native://newtab")));
        Assert.assertTrue(
                PartnerBrowserCustomizations.isValidHomepage(new GURL("chrome://newtab")));
        Assert.assertTrue(PartnerBrowserCustomizations.isValidHomepage(new GURL("chrome:newtab")));
        Assert.assertTrue(
                PartnerBrowserCustomizations.isValidHomepage(new GURL("http://example.com")));
        Assert.assertTrue(
                PartnerBrowserCustomizations.isValidHomepage(new GURL("https:example.com")));

        // TODO(crbug.com/40063064): Enable this test after the feature is
        // shipped. See https://crrev.com/c/5595374 for details.
        // Assert.assertTrue(
        //         PartnerBrowserCustomizations.isValidHomepage(new GURL("about://newtab")));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage(new GURL("about:newtab")));
        Assert.assertFalse(
                PartnerBrowserCustomizations.isValidHomepage(
                        new GURL("about:newtab/path#fragment")));
        Assert.assertFalse(
                PartnerBrowserCustomizations.isValidHomepage(new GURL("chrome://newtab--not")));
        Assert.assertFalse(
                PartnerBrowserCustomizations.isValidHomepage(
                        UrlFormatter.fixupUrl("about:newtab--not")));
        Assert.assertFalse(
                PartnerBrowserCustomizations.isValidHomepage(new GURL("chrome://history")));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage(new GURL("chrome://")));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage(new GURL("chrome:")));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage(new GURL("chrome")));

        Assert.assertFalse(
                PartnerBrowserCustomizations.isValidHomepage(
                        new GURL("chrome-native://bookmarks")));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage(new GURL("example.com")));
        Assert.assertFalse(
                PartnerBrowserCustomizations.isValidHomepage(
                        new GURL(
                                "content://com.android.providers.media.documents/document/video:113")));
        Assert.assertFalse(
                PartnerBrowserCustomizations.isValidHomepage(new GURL("ftp://example.com")));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage(GURL.emptyGURL()));
        Assert.assertFalse(PartnerBrowserCustomizations.isValidHomepage(null));
    }
}
