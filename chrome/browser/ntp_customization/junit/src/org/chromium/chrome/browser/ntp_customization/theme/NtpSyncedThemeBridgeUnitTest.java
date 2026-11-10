// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.theme.NtpSyncedThemeBridge.SyncedBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataColor;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.PlatformType;
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
    @Mock private Profile mProfile;
    @Mock private NtpSyncedThemeBridge.Observer mObserver;
    private NtpSyncedThemeBridge mNtpSyncedThemeBridge;

    @Before
    public void setUp() {
        NtpSyncedThemeBridgeJni.setInstanceForTesting(mNatives);
        mNtpSyncedThemeBridge = NtpSyncedThemeBridge.create(mProfile);
        mNtpSyncedThemeBridge.setObserver(mObserver);
    }

    @Test
    public void testGetForProfile() {
        when(mNatives.getForProfile(mProfile)).thenReturn(mNtpSyncedThemeBridge);

        assertSame(mNtpSyncedThemeBridge, NtpSyncedThemeBridge.getForProfile(mProfile));
    }

    @Test
    public void testOnThemeCollectionUpdated_passesIsImageFromSyncThrough() {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        BACKGROUND_URL,
                        COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        when(mNatives.getCustomBackgroundInfo(mProfile))
                .thenReturn(new SyncedBackgroundInfo(info, /* primaryColor= */ null));
        mNtpSyncedThemeBridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);
        verify(mObserver)
                .onThemeCollectionImageAvailable(
                        info, /* primaryColor= */ null, /* isImageFromSync= */ true);

        when(mNatives.getCustomBackgroundInfo(mProfile))
                .thenReturn(new SyncedBackgroundInfo(info, PRIMARY_COLOR));
        mNtpSyncedThemeBridge.onThemeCollectionUpdated(/* isImageFromSync= */ false);
        verify(mObserver)
                .onThemeCollectionImageAvailable(info, PRIMARY_COLOR, /* isImageFromSync= */ false);
    }

    @Test
    public void testOnThemeCollectionUpdated_nullInfo() {
        when(mNatives.getCustomBackgroundInfo(mProfile)).thenReturn(null);
        mNtpSyncedThemeBridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);
        verify(mObserver)
                .onThemeCollectionImageAvailable(
                        /* info= */ null, /* primaryColor= */ null, /* isImageFromSync= */ true);
    }

    @Test
    public void testCallbacks_withoutObserver_areDropped() {
        mNtpSyncedThemeBridge.resetObserver();

        mNtpSyncedThemeBridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);
        mNtpSyncedThemeBridge.onChromeColorSynced(
                THEME_COLOR_ID, /* isDailyRefreshEnabled= */ false);
        mNtpSyncedThemeBridge.onDefaultThemeSynced();
        mNtpSyncedThemeBridge.onThemeSyncReady();

        verify(mNatives, never()).getCustomBackgroundInfo(any());
        verify(mObserver, never()).onThemeCollectionImageAvailable(any(), any(), anyBoolean());
        verify(mObserver, never()).onChromeColorSynced(anyInt(), anyBoolean());
        verify(mObserver, never()).onDefaultThemeSynced();
        verify(mObserver, never()).onThemeSyncReady();
    }

    @Test
    public void testFetchNextThemeCollectionImage() {
        mNtpSyncedThemeBridge.fetchNextThemeCollectionImage();
        verify(mNatives).fetchNextThemeCollectionImage(mProfile);
    }

    @Test
    public void testGetStoredContinuousThemeAndNotify() {
        mNtpSyncedThemeBridge.getStoredContinuousThemeAndNotify();

        verify(mNatives).getStoredContinuousThemeAndNotify(mProfile);
    }

    @Test
    public void testIsThemeSyncReady() {
        when(mNatives.isThemeSyncReady(mProfile)).thenReturn(true);
        assertTrue(mNtpSyncedThemeBridge.isThemeSyncReady());

        when(mNatives.isThemeSyncReady(mProfile)).thenReturn(false);
        assertFalse(mNtpSyncedThemeBridge.isThemeSyncReady());
    }

    @Test
    public void testObserverCallbacks() {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        BACKGROUND_URL,
                        COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        when(mNatives.getCustomBackgroundInfo(mProfile))
                .thenReturn(new SyncedBackgroundInfo(info, /* primaryColor= */ null));
        mNtpSyncedThemeBridge.onThemeCollectionUpdated(/* isImageFromSync= */ true);
        verify(mObserver)
                .onThemeCollectionImageAvailable(
                        info, /* primaryColor= */ null, /* isImageFromSync= */ true);

        mNtpSyncedThemeBridge.onChromeColorSynced(
                THEME_COLOR_ID, /* isDailyRefreshEnabled= */ true);
        verify(mObserver).onChromeColorSynced(THEME_COLOR_ID, /* isDailyRefreshEnabled= */ true);

        mNtpSyncedThemeBridge.onDefaultThemeSynced();
        verify(mObserver).onDefaultThemeSynced();

        mNtpSyncedThemeBridge.onThemeSyncReady();
        verify(mObserver).onThemeSyncReady();
    }

    @Test
    public void testOutboundSyncMethods() {
        mNtpSyncedThemeBridge.setChromeColor(
                new NtpBackgroundDataColor(
                        ApplicationProvider.getApplicationContext(),
                        PlatformType.ANDROID,
                        THEME_COLOR_ID,
                        /* isChromeColorDailyRefreshEnabled= */ true));
        verify(mNatives)
                .setChromeColor(mProfile, THEME_COLOR_ID, /* isDailyRefreshEnabled= */ true);

        mNtpSyncedThemeBridge.resetCustomBackgroundInfo();
        verify(mNatives).resetCustomBackgroundInfo(mProfile);

        mNtpSyncedThemeBridge.selectLocalBackgroundImage();
        verify(mNatives).selectLocalBackgroundImage(mProfile);

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
                        mProfile,
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
                        mProfile,
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
