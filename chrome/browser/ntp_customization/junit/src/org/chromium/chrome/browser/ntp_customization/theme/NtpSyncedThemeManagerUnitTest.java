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
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundType.THEME_COLLECTION;

import android.content.Context;
import android.graphics.Bitmap;

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
import org.chromium.chrome.browser.ntp_customization.theme.NtpSyncedThemeBridge.SyncedBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.CrossDeviceThemeTracker;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataThemeCollection;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataUploadImage;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.List;

/** Unit tests for {@link NtpSyncedThemeManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION_V2})
public class NtpSyncedThemeManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Profile mProfile;
    @Mock private Profile mOtherProfile;
    @Mock private ImageFetcher mImageFetcher;
    @Mock private NtpSyncedThemeBridge.Natives mNatives;
    @Mock private CrossDeviceThemeTracker.Natives mCrossDeviceThemeTrackerNatives;
    @Captor private ArgumentCaptor<Callback<Bitmap>> mBitmapCallbackCaptor;

    private static final String TEST_COLLECTION_ID = "collectionId";
    private static final String TEST_HASH = "hash";

    private NtpSyncedThemeManager mNtpSyncedThemeManager;
    private NtpCustomizationConfigManager mConfigManagerSpy;
    private Context mContext;
    // The bridges native hands out for mProfile and mOtherProfile.
    private NtpSyncedThemeBridge mBridge;
    private NtpSyncedThemeBridge mOtherBridge;

    @Before
    public void setUp() {
        // The manager uses ContextUtils.getApplicationContext(), which Robolectric wraps.
        mContext = ContextUtils.getApplicationContext();
        NtpCustomizationUtils.setImageFetcherForTesting(mImageFetcher);
        NtpSyncedThemeBridgeJni.setInstanceForTesting(mNatives);
        CrossDeviceThemeTracker.setInstanceForTesting(mCrossDeviceThemeTrackerNatives);
        mBridge = NtpSyncedThemeBridge.create(mProfile);
        mOtherBridge = NtpSyncedThemeBridge.create(mOtherProfile);
        when(mNatives.getForProfile(mProfile)).thenReturn(mBridge);
        when(mNatives.getForProfile(mOtherProfile)).thenReturn(mOtherBridge);
        NtpCustomizationUtils.resetSharedPreferenceForTesting();
        mConfigManagerSpy = spy(new NtpCustomizationConfigManager());
        NtpCustomizationConfigManager.setInstanceForTesting(mConfigManagerSpy);
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

        // 3. Verify the fetch is requested from the profile's bridge.
        verify(mNatives).fetchNextThemeCollectionImage(mProfile);

        // 4. Simulate native callback with the info for the next day's image.
        CustomBackgroundInfo nextInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_2,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true);
        when(mNatives.getCustomBackgroundInfo(any()))
                .thenReturn(new SyncedBackgroundInfo(nextInfo, /* primaryColor= */ null));
        mBridge.onThemeCollectionUpdated(/* isImageFromSync= */ false);

        // 5. Verify image is fetched.
        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mBitmapCallbackCaptor.getValue().onResult(bitmap);

        RobolectricUtil.runAllBackgroundAndUi();

        // 6. Verify daily refresh info is saved.
        assertTrue(NtpCustomizationUtils.createDailyRefreshBackgroundImageFile().exists());
        assertNotNull(
                NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference());
        assertNotNull(NtpCustomizationUtils.readDailyRefreshNtpBackgroundImageInfo());
        assertNotNull(
                NtpCustomizationUtils.getDailyRefreshCustomizedPrimaryColorFromSharedPreference());
    }

    @Test
    public void testDestroy() {
        mNtpSyncedThemeManager = createInitializedManager();
        mNtpSyncedThemeManager.destroy();
        verify(mImageFetcher).destroy();

        // The bridge outlives the manager's attachment and stops delivering to it.
        mBridge.onDefaultThemeSynced();
        verify(mConfigManagerSpy, never()).onSyncedDefaultThemeReset(any());
    }

    @Test
    public void testOnProfileDestroyed_releasesProfileState() {
        mNtpSyncedThemeManager = createInitializedManager();

        // Attaching registered the manager, so the notification reaches it via ProfileManager.
        ProfileManager.onProfileDestroyed(mProfile);

        verify(mImageFetcher).destroy();
        mBridge.onDefaultThemeSynced();
        verify(mConfigManagerSpy, never()).onSyncedDefaultThemeReset(any());
        clearInvocations(mNatives);
        mNtpSyncedThemeManager.onThemeCommitted(/* data= */ null);
        verify(mNatives, never()).resetCustomBackgroundInfo(any());
    }

    @Test
    public void testOnProfileDestroyed_otherProfile_keepsProfileState() {
        mNtpSyncedThemeManager = createInitializedManager();

        ProfileManager.onProfileDestroyed(mOtherProfile);

        verify(mImageFetcher, never()).destroy();
        mNtpSyncedThemeManager.onThemeCommitted(/* data= */ null);
        verify(mNatives).resetCustomBackgroundInfo(mProfile);
    }

    @Test
    public void testOnFinishNativeInitialization_sameProfile_initializesOnce() {
        mNtpSyncedThemeManager = createInitializedManager();
        mNtpSyncedThemeManager.onFinishNativeInitialization(mProfile);

        verify(mNatives).getForProfile(mProfile);
        verify(mNatives).getStoredContinuousThemeAndNotify(mProfile);
        verify(mImageFetcher, never()).destroy();
    }

    @Test
    public void testOnFinishNativeInitialization_differentProfile_destroysPreviousState() {
        mNtpSyncedThemeManager = createInitializedManager();
        mNtpSyncedThemeManager.onFinishNativeInitialization(mOtherProfile);

        verify(mNatives).getStoredContinuousThemeAndNotify(mProfile);
        verify(mImageFetcher).destroy();
        verify(mNatives).getStoredContinuousThemeAndNotify(mOtherProfile);

        // The previous profile's bridge no longer reaches the manager.
        mBridge.onDefaultThemeSynced();
        verify(mConfigManagerSpy, never()).onSyncedDefaultThemeReset(any());

        // Theme commits reach the new profile's bridge.
        NtpCustomizationConfigManager.getInstance()
                .onBackgroundDataChanged(
                        mContext,
                        /* backgroundData= */ null,
                        /* shouldNotifySyncedThemeManager= */ true);
        verify(mNatives).resetCustomBackgroundInfo(mOtherProfile);
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

        verify(mNatives, never()).getForProfile(any());
        verify(mNatives, never()).fetchNextThemeCollectionImage(any());
        verify(mNatives, never()).resetCustomBackgroundInfo(any());
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
        verify(mConfigManagerSpy)
                .onSyncedThemeCollectionImageChanged(eq(mContext), themeCollectionCaptor.capture());
        NtpBackgroundDataThemeCollection themeCollectionData = themeCollectionCaptor.getValue();
        assertNotNull(themeCollectionData);
        return themeCollectionData;
    }

    private NtpSyncedThemeBridge initSyncedThemeManagerAndGetBridge() {
        mNtpSyncedThemeManager = createInitializedManager();
        return mBridge;
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
    public void testOnThemeCollectionUpdated_downloadFailure_clearsDownloadingAndNotifiesReady() {
        when(mNatives.isThemeSyncReady(any())).thenReturn(true);
        setupImageDownloading();
        Runnable readinessCallback = mock(Runnable.class);
        mNtpSyncedThemeManager.addOneShotReadinessCallback(readinessCallback);

        mBitmapCallbackCaptor.getValue().onResult(null);

        assertFalse(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());
        verify(mConfigManagerSpy, never()).onSyncedThemeCollectionImageChanged(any(), any());
        verify(readinessCallback).run();
    }

    @Test
    public void testDailyRefresh_downloadFailure_doesNotSave() {
        setupDailyRefreshImageFetching();

        mBitmapCallbackCaptor.getValue().onResult(null);

        assertFalse(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());
        assertNull(NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference());
    }

    /** Starts pre-fetching the next daily refresh image and returns the bridge. */
    private NtpSyncedThemeBridge setupDailyRefreshImageFetching() {
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();
        stubNativeBackground(JUnitTestGURLs.URL_2, /* isDailyRefreshEnabled= */ true);

        bridge.onThemeCollectionUpdated(/* isImageFromSync= */ false);

        assertFalse(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());

        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        return bridge;
    }

    /** Starts downloading a synced image and returns the bridge. */
    private NtpSyncedThemeBridge setupImageDownloading() {
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();
        stubNativeBackground(JUnitTestGURLs.URL_1, /* isDailyRefreshEnabled= */ false);

        bridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);

        assertTrue(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());

        verify(mImageFetcher).fetchImage(any(), mBitmapCallbackCaptor.capture());
        return bridge;
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
                        eq(/* isDailyRefresh= */ true));
    }

    @Test
    public void testDisabledByPolicy_doesNotAttach() {
        NtpCustomizationPolicyManager policyManager = mock(NtpCustomizationPolicyManager.class);
        when(policyManager.isNtpCustomBackgroundEnabled()).thenReturn(false);
        NtpCustomizationPolicyManager.setInstanceForTesting(policyManager);
        NtpSyncedThemeManager manager =
                new NtpSyncedThemeManager(NtpCustomizationConfigManager.getInstance());

        manager.onFinishNativeInitialization(mProfile);

        verify(mNatives, never()).getForProfile(any());
    }

    @Test
    public void testOnThemeCollectionImageAvailable_alreadyActive_notDownloaded() {
        // The same theme collection image with the same color is persisted on this device.
        NtpSyncedThemeBridge bridge = persistThemeCollectionAndGetBridge();

        // E.g. the stored synced theme delivered when the bridge attaches at startup.
        bridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);

        verify(mImageFetcher, never()).fetchImage(any(), any());
        assertFalse(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());
        verify(mConfigManagerSpy, never()).onSyncedThemeCollectionImageChanged(any(), any());
    }

    @Test
    public void testSyncDownload_supersededBySyncOfActiveTheme() {
        // A second synced theme arrives which this device already shows. The first download is
        // superseded even though nothing new is downloaded.
        verifySyncDownloadSuperseded(
                bridge -> {
                    CustomBackgroundInfo persistedInfo =
                            stubNativeBackground(
                                    JUnitTestGURLs.URL_2, /* isDailyRefreshEnabled= */ false);
                    NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(THEME_COLLECTION);
                    NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(persistedInfo);
                    bridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);
                });

        verify(mImageFetcher).fetchImage(any(), any());
    }

    @Test
    public void testOnThemeCollectionImageAvailable_nullInfo_isNoOp() {
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();
        when(mNatives.getCustomBackgroundInfo(any())).thenReturn(null);

        bridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);

        verify(mImageFetcher, never()).fetchImage(any(), any());
        assertFalse(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());
    }

    // Tests for the in-flight download tracking.

    @Test
    public void testSyncDownload_supersededByLocalCommit() {
        verifySyncDownloadSuperseded(
                bridge -> mNtpSyncedThemeManager.onThemeCommitted(/* data= */ null));
    }

    @Test
    public void testSyncDownload_supersededBySyncedChromeColor() {
        verifySyncDownloadSuperseded(
                bridge -> bridge.onChromeColorSynced(NtpThemeColorId.NTP_COLORS_BLUE));

        verify(mConfigManagerSpy).onSyncedChromeColorChanged(eq(mContext), any());
    }

    @Test
    public void testSyncDownload_supersededBySyncedDefaultTheme() {
        verifySyncDownloadSuperseded(NtpSyncedThemeBridge::onDefaultThemeSynced);

        verify(mConfigManagerSpy).onSyncedDefaultThemeReset(mContext);
    }

    @Test
    public void testSyncDownload_supersededBySyncedDailyRefresh() {
        verifySyncDownloadSuperseded(
                bridge -> {
                    stubNativeBackground(JUnitTestGURLs.URL_2, /* isDailyRefreshEnabled= */ true);
                    bridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);
                });

        // The synced daily refresh isn't downloaded, so only the first download was started.
        verify(mImageFetcher).fetchImage(any(), any());
    }

    @Test
    public void testSyncDownload_supersededByDestroy() {
        verifySyncDownloadSuperseded(bridge -> mNtpSyncedThemeManager.destroy());
    }

    @Test
    public void testSyncDownload_supersededByNewerSync() {
        NtpSyncedThemeBridge bridge = setupImageDownloading();

        // A second synced theme arrives while the first one is downloading, even sharing the same
        // image URL.
        CustomBackgroundInfo newerInfo =
                stubNativeBackground(JUnitTestGURLs.URL_1, /* isDailyRefreshEnabled= */ false);
        bridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);

        assertTrue(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());
        verify(mImageFetcher, times(2)).fetchImage(any(), mBitmapCallbackCaptor.capture());
        // The captor already holds the first callback from setupImageDownloading().
        List<Callback<Bitmap>> callbacks = mBitmapCallbackCaptor.getAllValues();
        Callback<Bitmap> firstCallback = callbacks.get(callbacks.size() - 2);
        Callback<Bitmap> secondCallback = callbacks.get(callbacks.size() - 1);

        // The first result is dropped despite matching the URL, and the second one is applied.
        completeDownload(firstCallback);
        verify(mConfigManagerSpy, never()).onSyncedThemeCollectionImageChanged(any(), any());
        assertTrue(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());

        completeDownload(secondCallback);
        ArgumentCaptor<NtpBackgroundDataThemeCollection> dataCaptor =
                ArgumentCaptor.forClass(NtpBackgroundDataThemeCollection.class);
        verify(mConfigManagerSpy)
                .onSyncedThemeCollectionImageChanged(eq(mContext), dataCaptor.capture());
        assertEquals(newerInfo, dataCaptor.getValue().getCustomBackgroundInfo());
        assertFalse(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());
    }

    @Test
    public void testDailyRefreshPrefetch_supersededByLocalCommit() {
        setDailyRefreshThemeCollectionToSharedPreference();
        setupDailyRefreshImageFetching();

        // The user selects a theme locally while the next image is downloading.
        mNtpSyncedThemeManager.onThemeCommitted(/* data= */ null);

        // The SharedPreferences still describe the daily refresh collection in this test, so only
        // the invalidation prevents the stale image from being saved.
        completeDownload(mBitmapCallbackCaptor.getValue());
        assertNull(NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference());
        assertFalse(NtpCustomizationUtils.createDailyRefreshBackgroundImageFile().exists());
    }

    @Test
    public void testDailyRefreshPrefetch_supersededBySyncOfSameUrl() {
        // The next daily refresh image is downloading when another device selects that very image
        // as a static theme, so both downloads share the URL.
        setDailyRefreshThemeCollectionToSharedPreference();
        NtpSyncedThemeBridge bridge = setupDailyRefreshImageFetching();
        CustomBackgroundInfo syncedInfo =
                stubNativeBackground(JUnitTestGURLs.URL_2, /* isDailyRefreshEnabled= */ false);
        bridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);
        assertTrue(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());
        verify(mImageFetcher, times(2)).fetchImage(any(), mBitmapCallbackCaptor.capture());
        List<Callback<Bitmap>> callbacks = mBitmapCallbackCaptor.getAllValues();
        Callback<Bitmap> prefetchCallback = callbacks.get(callbacks.size() - 2);
        Callback<Bitmap> syncCallback = callbacks.get(callbacks.size() - 1);

        // The pre-fetched image is dropped and must not be mistaken for the sync download.
        completeDownload(prefetchCallback);
        assertNull(NtpCustomizationUtils.getDailyRefreshCustomBackgroundInfoFromSharedPreference());
        assertTrue(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());

        // The synced image is applied.
        completeDownload(syncCallback);
        ArgumentCaptor<NtpBackgroundDataThemeCollection> dataCaptor =
                ArgumentCaptor.forClass(NtpBackgroundDataThemeCollection.class);
        verify(mConfigManagerSpy)
                .onSyncedThemeCollectionImageChanged(eq(mContext), dataCaptor.capture());
        assertEquals(syncedInfo, dataCaptor.getValue().getCustomBackgroundInfo());
        assertFalse(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());
    }

    @Test
    public void testDailyRefreshPrefetch_ignoredDuringSyncDownload() {
        NtpSyncedThemeBridge bridge = setupImageDownloading();

        // The next image pre-fetched for the local daily refresh arrives while the synced image is
        // still downloading. The synced image is the newer choice, so the pre-fetch is ignored.
        stubNativeBackground(JUnitTestGURLs.URL_2, /* isDailyRefreshEnabled= */ true);
        bridge.onThemeCollectionUpdated(/* isImageFromSync= */ false);

        verify(mImageFetcher).fetchImage(any(), any());
        assertTrue(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());

        // The synced image is still applied.
        completeDownload(mBitmapCallbackCaptor.getValue());
        verify(mConfigManagerSpy).onSyncedThemeCollectionImageChanged(eq(mContext), any());
        assertFalse(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());
    }

    @Test
    public void testReadinessCallback_notReadyUntilOnThemeSyncReady() {
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();
        when(mNatives.isThemeSyncReady(any())).thenReturn(false);
        assertFalse(mNtpSyncedThemeManager.isSyncedThemeReady());

        Runnable readinessCallback = mock(Runnable.class);
        mNtpSyncedThemeManager.addOneShotReadinessCallback(readinessCallback);
        verify(readinessCallback, never()).run();

        when(mNatives.isThemeSyncReady(any())).thenReturn(true);
        bridge.onThemeSyncReady();

        assertTrue(mNtpSyncedThemeManager.isSyncedThemeReady());
        verify(readinessCallback).run();
    }

    @Test
    public void testReadinessCallback_waitsForInFlightSyncImageDownloadAndStagesThemeFirst() {
        when(mNatives.isThemeSyncReady(any())).thenReturn(false);

        // MergeFullSyncData triggers OnThemeCollectionUpdated first (via setupImageDownloading),
        // starting the bitmap download, and then triggers OnThemeSyncReady immediately after.
        NtpSyncedThemeBridge bridge = setupImageDownloading();
        when(mNatives.isThemeSyncReady(any())).thenReturn(true);
        bridge.onThemeSyncReady();

        // Because the image download is still in flight, isSyncedThemeReady() must remain false.
        assertFalse(mNtpSyncedThemeManager.isSyncedThemeReady());
        Runnable readinessCallback =
                Mockito.mock(
                        Runnable.class,
                        invocation -> {
                            // When the readiness callback runs, the theme must already be staged in
                            // NtpCustomizationConfigManager.
                            verify(mConfigManagerSpy)
                                    .onSyncedThemeCollectionImageChanged(eq(mContext), any());
                            return null;
                        });
        mNtpSyncedThemeManager.addOneShotReadinessCallback(readinessCallback);
        verify(readinessCallback, never()).run();

        completeDownload(mBitmapCallbackCaptor.getValue());

        assertTrue(mNtpSyncedThemeManager.isSyncedThemeReady());
        verify(readinessCallback).run();
    }

    @Test
    public void testReadinessCallback_alreadyReady_runsImmediatelyAndRemoveWorks() {
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();
        when(mNatives.isThemeSyncReady(any())).thenReturn(true);
        assertTrue(mNtpSyncedThemeManager.isSyncedThemeReady());

        Runnable immediateCallback = mock(Runnable.class);
        mNtpSyncedThemeManager.addOneShotReadinessCallback(immediateCallback);
        verify(immediateCallback).run();

        when(mNatives.isThemeSyncReady(any())).thenReturn(false);
        Runnable removedCallback = mock(Runnable.class);
        mNtpSyncedThemeManager.addOneShotReadinessCallback(removedCallback);
        mNtpSyncedThemeManager.removeReadinessCallback(removedCallback);

        when(mNatives.isThemeSyncReady(any())).thenReturn(true);
        bridge.onThemeSyncReady();
        verify(removedCallback, never()).run();
    }

    /**
     * Starts a synced image download, lets {@code supersede} supersede it, and checks that the
     * in-flight download state is cleared and the stale result is dropped when it arrives.
     */
    private void verifySyncDownloadSuperseded(Callback<NtpSyncedThemeBridge> supersede) {
        NtpSyncedThemeBridge bridge = setupImageDownloading();

        supersede.onResult(bridge);

        assertFalse(mNtpSyncedThemeManager.isContinuousSyncedImageDownloading());

        completeDownload(mBitmapCallbackCaptor.getValue());
        verify(mConfigManagerSpy, never()).onSyncedThemeCollectionImageChanged(any(), any());
    }

    /** Makes native report the given image of {@link #TEST_COLLECTION_ID} as the background. */
    private CustomBackgroundInfo stubNativeBackground(GURL url, boolean isDailyRefreshEnabled) {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        url,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        isDailyRefreshEnabled);
        when(mNatives.getCustomBackgroundInfo(any()))
                .thenReturn(new SyncedBackgroundInfo(info, /* primaryColor= */ null));
        return info;
    }

    /** Persists {@link #TEST_COLLECTION_ID} with daily refresh as the background. */
    private static void setDailyRefreshThemeCollectionToSharedPreference() {
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(THEME_COLLECTION);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ true));
    }

    /** Completes the given image download and runs the tasks it posts. */
    private static void completeDownload(Callback<Bitmap> callback) {
        callback.onResult(Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888));
        RobolectricUtil.runAllBackgroundAndUi();
    }

    /**
     * Persists a static theme collection and makes native report the same image and color as the
     * synced theme.
     */
    private NtpSyncedThemeBridge persistThemeCollectionAndGetBridge() {
        int primaryColor = 0xFF112233;
        CustomBackgroundInfo syncedInfo =
                new CustomBackgroundInfo(
                        JUnitTestGURLs.URL_1,
                        TEST_COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        NtpCustomizationUtils.setNtpBackgroundTypeToSharedPreference(THEME_COLLECTION);
        NtpCustomizationUtils.setCustomBackgroundInfoToSharedPreference(syncedInfo);
        NtpCustomizationUtils.setCustomizedPrimaryColorToSharedPreference(primaryColor);
        NtpSyncedThemeBridge bridge = initSyncedThemeManagerAndGetBridge();
        when(mNatives.getCustomBackgroundInfo(any()))
                .thenReturn(new SyncedBackgroundInfo(syncedInfo, primaryColor));
        return bridge;
    }
}
