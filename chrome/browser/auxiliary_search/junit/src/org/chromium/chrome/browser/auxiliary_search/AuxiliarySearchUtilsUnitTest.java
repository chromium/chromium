// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.auxiliary_search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.sAndroidAppIntegrationWithFaviconUseLargeFavicon;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.io.File;

/** Unit tests for AuxiliarySearchUtils. */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchUtilsUnitTest {
    @Test
    public void testBitmapToString() {
        assertNull(AuxiliarySearchUtils.bitmapToBytes(null));

        Bitmap bitmap = Bitmap.createBitmap(100, 100, Config.RGB_565);
        assertNotNull(AuxiliarySearchUtils.bitmapToBytes(bitmap));
        assertTrue(bitmap.isRecycled());
    }

    @Test
    public void testGetTabDonateFile() {
        Context context = ContextUtils.getApplicationContext();
        File file = AuxiliarySearchUtils.getTabDonateFile(context);
        assertEquals("tabs_donate", file.getName());
    }

    @Test
    public void testGetFaviconSize_small() {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        int faviconSizeSmall =
                resources.getDimensionPixelSize(R.dimen.auxiliary_search_favicon_size_small);

        assertEquals(faviconSizeSmall, AuxiliarySearchUtils.getFaviconSize(resources));
    }

    @Test
    @EnableFeatures("AndroidAppIntegrationWithFavicon:use_large_favicon/true")
    public void testGetFaviconSize() {
        assertTrue(sAndroidAppIntegrationWithFaviconUseLargeFavicon.getValue());

        Resources resources = ContextUtils.getApplicationContext().getResources();
        int faviconSize = resources.getDimensionPixelSize(R.dimen.auxiliary_search_favicon_size);

        assertEquals(faviconSize, AuxiliarySearchUtils.getFaviconSize(resources));
    }

    @Test
    @SmallTest
    public void testShareTabsWithOs() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        AuxiliarySearchHooks hooksMock = Mockito.mock(AuxiliarySearchHooks.class);
        when(hooksMock.isSettingDefaultEnabledByOs()).thenReturn(true);
        AuxiliarySearchControllerFactory.getInstance().setHooksForTesting(hooksMock);
        assertTrue(AuxiliarySearchControllerFactory.getInstance().isSettingDefaultEnabledByOs());

        prefsManager.removeKey(ChromePreferenceKeys.SHARING_TABS_WITH_OS);
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsEnabled());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Search.AuxiliarySearch.ShareTabsWithOs", false)
                        .build();
        AuxiliarySearchUtils.setSharedTabsWithOs(false);
        assertFalse(prefsManager.readBoolean(ChromePreferenceKeys.SHARING_TABS_WITH_OS, true));
        assertFalse(AuxiliarySearchUtils.isShareTabsWithOsEnabled());
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Search.AuxiliarySearch.ShareTabsWithOs", true)
                        .build();
        AuxiliarySearchUtils.setSharedTabsWithOs(true);
        assertTrue(prefsManager.readBoolean(ChromePreferenceKeys.SHARING_TABS_WITH_OS, true));
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsEnabled());
        histogramWatcher.assertExpected();

        AuxiliarySearchUtils.resetSharedPreferenceForTesting();
    }

    @Test
    @SmallTest
    public void testIncreaseModuleImpressions() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_IMPRESSION);

        assertEquals(1, AuxiliarySearchUtils.incrementModuleImpressions());
        assertEquals(2, AuxiliarySearchUtils.incrementModuleImpressions());

        AuxiliarySearchUtils.resetSharedPreferenceForTesting();
    }

    @Test
    @SmallTest
    public void testHasUserResponded() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_USER_RESPONDED);
        assertFalse(AuxiliarySearchUtils.hasUserResponded());

        prefsManager.writeBoolean(
                ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_USER_RESPONDED, true);
        assertTrue(AuxiliarySearchUtils.hasUserResponded());

        AuxiliarySearchUtils.resetSharedPreferenceForTesting();
    }

    @Test
    @SmallTest
    public void testExceedMaxImpressions() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_IMPRESSION);
        assertFalse(AuxiliarySearchUtils.exceedMaxImpressions());

        prefsManager.writeInt(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_IMPRESSION, 2);
        assertFalse(AuxiliarySearchUtils.exceedMaxImpressions());

        prefsManager.writeInt(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_IMPRESSION, 3);
        assertTrue(AuxiliarySearchUtils.exceedMaxImpressions());

        AuxiliarySearchUtils.resetSharedPreferenceForTesting();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON + ":force_card_shown/false"
    })
    public void testCanShowCard() {
        assertTrue(AuxiliarySearchUtils.canShowCard(null));
        assertTrue(AuxiliarySearchUtils.canShowCard(false));

        // Verifies Not to show the card if it has been shown in the current session.
        assertFalse(AuxiliarySearchUtils.canShowCard(true));

        // Verifies Not to show the card if it exceeds the maximum allowed impressions.
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        prefsManager.writeInt(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_IMPRESSION, 3);
        assertFalse(AuxiliarySearchUtils.canShowCard(false));

        // Verifies Not to show the card if the user has responded.
        prefsManager.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_IMPRESSION);
        prefsManager.writeBoolean(
                ChromePreferenceKeys.AUXILIARY_SEARCH_MODULE_USER_RESPONDED, true);
        assertTrue(AuxiliarySearchUtils.hasUserResponded());
        assertFalse(AuxiliarySearchUtils.canShowCard(false));

        AuxiliarySearchUtils.resetSharedPreferenceForTesting();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON + ":force_card_shown/true"
    })
    public void testCanShowCard_ForceCardShown() {
        assertTrue(AuxiliarySearchUtils.FORCE_CARD_SHOWN.getValue());

        // Verifies that the card is always shown if the feature param
        // AuxiliarySearchUtils.FORCE_CARD_SHOWN_PARAM is enabled.
        assertTrue(AuxiliarySearchUtils.canShowCard(true));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON + ":skip_device_check/false"
    })
    public void testIsShareTabsWithOsDefaultEnabled() {
        AuxiliarySearchHooks hooksMock = Mockito.mock(AuxiliarySearchHooks.class);
        when(hooksMock.isEnabled()).thenReturn(true);
        when(hooksMock.isSettingDefaultEnabledByOs()).thenReturn(true);
        AuxiliarySearchControllerFactory.getInstance().setHooksForTesting(hooksMock);

        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsDefaultEnabled());

        when(hooksMock.isSettingDefaultEnabledByOs()).thenReturn(false);
        assertFalse(AuxiliarySearchUtils.isShareTabsWithOsDefaultEnabled());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON + ":skip_device_check/true"
    })
    public void testIsShareTabsWithOsDefaultEnabled_SkipDeviceCheck() {
        assertTrue(AuxiliarySearchUtils.SKIP_DEVICE_CHECK.getValue());

        assertFalse(AuxiliarySearchControllerFactory.getInstance().isSettingDefaultEnabledByOs());
        // Verifies that isShareTabsWithOsDefaultEnabled() returns true if skipping device check is
        // enabled on Pixel devices.
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsDefaultEnabled());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.ANDROID_APP_INTEGRATION_WITH_FAVICON
                + ":skip_device_check/true/show_third_party_card/true"
    })
    public void testIsShareTabsWithOsDefaultEnabled_SkipDeviceCheck_NonPixelDevices() {
        assertTrue(AuxiliarySearchUtils.SKIP_DEVICE_CHECK.getValue());

        assertFalse(AuxiliarySearchControllerFactory.getInstance().isSettingDefaultEnabledByOs());
        // Verifies that isShareTabsWithOsDefaultEnabled() returns false if skipping device check is
        // enabled on third party devices.
        assertFalse(AuxiliarySearchUtils.isShareTabsWithOsDefaultEnabled());
    }
}
