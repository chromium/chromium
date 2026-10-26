// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.jni_zero.JniUniquePtr;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.theme.NtpSyncedThemeBridge.NativeNtpSyncedThemeBridge;
import org.chromium.chrome.browser.ntp_customization.theme.NtpSyncedThemeBridge.SyncedBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link NtpSyncedThemeBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpSyncedThemeBridgeUnitTest {
    public static final GURL BACKGROUND_URL = JUnitTestGURLs.URL_1;
    public static final String COLLECTION_ID = "test_collection";
    public static final String ATTRIBUTION_LINE_1 = "Attribution 1";
    public static final String ATTRIBUTION_LINE_2 = "Attribution 2";
    public static final GURL ATTRIBUTION_ACTION_URL = JUnitTestGURLs.URL_2;
    public static final @NtpThemeColorId int THEME_COLOR_ID = NtpThemeColorId.NTP_COLORS_GREEN;
    public static final int PRIMARY_COLOR = 0xFF123456;
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private NtpSyncedThemeBridge.Natives mNatives;
    @Mock private JniUniquePtr<NativeNtpSyncedThemeBridge> mNativeNtpSyncedThemeBridge;
    @Mock private Profile mProfile;
    @Mock private NtpSyncedThemeBridge.Observer mObserver;
    private NtpSyncedThemeBridge mNtpSyncedThemeBridge;

    @Before
    public void setUp() {
        NtpSyncedThemeBridgeJni.setInstanceForTesting(mNatives);
        when(mNatives.init(any(), any())).thenReturn(mNativeNtpSyncedThemeBridge);
        mNtpSyncedThemeBridge = new NtpSyncedThemeBridge(mProfile, mObserver);
    }

    @Test
    public void testInitAndDestroy() {
        verify(mNatives).init(eq(mProfile), any(NtpSyncedThemeBridge.class));
        mNtpSyncedThemeBridge.destroy();
        verify(mNativeNtpSyncedThemeBridge).destroy();

        // Calling destroy() again should be a no-op.
        mNtpSyncedThemeBridge.destroy();
        verify(mNativeNtpSyncedThemeBridge).destroy();
    }

    @Test
    public void testOnThemeCollectionUpdated_passesIsImageFromSyncThrough() {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        BACKGROUND_URL,
                        COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        when(mNatives.getCustomBackgroundInfo(any()))
                .thenReturn(new SyncedBackgroundInfo(info, /* primaryColor= */ null));
        mNtpSyncedThemeBridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);
        verify(mObserver)
                .onThemeCollectionImageAvailable(
                        info, /* primaryColor= */ null, /* isImageFromSync= */ true);

        when(mNatives.getCustomBackgroundInfo(any()))
                .thenReturn(new SyncedBackgroundInfo(info, PRIMARY_COLOR));
        mNtpSyncedThemeBridge.onThemeCollectionUpdated(/* isImageFromSync= */ false);
        verify(mObserver)
                .onThemeCollectionImageAvailable(info, PRIMARY_COLOR, /* isImageFromSync= */ false);
    }

    @Test
    public void testOnThemeCollectionUpdated_nullInfo() {
        when(mNatives.getCustomBackgroundInfo(any())).thenReturn(null);
        mNtpSyncedThemeBridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);
        verify(mObserver)
                .onThemeCollectionImageAvailable(
                        /* info= */ null, /* primaryColor= */ null, /* isImageFromSync= */ true);
    }

    @Test
    public void testOnThemeCollectionUpdated_afterDestroy_isNoOp() {
        mNtpSyncedThemeBridge.destroy();
        mNtpSyncedThemeBridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);
        verify(mNatives, never()).getCustomBackgroundInfo(any());
        verify(mObserver, never()).onThemeCollectionImageAvailable(any(), any(), anyBoolean());
    }

    @Test
    public void testFetchNextThemeCollectionImage() {
        mNtpSyncedThemeBridge.fetchNextThemeCollectionImage();
        verify(mNatives).fetchNextThemeCollectionImage(mNativeNtpSyncedThemeBridge);
    }

    @Test
    public void testObserverCallbacks() {
        NtpSyncedThemeBridge.Observer observer = mock(NtpSyncedThemeBridge.Observer.class);
        NtpSyncedThemeBridge bridge = new NtpSyncedThemeBridge(mProfile, observer);

        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        BACKGROUND_URL,
                        COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        when(mNatives.getCustomBackgroundInfo(any()))
                .thenReturn(new SyncedBackgroundInfo(info, /* primaryColor= */ null));
        bridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);
        verify(observer)
                .onThemeCollectionImageAvailable(
                        info, /* primaryColor= */ null, /* isImageFromSync= */ true);

        bridge.onChromeColorSynced(THEME_COLOR_ID);
        verify(observer).onChromeColorSynced(THEME_COLOR_ID);

        bridge.onDefaultThemeSynced();
        verify(observer).onDefaultThemeSynced();
    }

    @Test
    public void testOutboundSyncMethods() {
        mNtpSyncedThemeBridge.setChromeColor(THEME_COLOR_ID);
        verify(mNatives).setChromeColor(mNativeNtpSyncedThemeBridge, THEME_COLOR_ID);

        mNtpSyncedThemeBridge.resetCustomBackgroundInfo();
        verify(mNatives).resetCustomBackgroundInfo(mNativeNtpSyncedThemeBridge);

        mNtpSyncedThemeBridge.selectLocalBackgroundImage();
        verify(mNatives).selectLocalBackgroundImage(mNativeNtpSyncedThemeBridge);

        mNtpSyncedThemeBridge.updateCustomBackgroundPrefsWithColor(
                BACKGROUND_URL,
                COLLECTION_ID,
                ATTRIBUTION_LINE_1,
                ATTRIBUTION_LINE_2,
                ATTRIBUTION_ACTION_URL,
                PRIMARY_COLOR,
                /* isDailyRefresh= */ true);
        verify(mNatives)
                .updateCustomBackgroundPrefsWithColor(
                        mNativeNtpSyncedThemeBridge,
                        BACKGROUND_URL,
                        COLLECTION_ID,
                        ATTRIBUTION_LINE_1,
                        ATTRIBUTION_LINE_2,
                        ATTRIBUTION_ACTION_URL,
                        PRIMARY_COLOR,
                        /* isDailyRefresh= */ true);

        // Test with null primaryColor defaults to 0.
        mNtpSyncedThemeBridge.updateCustomBackgroundPrefsWithColor(
                BACKGROUND_URL,
                COLLECTION_ID,
                ATTRIBUTION_LINE_1,
                ATTRIBUTION_LINE_2,
                ATTRIBUTION_ACTION_URL,
                /* primaryColor= */ null,
                /* isDailyRefresh= */ true);
        verify(mNatives)
                .updateCustomBackgroundPrefsWithColor(
                        mNativeNtpSyncedThemeBridge,
                        BACKGROUND_URL,
                        COLLECTION_ID,
                        ATTRIBUTION_LINE_1,
                        ATTRIBUTION_LINE_2,
                        ATTRIBUTION_ACTION_URL,
                        /* primaryColor= */ 0,
                        /* isDailyRefresh= */ true);
    }

    @Test
    public void testCreateCustomBackgroundInfo() {
        SyncedBackgroundInfo syncedBackgroundInfo =
                NtpSyncedThemeBridge.createCustomBackgroundInfo(
                        BACKGROUND_URL,
                        COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        ATTRIBUTION_LINE_1,
                        ATTRIBUTION_LINE_2,
                        ATTRIBUTION_ACTION_URL,
                        /* mainColor= */ 0);
        CustomBackgroundInfo info = syncedBackgroundInfo.info;
        assertEquals(BACKGROUND_URL, info.backgroundUrl);
        assertEquals(COLLECTION_ID, info.collectionId);
        assertFalse(info.isUploadedImage);
        assertFalse(info.isDailyRefreshEnabled);
        assertEquals(ATTRIBUTION_LINE_1, info.attributionLine1);
        assertEquals(ATTRIBUTION_LINE_2, info.attributionLine2);
        assertEquals(ATTRIBUTION_ACTION_URL, info.attributionActionUrl);
        assertNull(syncedBackgroundInfo.primaryColor);

        SyncedBackgroundInfo infoWithNonZeroColor =
                NtpSyncedThemeBridge.createCustomBackgroundInfo(
                        BACKGROUND_URL,
                        COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        ATTRIBUTION_LINE_1,
                        ATTRIBUTION_LINE_2,
                        ATTRIBUTION_ACTION_URL,
                        PRIMARY_COLOR);
        assertEquals(Integer.valueOf(PRIMARY_COLOR), infoWithNonZeroColor.primaryColor);
    }
}
