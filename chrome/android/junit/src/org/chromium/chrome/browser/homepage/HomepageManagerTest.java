// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNtpUrl;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.actor.ui.ActorUiTabController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.new_tab_url.DseNewTabUrlManager;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.url_constants.ExtensionsUrlOverrideRegistry;
import org.chromium.chrome.browser.url_constants.UrlConstantResolverFactory;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Locale;

/** Unit tests for {@link HomepageManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING)
@DisableFeatures(ChromeFeatureList.GLIC)
public class HomepageManagerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private PartnerBrowserCustomizations mPartnerBrowserCustomizations;
    @Mock private ActorUiTabController mActorUiTabController;

    private static final Locale DEFAULT_LOCALE = Locale.getDefault();

    @Before
    public void setUp() {
        PartnerBrowserCustomizations.setInstanceForTesting(mPartnerBrowserCustomizations);
        DseNewTabUrlManager.resetIsEeaChoiceCountryForTesting();
        ExtensionsUrlOverrideRegistry.resetRegistry();
        UrlConstantResolverFactory.resetResolvers();
    }

    @After
    public void tearDown() {
        Locale.setDefault(DEFAULT_LOCALE);
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
    @DisableFeatures(ChromeFeatureList.DISABLE_PARTNER_HOMEPAGE_ANDROID)
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
                UrlConstantResolverFactory.getOriginalResolver().getNtpGurl(),
                homepageManager.getDefaultHomepageGurl(/* isIncognito= */ false));

        final GURL blueUrl = JUnitTestGURLs.BLUE_1;
        ChromeSharedPreferences.getInstance()
                .writeString(
                        ChromePreferenceKeys.DEPRECATED_HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_URI,
                        blueUrl.getSpec());
        ChromeSharedPreferences.getInstance()
                .writeString(ChromePreferenceKeys.HOMEPAGE_PARTNER_CUSTOMIZED_DEFAULT_GURL, null);
        Assert.assertEquals(
                blueUrl, homepageManager.getDefaultHomepageGurl(/* isIncognito= */ false));

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
        Assert.assertEquals(
                redUrl, homepageManager.getDefaultHomepageGurl(/* isIncognito= */ false));

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
        Assert.assertEquals(url2, homepageManager.getDefaultHomepageGurl(/* isIncognito= */ false));
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
    public void testGetHomepageGurl_NoOverride() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);

        GURL originalNtp = UrlConstantResolverFactory.getOriginalResolver().getNtpGurl();
        GURL incognitoNtp = UrlConstantResolverFactory.getIncognitoResolver().getNtpGurl();

        Assert.assertEquals(
                "Regular homepage should be the native NTP URL.",
                originalNtp,
                homepageManager.getHomepageGurl(false));
        Assert.assertEquals(
                "Incognito homepage should be the native NTP URL for incognito.",
                incognitoNtp,
                homepageManager.getHomepageGurl(true));
    }

    @Test
    public void testGetHomepageGurl_RegularNtpOverridden() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);

        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        UrlConstantResolverFactory.resetResolvers();

        GURL nonNativeNtp = new GURL(getOriginalNtpUrl());
        GURL incognitoNtp = UrlConstantResolverFactory.getIncognitoResolver().getNtpGurl();

        Assert.assertEquals(
                "Regular homepage should be the non-native NTP URL.",
                nonNativeNtp,
                homepageManager.getHomepageGurl(false));
        Assert.assertEquals(
                "Incognito homepage should be the native NTP URL for incognito.",
                incognitoNtp,
                homepageManager.getHomepageGurl(true));
    }

    @Test
    public void testGetHomepageGurl_IncognitoNtpOverridden() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);

        ExtensionsUrlOverrideRegistry.setIncognitoNtpOverrideEnabled(true);
        UrlConstantResolverFactory.resetResolvers();

        GURL originalNtp = UrlConstantResolverFactory.getOriginalResolver().getNtpGurl();
        GURL nonNativeNtp = new GURL(getOriginalNtpUrl());

        Assert.assertEquals(
                "Regular homepage should be the native NTP URL.",
                originalNtp,
                homepageManager.getHomepageGurl(false));
        Assert.assertEquals(
                "Incognito homepage should be the non-native NTP URL.",
                nonNativeNtp,
                homepageManager.getHomepageGurl(true));
    }

    @Test
    public void testGetHomepageGurl_BothNtpOverridden() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);

        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        ExtensionsUrlOverrideRegistry.setIncognitoNtpOverrideEnabled(true);
        UrlConstantResolverFactory.resetResolvers();

        GURL nonNativeNtp = new GURL(getOriginalNtpUrl());

        Assert.assertEquals(
                "Regular homepage should be the non-native NTP URL.",
                nonNativeNtp,
                homepageManager.getHomepageGurl(false));
        Assert.assertEquals(
                "Incognito homepage should be the non-native NTP URL.",
                nonNativeNtp,
                homepageManager.getHomepageGurl(true));
    }

    @Test
    public void testGetNtpUrl_ExtensionOverride() {
        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        UrlConstantResolverFactory.resetResolvers();

        GURL nonNativeNtp = new GURL(getOriginalNtpUrl());
        Assert.assertEquals(
                "getNtpUrl should return non-native NTP when overridden.",
                nonNativeNtp,
                UrlConstantResolverFactory.getOriginalResolver().getNtpGurl());

        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(false);
        UrlConstantResolverFactory.resetResolvers();

        GURL nativeNtp = UrlConstantResolverFactory.getOriginalResolver().getNtpGurl();
        Assert.assertNotEquals(
                "getNtpUrl should return native NTP when not overridden.", nonNativeNtp, nativeNtp);
    }

    @Test
    public void testShouldCloseAppWithZeroTabs_NtpOverridden() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);

        Assert.assertFalse(
                "Should not close with zero tabs if homepage is NTP.",
                homepageManager.shouldCloseAppWithZeroTabs());

        // Override NTP and check that the behavior is unchanged.
        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        UrlConstantResolverFactory.resetResolvers();

        Assert.assertFalse(
                "Should not close with zero tabs if homepage is overridden NTP.",
                homepageManager.shouldCloseAppWithZeroTabs());
    }

    @Test
    public void testIsHomepageNonNtp_NtpOverridden() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);

        Assert.assertFalse(
                "Homepage should be considered NTP.", homepageManager.isHomepageNonNtp());

        // Override NTP and check that the behavior is unchanged.
        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        UrlConstantResolverFactory.resetResolvers();

        Assert.assertFalse(
                "Overridden NTP should still be considered NTP.",
                homepageManager.isHomepageNonNtp());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING)
    public void testGetHomepageGurl_RegularNtpOverridden_OverridingDisabled() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);

        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        UrlConstantResolverFactory.resetResolvers();

        GURL originalNtp = UrlConstantResolverFactory.getOriginalResolver().getNtpGurl();
        GURL incognitoNtp = UrlConstantResolverFactory.getIncognitoResolver().getNtpGurl();

        Assert.assertEquals(
                "Regular homepage should be the native NTP URL.",
                originalNtp,
                homepageManager.getHomepageGurl(false));
        Assert.assertEquals(
                "Incognito homepage should be the native NTP URL for incognito.",
                incognitoNtp,
                homepageManager.getHomepageGurl(true));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING)
    public void testGetHomepageGurl_IncognitoNtpOverridden_OverridingDisabled() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);

        ExtensionsUrlOverrideRegistry.setIncognitoNtpOverrideEnabled(true);
        UrlConstantResolverFactory.resetResolvers();

        GURL originalNtp = UrlConstantResolverFactory.getOriginalResolver().getNtpGurl();
        GURL incognitoNtp = UrlConstantResolverFactory.getIncognitoResolver().getNtpGurl();

        Assert.assertEquals(
                "Regular homepage should be the native NTP URL.",
                originalNtp,
                homepageManager.getHomepageGurl(false));
        Assert.assertEquals(
                "Incognito homepage should be the native NTP URL for incognito.",
                incognitoNtp,
                homepageManager.getHomepageGurl(true));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING)
    public void testGetHomepageGurl_BothNtpOverridden_OverridingDisabled() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);

        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        ExtensionsUrlOverrideRegistry.setIncognitoNtpOverrideEnabled(true);
        UrlConstantResolverFactory.resetResolvers();

        GURL originalNtp = UrlConstantResolverFactory.getOriginalResolver().getNtpGurl();
        GURL incognitoNtp = UrlConstantResolverFactory.getIncognitoResolver().getNtpGurl();

        Assert.assertEquals(
                "Regular homepage should be the native NTP URL.",
                originalNtp,
                homepageManager.getHomepageGurl(false));
        Assert.assertEquals(
                "Incognito homepage should be the native NTP URL for incognito.",
                incognitoNtp,
                homepageManager.getHomepageGurl(true));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING)
    public void testGetNtpUrl_ExtensionOverride_OverridingDisabled() {
        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        UrlConstantResolverFactory.resetResolvers();

        GURL nonNativeNtp = new GURL(getOriginalNtpUrl());
        GURL nativeNtp = UrlConstantResolverFactory.getOriginalResolver().getNtpGurl();
        Assert.assertNotEquals(
                "getNtpUrl should return native NTP when override is disabled by feature.",
                nonNativeNtp,
                nativeNtp);

        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(false);
        UrlConstantResolverFactory.resetResolvers();

        GURL nativeNtpAfterReset = UrlConstantResolverFactory.getOriginalResolver().getNtpGurl();
        Assert.assertEquals(
                "getNtpUrl should still return native NTP when not overridden.",
                nativeNtp,
                nativeNtpAfterReset);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING)
    public void testShouldCloseAppWithZeroTabs_NtpOverridden_OverridingDisabled() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);

        Assert.assertFalse(
                "Should not close with zero tabs if homepage is NTP.",
                homepageManager.shouldCloseAppWithZeroTabs());

        // Override NTP and check that the behavior is unchanged.
        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        UrlConstantResolverFactory.resetResolvers();

        Assert.assertFalse(
                "Should not close with zero tabs if homepage is NTP, even with override"
                        + " attempt.",
                homepageManager.shouldCloseAppWithZeroTabs());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CHROME_NATIVE_URL_OVERRIDING)
    public void testIsHomepageNonNtp_NtpOverridden_OverridingDisabled() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_USE_CHROME_NTP, true);

        Assert.assertFalse(
                "Homepage should be considered NTP.", homepageManager.isHomepageNonNtp());

        // Override NTP and check that the behavior is unchanged.
        ExtensionsUrlOverrideRegistry.setNtpOverrideEnabled(true);
        UrlConstantResolverFactory.resetResolvers();

        Assert.assertFalse(
                "Homepage should still be considered NTP when override is disabled.",
                homepageManager.isHomepageNonNtp());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DISABLE_PARTNER_HOMEPAGE_ANDROID)
    public void testGetDefaultHomepageGurl_DisablePartnerHomepageAndroid() {
        HomepageManager homepageManager = HomepageManager.getInstance();

        when(mPartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled())
                .thenReturn(true);
        when(mPartnerBrowserCustomizations.getHomePageUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        Assert.assertEquals(
                UrlConstantResolverFactory.getOriginalResolver().getNtpGurl(),
                homepageManager.getDefaultHomepageGurl(/* isIncognito= */ false));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.DISABLE_PARTNER_HOMEPAGE_ANDROID)
    public void testGetDefaultHomepageGurl_PartnerHomepageEnabled() {
        HomepageManager homepageManager = HomepageManager.getInstance();

        when(mPartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabled())
                .thenReturn(true);
        when(mPartnerBrowserCustomizations.getHomePageUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        Assert.assertEquals(
                JUnitTestGURLs.EXAMPLE_URL,
                homepageManager.getDefaultHomepageGurl(/* isIncognito= */ false));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.DISABLE_PARTNER_HOMEPAGE_ANDROID
                + ":disable_partner_homepage_android_for_zero_tabs/true"
    })
    public void testGetHomepageGurlForZeroTabs_DisablePartnerHomepage() {
        HomepageManager homepageManager = HomepageManager.getInstance();

        when(mPartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabledForZeroTabs())
                .thenReturn(false);
        when(mPartnerBrowserCustomizations.getHomePageUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        Assert.assertEquals(
                UrlConstantResolverFactory.getOriginalResolver().getNtpGurl(),
                homepageManager.getHomepageGurlForZeroTabs(/* isIncognito= */ false));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.DISABLE_PARTNER_HOMEPAGE_ANDROID
                + ":disable_partner_homepage_android_for_zero_tabs/false"
    })
    public void testGetHomepageGurlForZeroTabs_PartnerHomepageEnabled() {
        HomepageManager homepageManager = HomepageManager.getInstance();

        when(mPartnerBrowserCustomizations.isHomepageProviderAvailableAndEnabledForZeroTabs())
                .thenReturn(true);
        when(mPartnerBrowserCustomizations.getHomePageUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        Assert.assertEquals(
                JUnitTestGURLs.EXAMPLE_URL,
                homepageManager.getHomepageGurlForZeroTabs(/* isIncognito= */ false));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.HOME_BUTTON_REMOVAL + ":remove_home_button_everywhere/true")
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testIsHomepageEnabled_HomeButtonRemovalEverywhere_NoBottomBar() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        Assert.assertFalse(homepageManager.isHomepageEnabled());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.HOME_BUTTON_REMOVAL + ":remove_home_button_everywhere/true",
        ChromeFeatureList.ANDROID_BOTTOM_BAR
    })
    public void testIsHomepageEnabled_HomeButtonRemovalEverywhere_WithBottomBar() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        Assert.assertTrue(homepageManager.isHomepageEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.HOME_BUTTON_REMOVAL + ":remove_home_button_everywhere/true")
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldShowHomepageSettings_HomeButtonRemovalEverywhere_NoBottomBar() {
        Assert.assertFalse(HomepageManager.shouldShowHomepageSettings());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.HOME_BUTTON_REMOVAL + ":remove_home_button_everywhere/true",
        ChromeFeatureList.ANDROID_BOTTOM_BAR
    })
    public void testShouldShowHomepageSettings_HomeButtonRemovalEverywhere_WithBottomBar() {
        Assert.assertTrue(HomepageManager.shouldShowHomepageSettings());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.HOME_BUTTON_REMOVAL + ":remove_home_button_everywhere/false")
    public void testShouldShowHomepageSettings_HomeButtonRemovalEverywhereDisabled() {
        Assert.assertTrue(HomepageManager.shouldShowHomepageSettings());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.HOME_BUTTON_REMOVAL + ":keep_home_button_on_ntp/true"})
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldShowHomeButtonOnToolbar_KeepOnNtp_NoBottomBar() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        Assert.assertTrue(homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ true));
        Assert.assertFalse(homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ false));
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.HOME_BUTTON_REMOVAL + ":keep_home_button_on_ntp/true",
        ChromeFeatureList.ANDROID_BOTTOM_BAR
    })
    public void testShouldShowHomeButtonOnToolbar_KeepOnNtp_WithBottomBar() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        Assert.assertTrue(homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ true));
        Assert.assertTrue(homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ false));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.HOME_BUTTON_REMOVAL + ":keep_home_button_on_ntp/false"})
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldShowHomeButtonOnToolbar_NotKeepOnNtp() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        Assert.assertTrue(homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ true));
        Assert.assertTrue(homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ false));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.HOME_BUTTON_REMOVAL + ":keep_home_button_on_ntp/true"})
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldShowHomeButtonOnToolbar_KeepOnNtp_IsNtp_HomepageDisabled() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, false);
        Assert.assertFalse(homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ true));
        Assert.assertFalse(homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ false));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.HOME_BUTTON_REMOVAL + ":keep_home_button_on_ntp/true"})
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldShowHomepageMenuItem_KeepOnNtp_NoBottomBar() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        Assert.assertTrue(homepageManager.shouldShowHomepageMenuItem());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.HOME_BUTTON_REMOVAL + ":keep_home_button_on_ntp/true",
        ChromeFeatureList.ANDROID_BOTTOM_BAR
    })
    public void testShouldShowHomepageMenuItem_KeepOnNtp_WithBottomBar() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        Assert.assertFalse(homepageManager.shouldShowHomepageMenuItem());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.HOME_BUTTON_REMOVAL + ":keep_home_button_on_ntp/false"})
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldShowHomepageMenuItem_NotKeepOnNtp() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);
        Assert.assertFalse(homepageManager.shouldShowHomepageMenuItem());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.HOME_BUTTON_REMOVAL + ":keep_home_button_on_ntp/true"})
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_BAR)
    public void testShouldShowHomepageMenuItem_KeepOnNtp_HomepageDisabled() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, false);
        Assert.assertFalse(homepageManager.shouldShowHomepageMenuItem());
    }

    @Test
    public void testOpenHomepage_WithActiveTab() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        HomepagePolicyManager.setHomepageForTesting(true, JUnitTestGURLs.EXAMPLE_URL, false);

        Tab tab = Mockito.mock(Tab.class);
        TabCreatorManager tabCreatorManager = Mockito.mock(TabCreatorManager.class);

        homepageManager.openHomepage(tab, tabCreatorManager, /* isIncognito= */ false);

        verify(tab)
                .loadUrl(
                        ArgumentMatchers.argThat(
                                params ->
                                        params.getUrl().equals(JUnitTestGURLs.EXAMPLE_URL.getSpec())
                                                && params.getTransitionType()
                                                        == PageTransition.HOME_PAGE));
        Mockito.verifyNoInteractions(tabCreatorManager);
    }

    @Test
    public void testOpenHomepage_WithNoActiveTab() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        HomepagePolicyManager.setHomepageForTesting(true, JUnitTestGURLs.EXAMPLE_URL, false);

        TabCreatorManager tabCreatorManager = Mockito.mock(TabCreatorManager.class);
        TabCreator tabCreator = Mockito.mock(TabCreator.class);
        when(tabCreatorManager.getTabCreator(false)).thenReturn(tabCreator);

        homepageManager.openHomepage(null, tabCreatorManager, /* isIncognito= */ false);

        verify(tabCreator)
                .createNewTab(
                        ArgumentMatchers.argThat(
                                params ->
                                        params.getUrl().equals(JUnitTestGURLs.EXAMPLE_URL.getSpec())
                                                && params.getTransitionType()
                                                        == PageTransition.HOME_PAGE),
                        ArgumentMatchers.eq(TabLaunchType.FROM_CHROME_UI),
                        ArgumentMatchers.isNull());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC)
    public void testOpenHomepage_WithActiveTab_GlicEnabled_ActorActive_ConfirmDialogShown() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        HomepagePolicyManager.setHomepageForTesting(true, JUnitTestGURLs.EXAMPLE_URL, false);

        Tab tab = Mockito.mock(Tab.class);
        UserDataHost userDataHost = new UserDataHost();
        Mockito.doReturn(userDataHost).when(tab).getUserDataHost();
        userDataHost.setUserData(ActorUiTabController.class, mActorUiTabController);

        Mockito.doReturn(true).when(mActorUiTabController).isActorActive();
        Mockito.doReturn(true)
                .when(mActorUiTabController)
                .showTaskAbortConfirmationDialog(ArgumentMatchers.any());

        TabCreatorManager tabCreatorManager = Mockito.mock(TabCreatorManager.class);

        homepageManager.openHomepage(tab, tabCreatorManager, /* isIncognito= */ false);

        // Immediate loadUrl should not be called as the confirmation dialog was shown.
        Mockito.verify(tab, Mockito.never()).loadUrl(ArgumentMatchers.any());
        Mockito.verifyNoInteractions(tabCreatorManager);

        @SuppressWarnings("unchecked")
        ArgumentCaptor<Callback<Boolean>> callbackCaptor = ArgumentCaptor.forClass(Callback.class);
        Mockito.verify(mActorUiTabController)
                .showTaskAbortConfirmationDialog(callbackCaptor.capture());

        // Now run the navigation callback, which should load the page on the tab.
        callbackCaptor.getValue().onResult(true);

        Mockito.verify(tab)
                .loadUrl(
                        ArgumentMatchers.argThat(
                                params ->
                                        params.getUrl().equals(JUnitTestGURLs.EXAMPLE_URL.getSpec())
                                                && params.getTransitionType()
                                                        == PageTransition.HOME_PAGE));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC)
    public void testOpenHomepage_WithActiveTab_GlicEnabled_ActorActive_ConfirmDialogNotShown() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        HomepagePolicyManager.setHomepageForTesting(true, JUnitTestGURLs.EXAMPLE_URL, false);

        Tab tab = Mockito.mock(Tab.class);
        UserDataHost userDataHost = new UserDataHost();
        Mockito.doReturn(userDataHost).when(tab).getUserDataHost();
        userDataHost.setUserData(ActorUiTabController.class, mActorUiTabController);

        Mockito.doReturn(true).when(mActorUiTabController).isActorActive();
        Mockito.doReturn(false)
                .when(mActorUiTabController)
                .showTaskAbortConfirmationDialog(ArgumentMatchers.any());

        TabCreatorManager tabCreatorManager = Mockito.mock(TabCreatorManager.class);

        homepageManager.openHomepage(tab, tabCreatorManager, /* isIncognito= */ false);

        // Immediate loadUrl should be called.
        Mockito.verify(tab)
                .loadUrl(
                        ArgumentMatchers.argThat(
                                params ->
                                        params.getUrl().equals(JUnitTestGURLs.EXAMPLE_URL.getSpec())
                                                && params.getTransitionType()
                                                        == PageTransition.HOME_PAGE));
        Mockito.verifyNoInteractions(tabCreatorManager);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC)
    public void testOpenHomepage_WithActiveTab_GlicEnabled_ActorNotActive() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        HomepagePolicyManager.setHomepageForTesting(true, JUnitTestGURLs.EXAMPLE_URL, false);

        Tab tab = Mockito.mock(Tab.class);
        UserDataHost userDataHost = new UserDataHost();
        Mockito.doReturn(userDataHost).when(tab).getUserDataHost();
        userDataHost.setUserData(ActorUiTabController.class, mActorUiTabController);

        Mockito.doReturn(false).when(mActorUiTabController).isActorActive();

        TabCreatorManager tabCreatorManager = Mockito.mock(TabCreatorManager.class);

        homepageManager.openHomepage(tab, tabCreatorManager, /* isIncognito= */ false);

        // Immediate loadUrl should be called.
        Mockito.verify(tab)
                .loadUrl(
                        ArgumentMatchers.argThat(
                                params ->
                                        params.getUrl().equals(JUnitTestGURLs.EXAMPLE_URL.getSpec())
                                                && params.getTransitionType()
                                                        == PageTransition.HOME_PAGE));
        Mockito.verifyNoInteractions(tabCreatorManager);
    }

    @Test
    @EnableFeatures({ChromeFeatureList.HOME_BUTTON_REMOVAL + ":remove_home_button_everywhere/true"})
    public void testIsHomepageEnabled_HomeButtonRemovalEverywhere() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);

        Locale.setDefault(Locale.US);
        Assert.assertFalse(
                "Homepage should be disabled in US geo under removal.",
                homepageManager.isHomepageEnabled());

        Locale.setDefault(Locale.CANADA);
        Assert.assertTrue(
                "Homepage should be enabled in non-US geo when restricted.",
                homepageManager.isHomepageEnabled());

        FeatureOverrides.newBuilder()
                .param(ChromeFeatureList.HOME_BUTTON_REMOVAL, "apply_to_all_countries", true)
                .apply();
        Assert.assertFalse(
                "Homepage should be disabled with everywhere removal enabled and"
                        + " apply_to_all_countries set to true in non-US geo.",
                homepageManager.isHomepageEnabled());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.HOME_BUTTON_REMOVAL + ":remove_home_button_everywhere/true"})
    public void testShouldShowHomepageSettings_HomeButtonRemovalEverywhere() {
        Locale.setDefault(Locale.US);
        Assert.assertFalse(
                "Homepage settings should be hidden in US geo under removal.",
                HomepageManager.shouldShowHomepageSettings());

        Locale.setDefault(Locale.CANADA);
        Assert.assertTrue(
                "Homepage settings should still be visible in non-US geo when restricted.",
                HomepageManager.shouldShowHomepageSettings());

        FeatureOverrides.newBuilder()
                .param(ChromeFeatureList.HOME_BUTTON_REMOVAL, "apply_to_all_countries", true)
                .apply();
        Assert.assertFalse(
                "Homepage settings should be hidden with everywhere removal enabled and"
                        + " apply_to_all_countries set to true in non-US geo.",
                HomepageManager.shouldShowHomepageSettings());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.HOME_BUTTON_REMOVAL + ":keep_home_button_on_ntp/true"})
    public void testShouldShowHomeButtonOnToolbar_KeepOnNtp() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);

        Locale.setDefault(Locale.US);
        Assert.assertTrue(
                "Home button should be shown on NTP in US geo.",
                homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ true));
        Assert.assertFalse(
                "Home button should be hidden on non-NTP in US geo.",
                homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ false));

        Locale.setDefault(Locale.CANADA);
        Assert.assertTrue(
                "Home button should be shown on NTP in non-US geo when restricted.",
                homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ true));
        Assert.assertTrue(
                "Home button should still be shown on non-NTP in non-US geo when restricted.",
                homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ false));

        FeatureOverrides.newBuilder()
                .param(ChromeFeatureList.HOME_BUTTON_REMOVAL, "apply_to_all_countries", true)
                .apply();
        Assert.assertTrue(
                "Home button should be shown on NTP with keep_on_ntp and apply_to_all_countries set"
                        + " to true in non-US geo.",
                homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ true));
        Assert.assertFalse(
                "Home button should be hidden on non-NTP with keep_on_ntp and"
                        + " apply_to_all_countries set to true in non-US geo.",
                homepageManager.shouldShowHomeButtonOnToolbar(/* isNtp= */ false));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.HOME_BUTTON_REMOVAL + ":keep_home_button_on_ntp/true"})
    public void testShouldShowHomepageMenuItem_KeepOnNtp() {
        HomepageManager homepageManager = HomepageManager.getInstance();
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HOMEPAGE_ENABLED, true);

        Locale.setDefault(Locale.US);
        Assert.assertTrue(
                "Homepage menu item should be shown in US geo.",
                homepageManager.shouldShowHomepageMenuItem());

        Locale.setDefault(Locale.CANADA);
        Assert.assertFalse(
                "Homepage menu item should not be shown in non-US geo when restricted.",
                homepageManager.shouldShowHomepageMenuItem());

        FeatureOverrides.newBuilder()
                .param(ChromeFeatureList.HOME_BUTTON_REMOVAL, "apply_to_all_countries", true)
                .apply();
        Assert.assertTrue(
                "Homepage menu item should be shown with keep_on_ntp and apply_to_all_countries set"
                        + " to true in non-US geo.",
                homepageManager.shouldShowHomepageMenuItem());
    }
}
