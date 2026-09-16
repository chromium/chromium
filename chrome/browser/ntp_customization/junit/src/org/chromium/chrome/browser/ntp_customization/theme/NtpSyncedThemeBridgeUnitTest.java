// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo.NtpThemeColorId;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link NtpSyncedThemeBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpSyncedThemeBridgeUnitTest {
    public static final long NATIVE_NTP_SYNCED_THEME_BRIDGE = 1L;
    public static final GURL BACKGROUND_URL = JUnitTestGURLs.URL_1;
    public static final String COLLECTION_ID = "test_collection";
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
        when(mNatives.init(any(), any())).thenReturn(NATIVE_NTP_SYNCED_THEME_BRIDGE);
        mNtpSyncedThemeBridge = new NtpSyncedThemeBridge(mProfile, mObserver);
    }

    @Test
    public void testInitAndDestroy() {
        verify(mNatives).init(eq(mProfile), any(NtpSyncedThemeBridge.class));
        mNtpSyncedThemeBridge.destroy();
        verify(mNatives).destroy(NATIVE_NTP_SYNCED_THEME_BRIDGE);

        // Calling destroy() again should be a no-op.
        mNtpSyncedThemeBridge.destroy();
        verify(mNatives).destroy(NATIVE_NTP_SYNCED_THEME_BRIDGE);
    }

    @Test
    public void onCustomBackgroundImageUpdated() {
        CustomBackgroundInfo info =
                new CustomBackgroundInfo(
                        BACKGROUND_URL,
                        COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false);
        when(mNatives.getCustomBackgroundInfo(anyLong())).thenReturn(info);
        mNtpSyncedThemeBridge.onCustomBackgroundImageUpdated();
        verify(mObserver).onThemeCollectionSynced(info);
    }

    @Test
    public void testFetchNextThemeCollectionImage() {
        mNtpSyncedThemeBridge.fetchNextThemeCollectionImage();
        verify(mNatives).fetchNextThemeCollectionImage(NATIVE_NTP_SYNCED_THEME_BRIDGE);
    }

    @Test
    public void testIsProcessingSyncUpdate() {
        when(mNatives.isProcessingSyncUpdate(NATIVE_NTP_SYNCED_THEME_BRIDGE)).thenReturn(true);
        assertTrue(mNtpSyncedThemeBridge.isProcessingSyncUpdate());

        when(mNatives.isProcessingSyncUpdate(NATIVE_NTP_SYNCED_THEME_BRIDGE)).thenReturn(false);
        assertFalse(mNtpSyncedThemeBridge.isProcessingSyncUpdate());

        mNtpSyncedThemeBridge.destroy();
        assertFalse(mNtpSyncedThemeBridge.isProcessingSyncUpdate());
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
        when(mNatives.getCustomBackgroundInfo(anyLong())).thenReturn(info);
        bridge.onCustomBackgroundImageUpdated();
        verify(observer).onThemeCollectionSynced(info);

        bridge.onChromeColorSynced(THEME_COLOR_ID);
        verify(observer).onChromeColorSynced(THEME_COLOR_ID);

        bridge.onDefaultThemeSynced();
        verify(observer).onDefaultThemeSynced();
    }

    @Test
    public void testOutboundSyncMethods() {
        mNtpSyncedThemeBridge.setChromeColor(THEME_COLOR_ID);
        verify(mNatives).setChromeColor(NATIVE_NTP_SYNCED_THEME_BRIDGE, THEME_COLOR_ID);

        mNtpSyncedThemeBridge.resetCustomBackgroundInfo();
        verify(mNatives).resetCustomBackgroundInfo(NATIVE_NTP_SYNCED_THEME_BRIDGE);

        mNtpSyncedThemeBridge.selectLocalBackgroundImage();
        verify(mNatives).selectLocalBackgroundImage(NATIVE_NTP_SYNCED_THEME_BRIDGE);

        mNtpSyncedThemeBridge.updateCustomBackgroundPrefsWithColor(BACKGROUND_URL, PRIMARY_COLOR);
        verify(mNatives)
                .updateCustomBackgroundPrefsWithColor(
                        NATIVE_NTP_SYNCED_THEME_BRIDGE, BACKGROUND_URL, PRIMARY_COLOR);

        // Test with null primaryColor defaults to 0.
        mNtpSyncedThemeBridge.updateCustomBackgroundPrefsWithColor(
                BACKGROUND_URL, /* primaryColor= */ null);
        verify(mNatives)
                .updateCustomBackgroundPrefsWithColor(
                        NATIVE_NTP_SYNCED_THEME_BRIDGE, BACKGROUND_URL, 0);
    }

    @Test
    public void testCreateCustomBackgroundInfo() {
        String attribution = "Attribution 1,Attribution 2";
        CustomBackgroundInfo info =
                NtpSyncedThemeBridge.createCustomBackgroundInfo(
                        BACKGROUND_URL,
                        COLLECTION_ID,
                        /* isUploadedImage= */ false,
                        /* isDailyRefreshEnabled= */ false,
                        attribution);
        assertEquals(BACKGROUND_URL, info.backgroundUrl);
        assertEquals(COLLECTION_ID, info.collectionId);
        assertFalse(info.isUploadedImage);
        assertFalse(info.isDailyRefreshEnabled);
        assertEquals(attribution, info.attribution);
    }
}
