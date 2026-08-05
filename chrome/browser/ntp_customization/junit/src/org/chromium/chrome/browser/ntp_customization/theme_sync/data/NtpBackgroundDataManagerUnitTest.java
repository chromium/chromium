// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync.data;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Matrix;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorInt;
import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.io.File;

/** Tests for {@link NtpBackgroundDataManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpBackgroundDataManagerUnitTest {
    private static final String TEST_COLLECTION_ID = "test_collection";
    private static final String OTHER_COLLECTION_ID = "other_collection";
    private static final @PlatformType int REMOTE_PLATFORM_TYPE = PlatformType.IOS;
    private static final @ColorInt int TEST_PRIMARY_COLOR = Color.RED;

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
        RobolectricUtil.runAllBackgroundAndUi();
        NtpBackgroundDataGroup group =
                mManager.getBackgroundDataGroupFromSharedPreference(platformType);
        assertEquals(1, group.size());
        assertEquals(data1, group.get(0));

        // Save second data. It should be moved to the first.
        mManager.saveRemoteSyncDataToSharedPreference(data2);
        RobolectricUtil.runAllBackgroundAndUi();
        group = mManager.getBackgroundDataGroupFromSharedPreference(platformType);
        assertEquals(2, group.size());
        assertEquals(data2, group.get(0));
        assertEquals(data1, group.get(1));

        // Save third data. It should remove the last one (MAXIMUM_REMOTE_HISTORY = 2).
        mManager.saveRemoteSyncDataToSharedPreference(data3);
        RobolectricUtil.runAllBackgroundAndUi();
        group = mManager.getBackgroundDataGroupFromSharedPreference(platformType);
        assertEquals(2, group.size());
        assertEquals(data3, group.get(0));
        assertEquals(data2, group.get(1));

        // Save first data again. It should move to the first.
        mManager.saveRemoteSyncDataToSharedPreference(data2);
        RobolectricUtil.runAllBackgroundAndUi();
        group = mManager.getBackgroundDataGroupFromSharedPreference(platformType);
        assertEquals(2, group.size());
        assertEquals(data2, group.get(0));
        assertEquals(data3, group.get(1));
    }

    @Test
    public void testSaveRemoteSyncDataListToSharedPreference() {
        @PlatformType int platformType1 = PlatformType.IOS;
        @PlatformType int platformType2 = PlatformType.DESKTOP;
        @PlatformType int platformType3 = PlatformType.ANDROID;
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
        RobolectricUtil.runAllBackgroundAndUi();
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
        @PlatformType int localPlatform = PlatformType.ANDROID;
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
        @PlatformType int localPlatform = PlatformType.ANDROID;
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
    public void testSaveUserSelectedBackgroundType_EvictsUploadImage() {
        @PlatformType int localPlatform = PlatformType.ANDROID;
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        String fileHash = "evictedFileHash";

        NtpBackgroundDataUploadImage uploadImage =
                new NtpBackgroundDataUploadImage(
                        localPlatform,
                        /* backgroundImageInfo= */ null,
                        bitmap,
                        /* primaryColor= */ null,
                        fileHash);

        File savedFile = new File(uploadImage.getLastUploadImageFilePath());
        NtpCustomizationUtils.saveBitmapImageToFile(bitmap, savedFile);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(savedFile.exists());

        // Save local selection.
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(uploadImage);

        // Fill up history to exceed MAXIMUM_LOCAL_HISTORY (which is 3).
        for (int i = 0; i < 3; i++) {
            NtpBackgroundDataColor colorData =
                    new NtpBackgroundDataColor(
                            mContext,
                            localPlatform,
                            NtpThemeColorId.NTP_COLORS_BLUE + i,
                            /* isChromeColorDailyRefreshEnabled= */ true);
            mManager.saveUserSelectedBackgroundTypeToSharedPreference(colorData);
        }

        // Verify that the uploadImage was evicted and its local file was deleted.
        NtpBackgroundDataGroup group =
                mManager.getBackgroundDataGroupFromSharedPreference(localPlatform);
        assertEquals(3, group.size());
        boolean containsUploadImage = false;
        for (int j = 0; j < group.size(); j++) {
            if (group.get(j).equals(uploadImage)) {
                containsUploadImage = true;
                break;
            }
        }
        assertFalse(containsUploadImage);
        assertFalse(savedFile.exists());

        // Clean up
        NtpCustomizationUtils.deleteThemeImageFileDir(NtpCustomizationUtils.NTP_UPLOAD_IMAGES_DIR);
    }

    @Test
    public void testCleanUpForBackgroundData_RemotePlatform() {
        @PlatformType int remotePlatform = PlatformType.IOS;
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);

        // Remote data 1.
        NtpBackgroundDataUploadImage remoteData1 =
                new NtpBackgroundDataUploadImage(
                        remotePlatform, null, bitmap, null, "hash_remote1");
        File file1 = new File(remoteData1.getLastUploadImageFilePath());
        NtpCustomizationUtils.saveBitmapImageToFile(bitmap, file1);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(file1.exists());

        // Case 1: Remote platform, in Local List, NOT in Remote List.
        // Save to Local List.
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(remoteData1);
        // Add 3 local data to evict remoteData1 from Local List.
        for (int i = 0; i < 3; i++) {
            mManager.saveUserSelectedBackgroundTypeToSharedPreference(
                    new NtpBackgroundDataColor(
                            mContext,
                            PlatformType.ANDROID,
                            NtpThemeColorId.NTP_COLORS_BLUE + i,
                            true));
        }
        RobolectricUtil.runAllBackgroundAndUi();
        // Should be deleted because it's not in Remote List.
        assertFalse(file1.exists());

        // Case 2: Remote platform, in Local List, AND in Remote List.
        // Re-create the file.
        NtpCustomizationUtils.saveBitmapImageToFile(bitmap, file1);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(file1.exists());

        // Add to Remote List.
        mManager.saveRemoteSyncDataToSharedPreference(remoteData1);
        RobolectricUtil.runAllBackgroundAndUi();
        // Add back to Local List.
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(remoteData1);
        // Evict from Local List again.
        for (int i = 0; i < 3; i++) {
            mManager.saveUserSelectedBackgroundTypeToSharedPreference(
                    new NtpBackgroundDataColor(
                            mContext,
                            PlatformType.ANDROID,
                            NtpThemeColorId.NTP_COLORS_ORANGE + i,
                            true));
        }
        RobolectricUtil.runAllBackgroundAndUi();
        // Should NOT be deleted because it is still in Remote List.
        assertTrue(file1.exists());

        // Now evict from Remote List.
        // Add 3 more remote data of the same platform to evict remoteData1.
        for (int i = 0; i < 3; i++) {
            mManager.saveRemoteSyncDataToSharedPreference(
                    new NtpBackgroundDataUploadImage(
                            remotePlatform, null, bitmap, null, "hash_remote_case2_" + i));
        }
        RobolectricUtil.runAllBackgroundAndUi();
        // Should be deleted because it is not in Local List anymore.
        assertFalse(file1.exists());

        // Case 3: Evict from Remote List, but it IS in Local List.
        // Re-create the file.
        NtpCustomizationUtils.saveBitmapImageToFile(bitmap, file1);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(file1.exists());

        // Add to Local List first, so it is NOT evicted.
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(remoteData1);
        // Add to Remote List.
        mManager.saveRemoteSyncDataToSharedPreference(remoteData1);
        RobolectricUtil.runAllBackgroundAndUi();
        // Evict from Remote List by adding 3 more remote data.
        for (int i = 0; i < 3; i++) {
            mManager.saveRemoteSyncDataToSharedPreference(
                    new NtpBackgroundDataUploadImage(
                            remotePlatform, null, bitmap, null, "hash_remote_case3_" + i));
        }
        RobolectricUtil.runAllBackgroundAndUi();
        // Should NOT be deleted because it is still in Local List.
        assertTrue(file1.exists());

        // Clean up
        NtpCustomizationUtils.deleteThemeImageFileDir(NtpCustomizationUtils.NTP_UPLOAD_IMAGES_DIR);
    }

    @Test
    public void testCleanUpForBackgroundData_MultipleRemotePlatforms() {
        @PlatformType int remotePlatform1 = PlatformType.IOS;
        @PlatformType int remotePlatform2 = PlatformType.DESKTOP;
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        String sharedHash = "shared_remote_hash";

        NtpBackgroundDataUploadImage remoteData1 =
                new NtpBackgroundDataUploadImage(
                        remotePlatform1,
                        /* backgroundImageInfo= */ null,
                        bitmap,
                        /* primaryColor= */ null,
                        sharedHash);
        NtpBackgroundDataUploadImage remoteData2 =
                new NtpBackgroundDataUploadImage(
                        remotePlatform2,
                        /* backgroundImageInfo= */ null,
                        bitmap,
                        /* primaryColor= */ null,
                        sharedHash);

        File sharedFile = new File(remoteData1.getLastUploadImageFilePath());
        assertEquals(
                sharedFile.getAbsolutePath(),
                new File(remoteData2.getLastUploadImageFilePath()).getAbsolutePath());

        NtpCustomizationUtils.saveBitmapImageToFile(bitmap, sharedFile);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(sharedFile.exists());

        // Save both to their respective remote lists.
        mManager.saveRemoteSyncDataToSharedPreference(remoteData1);
        mManager.saveRemoteSyncDataToSharedPreference(remoteData2);
        RobolectricUtil.runAllBackgroundAndUi();

        // Evict remoteData1 from remotePlatform1 list by adding 3 other themes.
        for (int i = 0; i < 3; i++) {
            mManager.saveRemoteSyncDataToSharedPreference(
                    new NtpBackgroundDataUploadImage(
                            remotePlatform1,
                            /* backgroundImageInfo= */ null,
                            bitmap,
                            /* primaryColor= */ null,
                            "other_hash1_" + i));
        }
        RobolectricUtil.runAllBackgroundAndUi();

        // The file should NOT be deleted because it is still in remotePlatform2 list.
        assertTrue(sharedFile.exists());

        // Now evict remoteData2 from remotePlatform2 list.
        for (int j = 0; j < 3; j++) {
            mManager.saveRemoteSyncDataToSharedPreference(
                    new NtpBackgroundDataUploadImage(
                            remotePlatform2,
                            /* backgroundImageInfo= */ null,
                            bitmap,
                            /* primaryColor= */ null,
                            "other_hash2_" + j));
        }
        RobolectricUtil.runAllBackgroundAndUi();

        // Now the file SHOULD be deleted.
        assertFalse(sharedFile.exists());

        // Clean up
        NtpCustomizationUtils.deleteThemeImageFileDir(NtpCustomizationUtils.NTP_UPLOAD_IMAGES_DIR);
    }

    @Test
    public void testCleanUpForBackgroundData_ReplaceRemoteThemeLocally() {
        @PlatformType int remotePlatform = PlatformType.IOS;
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        String remoteHash = "remote_hash_for_local_replace";

        NtpBackgroundDataUploadImage remoteData =
                new NtpBackgroundDataUploadImage(
                        remotePlatform,
                        /* backgroundImageInfo= */ null,
                        bitmap,
                        /* primaryColor= */ null,
                        remoteHash);

        File remoteFile = new File(remoteData.getLastUploadImageFilePath());
        NtpCustomizationUtils.saveBitmapImageToFile(bitmap, remoteFile);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(remoteFile.exists());

        // Save to Remote List.
        mManager.saveRemoteSyncDataToSharedPreference(remoteData);
        // Save to Local List.
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(remoteData);
        RobolectricUtil.runAllBackgroundAndUi();

        // Evict from Remote List.
        for (int i = 0; i < 3; i++) {
            mManager.saveRemoteSyncDataToSharedPreference(
                    new NtpBackgroundDataUploadImage(
                            remotePlatform,
                            /* backgroundImageInfo= */ null,
                            bitmap,
                            /* primaryColor= */ null,
                            "other_remote_hash_" + i));
        }
        RobolectricUtil.runAllBackgroundAndUi();

        // Should NOT be deleted because it is still in Local List.
        assertTrue(remoteFile.exists());

        // Replace the remote theme in Local List by selecting a NEW theme from the SAME remote
        // platform.
        NtpBackgroundDataUploadImage newRemoteData =
                new NtpBackgroundDataUploadImage(
                        remotePlatform,
                        /* backgroundImageInfo= */ null,
                        bitmap,
                        /* primaryColor= */ null,
                        "new_remote_hash");
        mManager.saveUserSelectedBackgroundTypeToSharedPreference(newRemoteData);
        RobolectricUtil.runAllBackgroundAndUi();

        // Now the file for the old remote theme SHOULD be deleted.
        assertFalse(remoteFile.exists());

        // Clean up
        NtpCustomizationUtils.deleteThemeImageFileDir(NtpCustomizationUtils.NTP_UPLOAD_IMAGES_DIR);
    }

    @Test
    public void testGetJsonArrayFromSharedPreferenceImpl_Empty() {
        assertNull(mManager.getJsonArrayFromSharedPreferenceImpl(PlatformType.ANDROID));
    }

    @Test
    public void testUpdateRemoteSyncDataToSharedPreference() {
        GURL url = JUnitTestGURLs.URL_1;
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        url,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpBackgroundDataThemeCollection themeCollection =
                new NtpBackgroundDataThemeCollection(
                        REMOTE_PLATFORM_TYPE,
                        info,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        TEST_PRIMARY_COLOR,
                        /* fileIdHash= */ null);

        // Case 1: List is empty. Update should do nothing.
        mManager.updateRemoteSyncDataToSharedPreference(themeCollection);
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(
                mManager.getBackgroundDataGroupFromSharedPreference(REMOTE_PLATFORM_TYPE)
                        .isEmpty());

        // Case 2: Object not in list. Update should do nothing.
        // First add a different data to the list.
        NtpBackgroundDataThemeCollection differentCollection =
                new NtpBackgroundDataThemeCollection(
                        REMOTE_PLATFORM_TYPE,
                        new CustomBackgroundInfo(
                                JUnitTestGURLs.URL_2,
                                OTHER_COLLECTION_ID,
                                /* isUploadedImage= */ false,
                                /* isDailyRefreshEnabled= */ false),
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        TEST_PRIMARY_COLOR,
                        /* fileIdHash= */ null);
        mManager.saveRemoteSyncDataToSharedPreference(differentCollection);
        RobolectricUtil.runAllBackgroundAndUi();

        mManager.updateRemoteSyncDataToSharedPreference(themeCollection);
        RobolectricUtil.runAllBackgroundAndUi();
        NtpBackgroundDataGroup group =
                mManager.getBackgroundDataGroupFromSharedPreference(REMOTE_PLATFORM_TYPE);
        assertEquals(1, group.size());
        assertEquals(differentCollection, group.get(0));

        // Case 3: Object is in the list. Update should find and replace it.
        mManager.saveRemoteSyncDataToSharedPreference(themeCollection);
        RobolectricUtil.runAllBackgroundAndUi();

        group = mManager.getBackgroundDataGroupFromSharedPreference(REMOTE_PLATFORM_TYPE);
        assertEquals(2, group.size());
        // saveRemoteSyncDataToSharedPreference adds to the front or moves to front.
        // So themeCollection should be at index 0.
        assertEquals(themeCollection, group.get(0));
        NtpBackgroundDataThemeCollection savedData =
                (NtpBackgroundDataThemeCollection) group.get(0);
        assertFalse(savedData.isBitmapSaved());
        assertNull(savedData.getBackgroundImageInfo());

        // Modify the object as requested by the user.
        themeCollection.setIsBitmapSaved(/* isBitmapSaved= */ true);
        BackgroundImageInfo backgroundImageInfo =
                new BackgroundImageInfo(
                        new Matrix(),
                        new Matrix(),
                        /* portraitWindowSize= */ null,
                        /* landscapeWindowSize= */ null);
        themeCollection.setBackgroundImageInfo(backgroundImageInfo);

        mManager.updateRemoteSyncDataToSharedPreference(themeCollection);
        RobolectricUtil.runAllBackgroundAndUi();

        group = mManager.getBackgroundDataGroupFromSharedPreference(REMOTE_PLATFORM_TYPE);
        assertEquals(2, group.size());
        NtpBackgroundDataThemeCollection updatedData =
                (NtpBackgroundDataThemeCollection) group.get(0);
        assertTrue(updatedData.isBitmapSaved());
        assertNotNull(updatedData.getBackgroundImageInfo());
        assertEquals(backgroundImageInfo, updatedData.getBackgroundImageInfo());

        // Ensure the other object in the list was not modified.
        assertEquals(differentCollection, group.get(1));
    }

    @Test
    public void testSaveRemoteSyncDataToSharedPreference_PreservesEnrichedMetadata() {
        @PlatformType int platformType = PlatformType.DESKTOP;
        GURL url = JUnitTestGURLs.URL_1;
        String collectionId = "test_collection";
        CustomBackgroundInfo customBgInfo =
                new CustomBackgroundInfo(
                        url,
                        collectionId,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);

        // 1. Initial remote data without BackgroundImageInfo (as received from native sync).
        NtpBackgroundDataThemeCollection initialData =
                new NtpBackgroundDataThemeCollection(
                        platformType,
                        customBgInfo,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        /* primaryColor= */ null,
                        /* fileIdHash= */ null);

        mManager.saveRemoteSyncDataToSharedPreference(initialData);
        RobolectricUtil.runAllBackgroundAndUi();

        NtpBackgroundDataGroup group =
                mManager.getBackgroundDataGroupFromSharedPreference(platformType);
        assertEquals(1, group.size());
        assertEquals(initialData, group.get(0));
        assertNull(((NtpBackgroundDataThemeCollection) group.get(0)).getBackgroundImageInfo());

        // 2. Simulate UI reading from SharedPreferences and enriching it with BackgroundImageInfo.
        BackgroundImageInfo enrichedInfo =
                new BackgroundImageInfo(new Matrix(), new Matrix(), null, null);
        NtpBackgroundDataThemeCollection enrichedData =
                new NtpBackgroundDataThemeCollection(
                        platformType,
                        customBgInfo,
                        enrichedInfo,
                        /* bitmap= */ null,
                        /* primaryColor= */ null,
                        /* fileIdHash= */ null);
        mManager.saveRemoteSyncDataToSharedPreference(enrichedData);
        RobolectricUtil.runAllBackgroundAndUi();

        group = mManager.getBackgroundDataGroupFromSharedPreference(platformType);
        assertEquals(1, group.size());
        assertEquals(enrichedData, group.get(0));
        assertNotNull(((NtpBackgroundDataThemeCollection) group.get(0)).getBackgroundImageInfo());

        // 3. Simulate another remote sync update arriving from native with incomplete info (null
        // BackgroundImageInfo).
        NtpBackgroundDataThemeCollection subsequentSyncData =
                new NtpBackgroundDataThemeCollection(
                        platformType,
                        customBgInfo,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        /* primaryColor= */ null,
                        /* fileIdHash= */ null);
        mManager.saveRemoteSyncDataToSharedPreference(subsequentSyncData);
        RobolectricUtil.runAllBackgroundAndUi();

        // Verify that the enriched BackgroundImageInfo from step 2 was preserved!
        group = mManager.getBackgroundDataGroupFromSharedPreference(platformType);
        assertEquals(1, group.size());
        assertEquals(enrichedData, group.get(0));
        assertNotNull(((NtpBackgroundDataThemeCollection) group.get(0)).getBackgroundImageInfo());
    }
}
