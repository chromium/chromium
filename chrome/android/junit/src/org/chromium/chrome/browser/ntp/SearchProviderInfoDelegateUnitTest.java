// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.search_engines.AiModeButtonUiConfig;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link SearchProviderInfoDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SearchProviderInfoDelegateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TemplateUrlService mTemplateUrlService;

    private SearchProviderInfoDelegate mDelegate;

    @Before
    public void setUp() {
        mDelegate = new SearchProviderInfoDelegate(mTemplateUrlService);
    }

    @Test
    public void testDefaultValues() {
        assertTrue(mDelegate.getSearchProviderHasLogo());
        assertFalse(mDelegate.getSearchProviderIsGoogle());
        assertFalse(mDelegate.getShowingNonStandardGoogleLogo());
    }

    @Test
    public void testSetSearchProviderInfo() {
        mDelegate.setShowingNonStandardGoogleLogo(/* showingNonStandardGoogleLogo= */ true);
        assertTrue(mDelegate.setSearchProviderInfo(/* hasLogo= */ false, /* isGoogle= */ true));
        assertFalse(mDelegate.getSearchProviderHasLogo());
        assertTrue(mDelegate.getSearchProviderIsGoogle());
        assertTrue(mDelegate.getShowingNonStandardGoogleLogo());

        // Calling with the same parameters returns false.
        assertFalse(mDelegate.setSearchProviderInfo(/* hasLogo= */ false, /* isGoogle= */ true));

        // Setting isGoogle to false resets mShowingNonStandardGoogleLogo to false.
        assertTrue(mDelegate.setSearchProviderInfo(/* hasLogo= */ true, /* isGoogle= */ false));
        assertTrue(mDelegate.getSearchProviderHasLogo());
        assertFalse(mDelegate.getSearchProviderIsGoogle());
        assertFalse(mDelegate.getShowingNonStandardGoogleLogo());
    }

    @Test
    public void testSettersAndGetters() {
        mDelegate.setSearchProviderHasLogo(/* hasLogo= */ false);
        assertFalse(mDelegate.getSearchProviderHasLogo());

        mDelegate.setSearchProviderHasLogo(/* hasLogo= */ true);
        assertTrue(mDelegate.getSearchProviderHasLogo());

        mDelegate.setSearchProviderIsGoogle(/* isGoogle= */ true);
        assertTrue(mDelegate.getSearchProviderIsGoogle());

        mDelegate.setSearchProviderIsGoogle(/* isGoogle= */ false);
        assertFalse(mDelegate.getSearchProviderIsGoogle());

        mDelegate.setShowingNonStandardGoogleLogo(/* showingNonStandardGoogleLogo= */ true);
        assertTrue(mDelegate.getShowingNonStandardGoogleLogo());

        mDelegate.setShowingNonStandardGoogleLogo(/* showingNonStandardGoogleLogo= */ false);
        assertFalse(mDelegate.getShowingNonStandardGoogleLogo());
    }

    @Test
    public void testGetComposeplateUrl() {
        GURL composeplateUrl = JUnitTestGURLs.URL_1;
        when(mTemplateUrlService.getComposeplateUrl()).thenReturn(composeplateUrl);
        assertEquals(composeplateUrl, mDelegate.getComposeplateUrl());

        when(mTemplateUrlService.getComposeplateUrl()).thenReturn(null);
        assertNull(mDelegate.getComposeplateUrl());
    }

    @Test
    public void testSetAiModeButtonUiConfig() {
        // By default the search provider doesn't offer an AI Mode entry point.
        assertNull(mDelegate.getAiModeButtonUiConfig());
        assertFalse(mDelegate.hasAiModeEntryPoint());

        AiModeButtonUiConfig config = createAiModeButtonUiConfig();
        assertTrue(mDelegate.setAiModeButtonUiConfig(config));
        assertEquals(config, mDelegate.getAiModeButtonUiConfig());
        assertTrue(mDelegate.hasAiModeEntryPoint());

        // Setting the same config returns false.
        assertFalse(mDelegate.setAiModeButtonUiConfig(config));

        // Switching to a search provider which offers a different entry point.
        AiModeButtonUiConfig newConfig = createAiModeButtonUiConfig();
        assertTrue(mDelegate.setAiModeButtonUiConfig(newConfig));
        assertEquals(newConfig, mDelegate.getAiModeButtonUiConfig());
        assertTrue(mDelegate.hasAiModeEntryPoint());

        // Switching to a search provider which doesn't offer an entry point.
        assertTrue(mDelegate.setAiModeButtonUiConfig(null));
        assertNull(mDelegate.getAiModeButtonUiConfig());
        assertFalse(mDelegate.hasAiModeEntryPoint());

        // Setting null again returns false.
        assertFalse(mDelegate.setAiModeButtonUiConfig(null));
    }

    @Test
    @EnableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testGetComposeplateUrl_Aim3pEntrypointEnabled() {
        testGetComposeplateUrlWithAiModeButtonUiConfigImpl(/* expectsUrlFromConfig= */ true);
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.AIM3P_ENTRYPOINT)
    public void testGetComposeplateUrl_Aim3pEntrypointDisabled() {
        testGetComposeplateUrlWithAiModeButtonUiConfigImpl(/* expectsUrlFromConfig= */ false);
    }

    private void testGetComposeplateUrlWithAiModeButtonUiConfigImpl(boolean expectsUrlFromConfig) {
        GURL templateUrlServiceUrl = JUnitTestGURLs.URL_1;
        when(mTemplateUrlService.getComposeplateUrl()).thenReturn(templateUrlServiceUrl);

        // A third party search provider carries its own AI Mode URL in the config.
        GURL thirdPartyUrl = JUnitTestGURLs.RED_1;
        mDelegate.setAiModeButtonUiConfig(
                createAiModeButtonUiConfig(/* navigationUrlEmpty= */ thirdPartyUrl));
        assertEquals(
                expectsUrlFromConfig ? thirdPartyUrl : templateUrlServiceUrl,
                mDelegate.getComposeplateUrl());

        // Google's config carries an empty URL, so the TemplateUrlService provides it instead.
        mDelegate.setAiModeButtonUiConfig(
                createAiModeButtonUiConfig(/* navigationUrlEmpty= */ GURL.emptyGURL()));
        assertEquals(templateUrlServiceUrl, mDelegate.getComposeplateUrl());
    }

    private static AiModeButtonUiConfig createAiModeButtonUiConfig() {
        return createAiModeButtonUiConfig(/* navigationUrlEmpty= */ JUnitTestGURLs.URL_2);
    }

    private static AiModeButtonUiConfig createAiModeButtonUiConfig(GURL navigationUrlEmpty) {
        return new AiModeButtonUiConfig(
                "AI Mode",
                "Ask AI Mode",
                "AI Mode button",
                "Always show AI Mode",
                "Ask AI Mode",
                /* faviconUrl= */ JUnitTestGURLs.RED_1,
                /* navigationUrl= */ "https://www.red.com/search?q={searchTerms}",
                navigationUrlEmpty);
    }
}
