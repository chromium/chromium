// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.json.JSONException;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataBase.PlatformType;

import java.util.ArrayList;
import java.util.List;

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
    public void testSaveRemoteSyncDataToSharedPreference() throws JSONException {
        @PlatformType int platformType = PlatformType.IOS;
        NtpBackgroundDataColor data1 = new NtpBackgroundDataColor(mContext, platformType, 1, true);
        NtpBackgroundDataColor data2 = new NtpBackgroundDataColor(mContext, platformType, 2, true);
        NtpBackgroundDataColor data3 = new NtpBackgroundDataColor(mContext, platformType, 3, true);

        // Save first data.
        mManager.saveRemoteSyncDataToSharedPreference(data1);
        List<NtpBackgroundDataBase> list =
                mManager.getBackgroundDataListFromSharedPreference(platformType);
        assertEquals(1, list.size());
        assertEquals(data1, list.get(0));

        // Save second data. It should be moved to the first.
        mManager.saveRemoteSyncDataToSharedPreference(data2);
        list = mManager.getBackgroundDataListFromSharedPreference(platformType);
        assertEquals(2, list.size());
        assertEquals(data2, list.get(0));
        assertEquals(data1, list.get(1));

        // Save third data. It should remove the last one (MAXIMUM_REMOTE_HISTORY = 2).
        mManager.saveRemoteSyncDataToSharedPreference(data3);
        list = mManager.getBackgroundDataListFromSharedPreference(platformType);
        assertEquals(2, list.size());
        assertEquals(data3, list.get(0));
        assertEquals(data2, list.get(1));

        // Save first data again. It should move to the first.
        mManager.saveRemoteSyncDataToSharedPreference(data2);
        list = mManager.getBackgroundDataListFromSharedPreference(platformType);
        assertEquals(2, list.size());
        assertEquals(data2, list.get(0));
        assertEquals(data3, list.get(1));
    }

    @Test
    public void testSaveRemoteSyncDataListToSharedPreference() throws JSONException {
        @PlatformType int platformType1 = PlatformType.IOS;
        @PlatformType int platformType2 = PlatformType.DESKTOP;
        @PlatformType int platformType3 = PlatformType.ANDROID_LOCAL;
        NtpBackgroundDataColor data1 =
                new NtpBackgroundDataColor(mContext, platformType1, /* themeColorId= */ 1, true);
        NtpBackgroundDataColor data2 =
                new NtpBackgroundDataColor(mContext, platformType2, /* themeColorId= */ 2, true);
        NtpBackgroundDataColor data3 =
                new NtpBackgroundDataColor(mContext, platformType3, /* themeColorId= */ 3, true);
        List<NtpBackgroundDataBase> dataList = new ArrayList<>();
        dataList.add(data1);
        dataList.add(data2);
        dataList.add(data3);

        mManager.saveRemoteSyncDataToSharedPreference(dataList);
        List<NtpBackgroundDataBase> list1 =
                mManager.getBackgroundDataListFromSharedPreference(platformType1);
        assertNotNull(list1);
        assertEquals(1, list1.size());
        assertEquals(data1, list1.get(0));

        List<NtpBackgroundDataBase> list2 =
                mManager.getBackgroundDataListFromSharedPreference(platformType2);
        assertNotNull(list1);
        assertEquals(1, list2.size());
        assertEquals(data2, list2.get(0));

        List<NtpBackgroundDataBase> list3 =
                mManager.getBackgroundDataListFromSharedPreference(platformType3);
        assertNull(list3);
    }

    @Test
    public void testSaveUserSelectedBackgroundTypeToSharedPreference() throws JSONException {
        @PlatformType int localPlatform = PlatformType.ANDROID_LOCAL;
        NtpBackgroundDataColor localData1 =
                new NtpBackgroundDataColor(
                        mContext, localPlatform, NtpThemeColorId.NTP_COLORS_BLUE, true);
        NtpBackgroundDataColor localData2 =
                new NtpBackgroundDataColor(
                        mContext, localPlatform, NtpThemeColorId.NTP_COLORS_AQUA, true);
        NtpBackgroundDataColor localData3 =
                new NtpBackgroundDataColor(
                        mContext, localPlatform, NtpThemeColorId.NTP_COLORS_GREEN, true);
        NtpBackgroundDataColor localData4 =
                new NtpBackgroundDataColor(
                        mContext, localPlatform, NtpThemeColorId.NTP_COLORS_VIRIDIAN, true);

        // Save local selections.
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(localData1);
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(localData2);
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(localData3);
        List<NtpBackgroundDataBase> list =
                mManager.getBackgroundDataListFromSharedPreference(localPlatform);
        assertEquals(3, list.size());
        assertEquals(localData3, list.get(0));

        // Exceed MAXIMUM_LOCAL_HISTORY = 3.
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(localData4);
        list = mManager.getBackgroundDataListFromSharedPreference(localPlatform);
        assertEquals(3, list.size());
        assertEquals(localData4, list.get(0));
        assertEquals(localData3, list.get(1));
        assertEquals(localData2, list.get(2));

        // Save a remote background.
        NtpBackgroundDataColor iosData =
                new NtpBackgroundDataColor(
                        mContext, PlatformType.IOS, NtpThemeColorId.NTP_COLORS_CITRON, true);
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(iosData);
        list = mManager.getBackgroundDataListFromSharedPreference(localPlatform);
        assertEquals(3, list.size());
        assertEquals(iosData, list.get(0));
        assertEquals(localData4, list.get(1));
        assertEquals(localData3, list.get(2));

        // Save another background from the same remote platform. It should remove the previous one.
        NtpBackgroundDataColor iosData2 =
                new NtpBackgroundDataColor(
                        mContext, PlatformType.IOS, NtpThemeColorId.NTP_COLORS_ORANGE, true);
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(iosData2);
        list = mManager.getBackgroundDataListFromSharedPreference(localPlatform);
        assertEquals(3, list.size());

        // Ensure iosData is gone and iosData2 is at the front.
        assertEquals(iosData2, list.get(0));
        assertEquals(localData4, list.get(1));
        assertEquals(localData3, list.get(2));
    }

    @Test
    public void testGetJsonArrayFromSharedPreferenceImpl_Empty() throws JSONException {
        assertNull(mManager.getJsonArrayFromSharedPreferenceImpl(PlatformType.ANDROID_LOCAL));
    }
}
