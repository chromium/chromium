// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;

import android.content.Context;
import android.graphics.Bitmap;

import org.jni_zero.JniUniquePtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils;
import org.chromium.chrome.browser.ntp_customization.policy.NtpCustomizationPolicyManager;
import org.chromium.chrome.browser.ntp_customization.theme.NtpSyncedThemeBridge.NativeNtpSyncedThemeBridge;
import org.chromium.chrome.browser.ntp_customization.theme.NtpSyncedThemeBridge.SyncedBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.CrossDeviceThemeTracker;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataUploadImage;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link NtpSyncedThemeManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
public class NtpSyncedThemeManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private Profile mOtherProfile;
    @Mock private ImageFetcher mImageFetcher;
    @Mock private NtpSyncedThemeBridge.Natives mNatives;
    @Mock private JniUniquePtr<NativeNtpSyncedThemeBridge> mNativeNtpSyncedThemeBridge;
    @Mock private CrossDeviceThemeTracker.Natives mCrossDeviceThemeTrackerNatives;
    @Mock private Callback<@Nullable NtpBackgroundDataThemeCollection> mDownloadCallback;
    @Captor private ArgumentCaptor<NtpSyncedThemeBridge> mBridgeCaptor;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mBitmapCallbackCaptor;

    private static final String TEST_COLLECTION_ID = "collectionId";
    private static final String TEST_HASH = "hash";

    private NtpSyncedThemeManager mNtpSyncedThemeManager;
    private Context mContext;

    @Before
    public void setUp() {
        // The manager uses ContextUtils.getApplicationContext(), which Robolectric wraps.
        mContext = ContextUtils.getApplicationContext();
        NtpCustomizationUtils.setImageFetcherForTesting(mImageFetcher);
        NtpSyncedThemeBridgeJni.setInstanceForTesting(mNatives);
        CrossDeviceThemeTracker.setInstanceForTesting(mCrossDeviceThemeTrackerNatives);
        when(mNatives.init(any(), any())).thenReturn(mNativeNtpSyncedThemeBridge);
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @After
    public void tearDown() {
        if (mNtpSyncedThemeManager != null) {
            mNtpSyncedThemeManager.destroy();
        }
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
    }

    @Test
    public void testFetchNextThemeCollectionImageAfterDailyRefreshApplied_dailyRefreshDisabled() {
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(THEME_COLLECTION);
        CustomBackgroundInfo currentInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(currentInfo);

        mNtpSyncedThemeManager = createInitializedManager();
        mNtpSyncedThemeManager.fetchNextThemeCollectionImageAfterDailyRefreshApplied();
        verify(mNatives, never()).fetchNextThemeCollectionImage(any());
    }

    @Test
    public void testFetchNextThemeCollectionImageAfterDailyRefreshApplied_infoAlreadyExists() {
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(THEME_COLLECTION);
        CustomBackgroundInfo currentInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(currentInfo);

        // Set some daily refresh info.
        CustomBackgroundInfo dailyInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_2,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setDailyRefreshCustomBackgroundInfoToSharedPreference(dailyInfo);

        mNtpSyncedThemeManager = createInitializedManager();
        mNtpSyncedThemeManager.fetchNextThemeCollectionImageAfterDailyRefreshApplied();
        verify(mNatives, never()).fetchNextThemeCollectionImage(any());
    }

    @Test
    public void testFetchNextThemeCollectionImageAfterDailyRefreshApplied() {
        // 1. Set up preconditions.
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(THEME_COLLECTION);
        CustomBackgroundInfo currentInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(currentInfo);

        // Make sure daily refresh info is not present.
        assertNull(NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference());

        mNtpSyncedThemeManager = createInitializedManager();

        // 2. Call the method.
        mNtpSyncedThemeManager.fetchNextThemeCollectionImageAfterDailyRefreshApplied();

        // 3. Verify bridge is created and fetch is called.
        verify(mNatives).init(eq(mProfile), mBridgeCaptor.capture());
        verify(mNatives).fetchNextThemeCollectionImage(any());

        // 4. Simulate native callback with the info for the next day's image.
        NtpSyncedThemeBridge bridge = mBridgeCaptor.getValue();
        CustomBackgroundInfo nextInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_2,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        when(mNatives.getCustomBackgroundInfo(any()))
                .thenReturn(new SyncedBackgroundInfo(nextInfo, /* primaryColor= */ null));
        bridge.onThemeCollectionUpdated(/* isImageFromSync= */ false);

        // 5. Verify image is fetched.
        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mBitmapCallbackCaptor.getValue().onResult(bitmap);

        RobolectricUtil.runAllBackgroundAndUi();

        // 6. Verify daily refresh info is saved and bridge is kept alive.
        assertTrue(NtpCustomizationUtils.createDailyRefreshBackgroundImageFile().exists());
        assertNotNull(
                NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference());
        assertNotNull(NtpCustomizationUtils.readDailyRefreshNtpBackgroundImageInfo());
        assertNotNull(
                NtpCustomizationUtils.getDailyRefreshCustomizedPrimaryColorFromSharedPreference());

        verify(mNativeNtpSyncedThemeBridge, never()).destroy();
    }

    @Test
    public void testDestroy() {
        mNtpSyncedThemeManager = createInitializedManager();
        mNtpSyncedThemeManager.destroy();
        verify(mNativeNtpSyncedThemeBridge).destroy();
        verify(mImageFetcher).destroy();
    }

    @Test
    public void testOnFinishNativeInitialization_sameProfile_initializesOnce() {
        mNtpSyncedThemeManager = createInitializedManager();
        mNtpSyncedThemeManager.onFinishNativeInitialization(mProfile);

        verify(mNatives).init(eq(mProfile), any());
        verify(mNativeNtpSyncedThemeBridge, never()).destroy();
        verify(mImageFetcher, never()).destroy();
    }

    @Test
    public void testOnFinishNativeInitialization_differentProfile_destroysPreviousState() {
        mNtpSyncedThemeManager = createInitializedManager();
        mNtpSyncedThemeManager.onFinishNativeInitialization(mOtherProfile);

        verify(mNatives).init(eq(mProfile), any());
        verify(mNativeNtpSyncedThemeBridge).destroy();
        verify(mImageFetcher).destroy();
        verify(mNatives).init(eq(mOtherProfile), any());

        // Theme commits reach the new profile's bridge.
        NtpCustomizationConfigManager.getInstance()
                .onBackgroundDataChanged(
                        mContext,
                        /* backgroundData= */ null,
                        /* shouldNotifySyncedThemeManager= */ true);
        verify(mNatives).resetCustomBackgroundInfo(any());
    }

    @Test
    public void testBeforeInitialization_isNoOp() {
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(THEME_COLLECTION);
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(info);
        mNtpSyncedThemeManager =
                new NtpSyncedThemeManager(NtpCustomizationConfigManager.getInstance());

        mNtpSyncedThemeManager.fetchNextThemeCollectionImageAfterDailyRefreshApplied();
        mNtpSyncedThemeManager.onThemeCommitted(/* data= */ null);
        mNtpSyncedThemeManager.onThemeCollectionImageAvailable(
                info, /* primaryColor= */ null, /* isImageFromSync= */ true);
        mNtpSyncedThemeManager.destroy();

        verify(mNatives, never()).init(any(), any());
        verify(mNatives, never()).fetchNextThemeCollectionImage(any());
        verify(mNatives, never()).resetCustomBackgroundInfo(any());
        verify(mNativeNtpSyncedThemeBridge, never()).destroy();
        verify(mImageFetcher, never()).fetchImage(any(), any());
        verify(mImageFetcher, never()).destroy();
    }

    @Test
    public void testDestroy_dropsThemeCommits() {
        NtpCustomizationConfigManager configManager = NtpCustomizationConfigManager.getInstance();
        mNtpSyncedThemeManager = createInitializedManager();

        configManager.onBackgroundDataChanged(
                mContext, /* backgroundData= */ null, /* shouldNotifySyncedThemeManager= */ true);
        verify(mNatives).resetCustomBackgroundInfo(any());

        mNtpSyncedThemeManager.destroy();
        clearInvocations(mNatives);
        configManager.onBackgroundDataChanged(
                mContext, /* backgroundData= */ null, /* shouldNotifySyncedThemeManager= */ true);
        verify(mNatives, never()).resetCustomBackgroundInfo(any());
    }

    @Test
    public void testOnThemeCollectionUpdated_syncedStaticThemeCollection() {
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        NtpBackgroundDataThemeCollection themeCollectionData =
                applySyncedStaticThemeCollection(/* syncedColor= */ null, bitmap);

        // Without a synced color, the color is extracted from the bitmap.
        assertNotNull(themeCollectionData.getPrimaryColor());
        assertEquals(
                NtpCustomizationUtils.getContentBasedSeedColor(bitmap),
                themeCollectionData.getPrimaryColor());

        assertEquals(
                THEME_COLLECTION, NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
        assertNotNull(NtpCustomizationUtils.getCustomBackgroundInfoFromSharedPreference());
    }

    @Test
    public void testOnThemeCollectionUpdated_syncedStaticThemeCollectionWithMainColor() {
        int syncedColor = 0xFF112233;
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        NtpBackgroundDataThemeCollection themeCollectionData =
                applySyncedStaticThemeCollection(syncedColor, bitmap);

        // The synced color is used instead of extracting one from the bitmap.
        assertEquals(Integer.valueOf(syncedColor), themeCollectionData.getPrimaryColor());
    }

    /**
     * Delivers a synced static theme collection image with {@code syncedColor}, completes the image
     * fetch with {@code bitmap}, and returns the data passed to {@link
     * NtpCustomizationConfigManager#onSyncedThemeCollectionImageChanged}.
     */
    private NtpBackgroundDataThemeCollection applySyncedStaticThemeCollection(
            @Nullable Integer syncedColor, Bitmap bitmap) {
        NtpCustomizationConfigManager configManagerSpy =
                Mockito.spy(NtpCustomizationConfigManager.getInstance());
        NtpCustomizationConfigManager.setInstanceForTesting(configManagerSpy);
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();

        CustomBackgroundInfo syncedInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        when(mNatives.getCustomBackgroundInfo(any()))
                .thenReturn(new SyncedBackgroundInfo(syncedInfo, syncedColor));

        bridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);

        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        mBitmapCallbackCaptor.getValue().onResult(bitmap);
        RobolectricUtil.runAllBackgroundAndUi();

        ArgumentCaptor<NtpBackgroundDataThemeCollection> themeCollectionCaptor =
                ArgumentCaptor.forClass(NtpBackgroundDataThemeCollection.class);
        verify(configManagerSpy)
                .onSyncedThemeCollectionImageChanged(eq(mContext), themeCollectionCaptor.capture());
        NtpBackgroundDataThemeCollection themeCollectionData = themeCollectionCaptor.getValue();
        assertNotNull(themeCollectionData);
        return themeCollectionData;
    }

    private NtpSyncedThemeBridge initSyncedThemeManagerAndGetBridge() {
        mNtpSyncedThemeManager = createInitializedManager();
        verify(mNatives).init(eq(mProfile), mBridgeCaptor.capture());
        return mBridgeCaptor.getValue();
    }

    /**
     * Creates a manager backed by the current config manager and attaches it to {@code mProfile}.
     */
    private NtpSyncedThemeManager createInitializedManager() {
        NtpSyncedThemeManager manager =
                new NtpSyncedThemeManager(NtpCustomizationConfigManager.getInstance());
        // The config manager commits theme changes to the singleton.
        NtpSyncedThemeManager.setInstanceForTesting(manager);
        manager.onFinishNativeInitialization(mProfile);
        return manager;
    }

    @Test
    public void testOnChromeColorSynced_validColor() {
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();

        bridge.onChromeColorSynced(NtpThemeColorId.NTP_COLORS_BLUE);

        assertEquals(
                NtpCustomizationUtils.NtpBackgroundType.CHROME_COLOR,
                NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
        assertEquals(
                NtpThemeColorId.NTP_COLORS_BLUE,
                NtpCustomizationUtils.getNtpThemeColorIdFromSharedPreference());
    }

    @Test
    public void testOnChromeColorSynced_defaultOrLess() {
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();

        bridge.onChromeColorSynced(NtpThemeColorId.DEFAULT);

        assertEquals(
                NtpCustomizationUtils.NtpBackgroundType.DEFAULT,
                NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
    }

    @Test
    public void testOnChromeColorSynced_outOfBounds() {
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(THEME_COLLECTION);
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();

        bridge.onChromeColorSynced(NtpThemeColorId.NUM_ENTRIES);

        // Out of bounds color should reset to default.
        assertEquals(
                NtpCustomizationUtils.NtpBackgroundType.DEFAULT,
                NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
    }

    @Test
    public void testOnDefaultThemeSynced() {
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(
                NtpCustomizationUtils.NtpBackgroundType.CHROME_COLOR);
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();

        bridge.onDefaultThemeSynced();

        assertEquals(
                NtpCustomizationUtils.NtpBackgroundType.DEFAULT,
                NtpCustomizationUtils.getNtpBackgroundTypeFromSharedPreference());
    }

    @Test
    public void testAddOneShotCompletionCallback_downloadSuccess() {
        setupImageDownloading();

        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mBitmapCallbackCaptor.getValue().onResult(bitmap);

        ArgumentCaptor<NtpBackgroundDataThemeCollection> dataCaptor =
                ArgumentCaptor.forClass(NtpBackgroundDataThemeCollection.class);
        verify(mDownloadCallback).onResult(dataCaptor.capture());
        assertFalse(mNtpSyncedThemeManager.isImageDownloading());
        assertNotNull(dataCaptor.getValue());
        assertEquals(bitmap, dataCaptor.getValue().getBitmap());
    }

    @Test
    public void testAddOneShotCompletionCallback_downloadFailure() {
        setupImageDownloading();

        mBitmapCallbackCaptor.getValue().onResult(null);

        verify(mDownloadCallback).onResult(isNull());
        assertFalse(mNtpSyncedThemeManager.isImageDownloading());
    }

    @Test
    public void testAddOneShotCompletionCallback_notDownloading() {
        mNtpSyncedThemeManager = createInitializedManager();

        mNtpSyncedThemeManager.addOneShotCompletionCallback(mDownloadCallback);

        verify(mDownloadCallback).onResult(isNull());
    }

    @Test
    public void testDailyRefresh_doesNotTriggerDownloadCallback() {
        setupDailyRefreshImageFetching();

        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mBitmapCallbackCaptor.getValue().onResult(bitmap);

        verify(mDownloadCallback, never()).onResult(any());
        assertFalse(mNtpSyncedThemeManager.isImageDownloading());
    }

    @Test
    public void testDailyRefresh_downloadFailure_doesNotTriggerDownloadCallback() {
        setupDailyRefreshImageFetching();

        mBitmapCallbackCaptor.getValue().onResult(null);

        verify(mDownloadCallback, never()).onResult(any());
        assertFalse(mNtpSyncedThemeManager.isImageDownloading());
    }

    private void setupDailyRefreshImageFetching() {
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();

        CustomBackgroundInfo dailyInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_2,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        when(mNatives.getCustomBackgroundInfo(any()))
                .thenReturn(new SyncedBackgroundInfo(dailyInfo, /* primaryColor= */ null));

        bridge.onThemeCollectionUpdated(/* isImageFromSync= */ false);

        assertFalse(mNtpSyncedThemeManager.isImageDownloading());

        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
    }

    private void setupImageDownloading() {
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();

        CustomBackgroundInfo syncedInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        when(mNatives.getCustomBackgroundInfo(any()))
                .thenReturn(new SyncedBackgroundInfo(syncedInfo, /* primaryColor= */ null));

        bridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);

        assertTrue(mNtpSyncedThemeManager.isImageDownloading());

        mNtpSyncedThemeManager.addOneShotCompletionCallback(mDownloadCallback);

        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
    }

    @Test
    public void testOnThemeCommitted_colorData_defaultId_resetsInsteadOfSettingColor() {
        mNtpSyncedThemeManager = createInitializedManager();
        NtpBackgroundDataColor colorData =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        NtpThemeColorId.DEFAULT,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpSyncedThemeManager.onThemeCommitted(colorData);
        verify(mNatives).resetCustomBackgroundInfo(any());
        verify(mNatives, never()).setChromeColor(any(), anyInt());
    }

    @Test
    public void testOnThemeCommitted_nullData_resetsBackground() {
        mNtpSyncedThemeManager = createInitializedManager();
        mNtpSyncedThemeManager.onThemeCommitted(/* data= */ null);
        verify(mNatives).resetCustomBackgroundInfo(any());
    }

    @Test
    public void testOnThemeCommitted_colorData_setsChromeColor() {
        mNtpSyncedThemeManager = createInitializedManager();
        NtpBackgroundDataColor colorData =
                new NtpBackgroundDataColor(
                        mContext,
                        PlatformType.ANDROID,
                        NtpThemeColorId.NTP_COLORS_GREEN,
                        /* isChromeColorDailyRefreshEnabled= */ false);
        mNtpSyncedThemeManager.onThemeCommitted(colorData);
        verify(mNatives).setChromeColor(any(), eq(NtpThemeColorId.NTP_COLORS_GREEN));
    }

    @Test
    public void testOnThemeCommitted_uploadImageData_selectsLocalImage() {
        mNtpSyncedThemeManager = createInitializedManager();
        NtpBackgroundDataUploadImage uploadData =
                new NtpBackgroundDataUploadImage(
                        PlatformType.ANDROID,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        /* primaryColor= */ null,
                        /* fileIdHash= */ null);
        mNtpSyncedThemeManager.onThemeCommitted(uploadData);
        verify(mNatives).selectLocalBackgroundImage(any());
    }

    @Test
    public void testOnThemeCommitted_themeCollectionData_updatesColor() {
        mNtpSyncedThemeManager = createInitializedManager();
        String attributionLine1 = "Attribution 1";
        String attributionLine2 = "Attribution 2";
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true,
                        attributionLine1,
                        attributionLine2,
                        JUnitTestGURLs.URL_2);
        int primaryColor = 0xFF112233;
        NtpBackgroundDataThemeCollection themeData =
                new NtpBackgroundDataThemeCollection(
                        PlatformType.ANDROID,
                        info,
                        /* backgroundImageInfo= */ null,
                        /* bitmap= */ null,
                        primaryColor,
                        TEST_HASH);
        mNtpSyncedThemeManager.onThemeCommitted(themeData);
        verify(mNatives)
                .updateCustomBackgroundPrefsWithColor(
                        any(),
                        eq(JUnitTestGURLs.URL_1),
                        eq(TEST_COLLECTION_ID),
                        eq(attributionLine1),
                        eq(attributionLine2),
                        eq(JUnitTestGURLs.URL_2),
                        eq(primaryColor),
                        /* isDailyRefresh= */ eq(true));
    }

    @Test
    public void testDisabledByPolicy_doesNotAttach() {
        NtpCustomizationPolicyManager policyManager = mock(NtpCustomizationPolicyManager.class);
        when(policyManager.isNtpCustomBackgroundEnabled()).thenReturn(false);
        NtpCustomizationPolicyManager.setInstanceForTesting(policyManager);
        NtpSyncedThemeManager manager =
                new NtpSyncedThemeManager(NtpCustomizationConfigManager.getInstance());

        manager.onFinishNativeInitialization(mProfile);

        verify(mNatives, never()).init(any(), any());
    }

    @Test
    public void testOnThemeCollectionImageAvailable_nullInfo_isNoOp() {
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();
        when(mNatives.getCustomBackgroundInfo(any())).thenReturn(null);

        bridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);

        verify(mImageFetcher, never()).fetchImage(any(), any());
        assertFalse(mNtpSyncedThemeManager.isImageDownloading());
    }
}
