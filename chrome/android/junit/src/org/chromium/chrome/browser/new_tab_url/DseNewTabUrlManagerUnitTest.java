// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.new_tab_url;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.regional_capabilities.RegionalCapabilitiesServiceFactory;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.components.regional_capabilities.RegionalCapabilitiesService;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link DseNewTabUrlManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DseNewTabUrlManagerUnitTest {
    private static final String SEARCH_URL = JUnitTestGURLs.SEARCH_URL.getSpec();
    private static final String NEW_TAB_URL = JUnitTestGURLs.NTP_URL.getSpec();
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    @Mock private RegionalCapabilitiesService mRegionalCapabilities;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private TemplateUrl mTemplateUrl;

    private SharedPreferencesManager mSharedPreferenceManager;
    private DseNewTabUrlManager mDseNewTabUrlManager;

    @Captor private ArgumentCaptor<TemplateUrlServiceObserver> mTemplateUrlServiceObserverCaptor;

    @Before
    public void setUp() {
        mSharedPreferenceManager = ChromeSharedPreferences.getInstance();

        doReturn(SEARCH_URL).when(mTemplateUrl).getURL();
        doReturn(NEW_TAB_URL).when(mTemplateUrl).getNewTabURL();
        doReturn(mTemplateUrl).when(mTemplateUrlService).getDefaultSearchEngineTemplateUrl();

        doReturn(false).when(mProfile).isOffTheRecord();
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        RegionalCapabilitiesServiceFactory.setInstanceForTesting(mRegionalCapabilities);

        mDseNewTabUrlManager = new DseNewTabUrlManager(mProfileSupplier);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.IS_EEA_CHOICE_COUNTRY);
    }

    @Test
    public void testIsNewTabSearchEngineUrlAndroidEnabled() {
        DseNewTabUrlManager.setIsEeaChoiceCountryForTesting(true);
        assertTrue(DseNewTabUrlManager.isNewTabSearchEngineUrlAndroidEnabled());
    }

    @Test
    public void testIsNewTabSearchEngineUrlAndroidEnabledForEeaCountry() {
        DseNewTabUrlManager.setIsEeaChoiceCountryForTesting(true);
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.IS_EEA_CHOICE_COUNTRY, false));

        assertTrue(DseNewTabUrlManager.isNewTabSearchEngineUrlAndroidEnabled());
    }

    @Test
    public void testGetDSENewTabUrl() {
        String newTabUrl = DseNewTabUrlManager.getDSENewTabUrl(null);
        assertNull(newTabUrl);

        mSharedPreferenceManager.writeString(ChromePreferenceKeys.DSE_NEW_TAB_URL, NEW_TAB_URL);
        assertEquals(NEW_TAB_URL, DseNewTabUrlManager.getDSENewTabUrl(null));

        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        assertNull(DseNewTabUrlManager.getDSENewTabUrl(mTemplateUrlService));

        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        assertEquals(NEW_TAB_URL, DseNewTabUrlManager.getDSENewTabUrl(mTemplateUrlService));

        doReturn(null).when(mTemplateUrl).getNewTabURL();
        assertEquals(SEARCH_URL, DseNewTabUrlManager.getDSENewTabUrl(mTemplateUrlService));
    }

    @Test
    public void testOnProfileAvailable() {
        assertNull(mDseNewTabUrlManager.getTemplateUrlServiceForTesting());
        assertFalse(mSharedPreferenceManager.contains(ChromePreferenceKeys.IS_DSE_GOOGLE));

        // Sets the DSE is Google.
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(true).when(mRegionalCapabilities).isInEeaCountry();
        mProfileSupplier.set(mProfile);

        // Verifies that the SharedPreference is updated once the TemplateUrlService is ready.
        assertEquals(mTemplateUrlService, mDseNewTabUrlManager.getTemplateUrlServiceForTesting());
        assertTrue(mSharedPreferenceManager.readBoolean(ChromePreferenceKeys.IS_DSE_GOOGLE, false));
        assertFalse(mSharedPreferenceManager.contains(ChromePreferenceKeys.DSE_NEW_TAB_URL));
        assertTrue(
                mSharedPreferenceManager.readBoolean(
                        ChromePreferenceKeys.IS_EEA_CHOICE_COUNTRY, false));
    }

    @Test
    public void testOnTemplateURLServiceChanged() {
        // Sets the DSE isn't Google.
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        mProfileSupplier.set(mProfile);

        // Verifies that the SharedPreference is updated once the TemplateUrlService is ready.
        assertEquals(mTemplateUrlService, mDseNewTabUrlManager.getTemplateUrlServiceForTesting());
        verify(mTemplateUrlService).addObserver(mTemplateUrlServiceObserverCaptor.capture());
        assertFalse(mSharedPreferenceManager.readBoolean(ChromePreferenceKeys.IS_DSE_GOOGLE, true));
        assertEquals(
                NEW_TAB_URL,
                mSharedPreferenceManager.readString(ChromePreferenceKeys.DSE_NEW_TAB_URL, null));

        // Verifies that the SharedPreference is updated when the DSE is changed.
        doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        doReturn(true).when(mRegionalCapabilities).isInEeaCountry();
        mTemplateUrlServiceObserverCaptor.getValue().onTemplateURLServiceChanged();
        assertTrue(mSharedPreferenceManager.readBoolean(ChromePreferenceKeys.IS_DSE_GOOGLE, false));
        assertTrue(
                mSharedPreferenceManager.readBoolean(
                        ChromePreferenceKeys.IS_EEA_CHOICE_COUNTRY, false));
        assertFalse(mSharedPreferenceManager.contains(ChromePreferenceKeys.DSE_NEW_TAB_URL));

        mDseNewTabUrlManager.destroy();
        verify(mTemplateUrlService).removeObserver(mTemplateUrlServiceObserverCaptor.capture());
    }

    @Test
    public void testIsDefaultSearchEngineGoogle() {
        assertNull(mDseNewTabUrlManager.getTemplateUrlServiceForTesting());

        assertFalse(
                ChromeSharedPreferences.getInstance().contains(ChromePreferenceKeys.IS_DSE_GOOGLE));
        assertTrue(DseNewTabUrlManager.isDefaultSearchEngineGoogle());

        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.IS_DSE_GOOGLE, false);
        assertFalse(DseNewTabUrlManager.isDefaultSearchEngineGoogle());

        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.IS_DSE_GOOGLE, true);
        assertTrue(DseNewTabUrlManager.isDefaultSearchEngineGoogle());
    }
}
