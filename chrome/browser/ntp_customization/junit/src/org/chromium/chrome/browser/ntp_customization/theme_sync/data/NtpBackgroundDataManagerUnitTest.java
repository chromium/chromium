// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.PlatformType;

/** Tests for {@link NtpBackgroundDataManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundDataManagerUnitTest {
    private NtpBackgroundDataManager mManager;
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mManager = new NtpBackgroundDataManager(mContext);
    }

    @After
    public void tearDown() {
        mManager.resetSharedPreferenceForTesting();
    }

    @Test
    public void testSaveRemoteSyncDataToSharedPreference() {
        @PlatformType int platformType = PlatformType.IOS;
        NtpBackgroundDataColor data1 =
                new NtpBackgroundDataColor(
                        mContext,
                        platformType,
                        NtpThemeColorId.NTP_COLORS_VIRIDIAN,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        NtpBackgroundDataColor data2 =
                new NtpBackgroundDataColor(
                        mContext,
                        platformType,
                        NtpThemeColorId.NTP_COLORS_CITRON,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        NtpBackgroundDataColor data3 =
                new NtpBackgroundDataColor(
                        mContext,
                        platformType,
                        NtpThemeColorId.NTP_COLORS_ORANGE,
                        /* isChromeColorDailyRefreshEnabled= */ true);

        // Save first data.
        mManager.saveRemoteSyncDataToSharedPreference(data1);
        NtpBackgroundDataGroup group =
                mManager.getBackgroundDataGroupFromSharedPreference(platformType);
        assertEquals(1, group.size());
        assertEquals(data1, group.get(0));

        // Save second data. It should be moved to the first.
        mManager.saveRemoteSyncDataToSharedPreference(data2);
        group = mManager.getBackgroundDataGroupFromSharedPreference(platformType);
        assertEquals(2, group.size());
        assertEquals(data2, group.get(0));
        assertEquals(data1, group.get(1));

        // Save third data. It should remove the last one (MAXIMUM_REMOTE_HISTORY = 2).
        mManager.saveRemoteSyncDataToSharedPreference(data3);
        group = mManager.getBackgroundDataGroupFromSharedPreference(platformType);
        assertEquals(2, group.size());
        assertEquals(data3, group.get(0));
        assertEquals(data2, group.get(1));

        // Save first data again. It should move to the first.
        mManager.saveRemoteSyncDataToSharedPreference(data2);
        group = mManager.getBackgroundDataGroupFromSharedPreference(platformType);
        assertEquals(2, group.size());
        assertEquals(data2, group.get(0));
        assertEquals(data3, group.get(1));
    }

    @Test
    public void testSaveRemoteSyncDataListToSharedPreference() {
        @PlatformType int platformType1 = PlatformType.IOS;
        @PlatformType int platformType2 = PlatformType.DESKTOP;
        @PlatformType int platformType3 = PlatformType.ANDROID_LOCAL;
        NtpBackgroundDataColor data1 =
                new NtpBackgroundDataColor(
                        mContext,
                        platformType1,
                        NtpThemeColorId.NTP_COLORS_VIRIDIAN,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        NtpBackgroundDataColor data2 =
                new NtpBackgroundDataColor(
                        mContext,
                        platformType2,
                        NtpThemeColorId.NTP_COLORS_CITRON,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        NtpBackgroundDataColor data3 =
                new NtpBackgroundDataColor(
                        mContext,
                        platformType3,
                        NtpThemeColorId.NTP_COLORS_ORANGE,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        NtpBackgroundDataGroup dataGroup = new NtpBackgroundDataGroup();
        dataGroup.add(data1);
        dataGroup.add(data2);
        dataGroup.add(data3);

        mManager.saveRemoteSyncDataToSharedPreference(dataGroup);
        NtpBackgroundDataGroup group1 =
                mManager.getBackgroundDataGroupFromSharedPreference(platformType1);
        assertNotNull(group1);
        assertEquals(1, group1.size());
        assertEquals(data1, group1.get(0));

        NtpBackgroundDataGroup group2 =
                mManager.getBackgroundDataGroupFromSharedPreference(platformType2);
        assertNotNull(group2);
        assertEquals(1, group2.size());
        assertEquals(data2, group2.get(0));

        NtpBackgroundDataGroup group3 =
                mManager.getBackgroundDataGroupFromSharedPreference(platformType3);
        assertTrue(group3.isEmpty());
    }

    @Test
    public void testSaveUserSelectedBackgroundTypeToSharedPreference() {
        @PlatformType int localPlatform = PlatformType.ANDROID_LOCAL;
        NtpBackgroundDataColor localData1 =
                new NtpBackgroundDataColor(
                        mContext,
                        localPlatform,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        NtpBackgroundDataColor localData2 =
                new NtpBackgroundDataColor(
                        mContext,
                        localPlatform,
                        NtpThemeColorId.NTP_COLORS_AQUA,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        NtpBackgroundDataColor localData3 =
                new NtpBackgroundDataColor(
                        mContext,
                        localPlatform,
                        NtpThemeColorId.NTP_COLORS_GREEN,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        NtpBackgroundDataColor localData4 =
                new NtpBackgroundDataColor(
                        mContext,
                        localPlatform,
                        NtpThemeColorId.NTP_COLORS_VIRIDIAN,
                        /* isChromeColorDailyRefreshEnabled= */ true);

        // Save local selections.
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(localData1);
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(localData2);
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(localData3);
        NtpBackgroundDataGroup group =
                mManager.getBackgroundDataGroupFromSharedPreference(localPlatform);
        assertEquals(3, group.size());
        assertEquals(localData3, group.get(0));

        // Exceed MAXIMUM_LOCAL_HISTORY = 3.
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(localData4);
        group = mManager.getBackgroundDataGroupFromSharedPreference(localPlatform);
        assertEquals(3, group.size());
        assertEquals(localData4, group.get(0));
        assertEquals(localData3, group.get(1));
        assertEquals(localData2, group.get(2));

        // Save a remote background.
        NtpBackgroundDataColor iosData =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.IOS,
                        NtpThemeColorId.NTP_COLORS_CITRON,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(iosData);
        group = mManager.getBackgroundDataGroupFromSharedPreference(localPlatform);
        assertEquals(3, group.size());
        assertEquals(iosData, group.get(0));
        assertEquals(localData4, group.get(1));
        assertEquals(localData3, group.get(2));

        // Save another background from the same remote platform. It should remove the previous one.
        NtpBackgroundDataColor iosData2 =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.IOS,
                        NtpThemeColorId.NTP_COLORS_ORANGE,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(iosData2);
        group = mManager.getBackgroundDataGroupFromSharedPreference(localPlatform);
        assertEquals(3, group.size());

        // Ensure iosData is gone and iosData2 is at the front.
        assertEquals(iosData2, group.get(0));
        assertEquals(localData4, group.get(1));
        assertEquals(localData3, group.get(2));
    }

    @Test
    public void testSaveUserSelectedBackgroundTypeToSharedPreference_Duplicate() {
        @PlatformType int localPlatform = PlatformType.ANDROID_LOCAL;
        NtpBackgroundDataColor localData1 =
                new NtpBackgroundDataColor(
                        mContext,
                        localPlatform,
                        NtpThemeColorId.NTP_COLORS_BLUE,
                        /* isChromeColorDailyRefreshEnabled= */ true);
        NtpBackgroundDataColor localData2 =
                new NtpBackgroundDataColor(
                        mContext,
                        localPlatform,
                        NtpThemeColorId.NTP_COLORS_AQUA,
                        /* isChromeColorDailyRefreshEnabled= */ true);

        // Save local selections.
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(localData1);
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(localData2);
        NtpBackgroundDataGroup group =
                mManager.getBackgroundDataGroupFromSharedPreference(localPlatform);
        assertEquals(2, group.size());
        assertEquals(localData2, group.get(0));
        assertEquals(localData1, group.get(1));

        // Save localData1 again. It should move to the front and size remains 2.
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(localData1);
        group = mManager.getBackgroundDataGroupFromSharedPreference(localPlatform);
        assertEquals(2, group.size());
        assertEquals(localData1, group.get(0));
        assertEquals(localData2, group.get(1));
    }

    @Test
    public void testGetJsonArrayFromSharedPreferenceImpl_Empty() {
        assertNull(mManager.getJsonArrayFromSharedPreferenceImpl(PlatformType.ANDROID_LOCAL));
    }
}
