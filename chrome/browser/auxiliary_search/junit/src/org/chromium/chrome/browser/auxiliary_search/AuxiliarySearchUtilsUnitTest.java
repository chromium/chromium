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

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.TimeUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchGroupProto.AuxiliarySearchEntry;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchProvider.MetaDataVersion;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.url.JUnitTestGURLs;

import java.io.File;

/** Unit tests for AuxiliarySearchUtils. */
@RunWith(BaseRobolectricTestRunner.class)
public class AuxiliarySearchUtilsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private AuxiliarySearchHooks mAuxiliarySearchHooks;
    @Mock private Tab mTab;

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
    public void testGetFaviconSize() {
        Resources resources = ContextUtils.getApplicationContext().getResources();
        int faviconSize = resources.getDimensionPixelSize(R.dimen.auxiliary_search_favicon_size);

        assertEquals(faviconSize, AuxiliarySearchUtils.getFaviconSize(resources));
    }

    @Test
    public void testShareTabsWithOs() {
        SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
        when(mAuxiliarySearchHooks.isSettingDefaultEnabledByOs()).thenReturn(true);
        ServiceLoaderUtil.setInstanceForTesting(AuxiliarySearchHooks.class, mAuxiliarySearchHooks);
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
    @EnableFeatures({
        ChromeFeatureList.ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE
                + ":multi_data_source_skip_device_check/false"
    })
    public void testIsShareTabsWithOsDefaultEnabled_MultiDataSource() {
        when(mAuxiliarySearchHooks.isEnabled()).thenReturn(true);
        when(mAuxiliarySearchHooks.isSettingDefaultEnabledByOs()).thenReturn(true);
        ServiceLoaderUtil.setInstanceForTesting(AuxiliarySearchHooks.class, mAuxiliarySearchHooks);

        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsDefaultEnabled());

        when(mAuxiliarySearchHooks.isSettingDefaultEnabledByOs()).thenReturn(false);
        assertFalse(AuxiliarySearchUtils.isShareTabsWithOsDefaultEnabled());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE
                + ":multi_data_source_skip_device_check/true"
    })
    public void testIsShareTabsWithOsDefaultEnabled_SkipDeviceCheck_MultiDataSource() {
        assertTrue(AuxiliarySearchUtils.MULTI_DATA_SOURCE_SKIP_DEVICE_CHECK.getValue());

        assertFalse(AuxiliarySearchControllerFactory.getInstance().isSettingDefaultEnabledByOs());
        // Verifies that isShareTabsWithOsDefaultEnabled() returns true if skipping device check is
        // enabled on Pixel devices.
        assertTrue(AuxiliarySearchUtils.isShareTabsWithOsDefaultEnabled());
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE
                + ":multi_data_source_skip_device_check/true",
        ChromeFeatureList.ANDROID_APP_INTEGRATION_MODULE + ":show_third_party_card/true"
    })
    public void
            testIsShareTabsWithOsDefaultEnabled_SkipDeviceCheck_NonPixelDevices_MultiDataSource() {
        assertTrue(AuxiliarySearchUtils.MULTI_DATA_SOURCE_SKIP_DEVICE_CHECK.getValue());

        assertFalse(AuxiliarySearchControllerFactory.getInstance().isSettingDefaultEnabledByOs());
        // Verifies that isShareTabsWithOsDefaultEnabled() returns false if skipping device check is
        // enabled on third party devices.
        assertFalse(AuxiliarySearchUtils.isShareTabsWithOsDefaultEnabled());
    }

    @Test
    public void testGetMetadataVersion() {
        assertEquals(MetaDataVersion.V1, AuxiliarySearchUtils.getMetadataVersion(mTab));

        AuxiliarySearchEntry entry = AuxiliarySearchEntry.newBuilder().build();
        assertEquals(MetaDataVersion.V1, AuxiliarySearchUtils.getMetadataVersion(entry));

        AuxiliarySearchDataEntry dataEntry =
                new AuxiliarySearchDataEntry(
                        /* type= */ AuxiliarySearchEntryType.TAB,
                        /* url= */ JUnitTestGURLs.URL_1,
                        /* title= */ "Title 1",
                        /* lastActiveTime= */ TimeUtils.uptimeMillis(),
                        /* tabId= */ 10,
                        /* appId= */ null,
                        /* visitId= */ -1,
                        /* score= */ 0);
        assertEquals(
                MetaDataVersion.MULTI_TYPE_V2, AuxiliarySearchUtils.getMetadataVersion(dataEntry));
    }

    @Test
    public void testSchemaVersion() {
        var sharedPreference = ChromeSharedPreferences.getInstance();
        sharedPreference.removeKey(ChromePreferenceKeys.AUXILIARY_SEARCH_SCHEMA_VERSION);

        assertEquals(0, AuxiliarySearchUtils.getSchemaVersion());

        int version = 10;
        AuxiliarySearchUtils.setSchemaVersion(version);
        assertEquals(version, AuxiliarySearchUtils.getSchemaVersion());
    }
}
