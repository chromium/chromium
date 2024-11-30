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

        AuxiliarySearchUtils.resetSharedTabsWithOsForTesting();
    }
}
