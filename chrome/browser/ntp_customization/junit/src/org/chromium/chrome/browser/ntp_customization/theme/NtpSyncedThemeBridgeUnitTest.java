// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ntp_customization.theme;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.CustomBackgroundInfo;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link NtpSyncedThemeBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NtpSyncedThemeBridgeUnitTest {
    public static final long NATIVE_NTP_SYNCED_THEME_BRIDGE = 1L;
    public static final GURL BACKGROUND_URL = JUnitTestGURLs.URL_1;
    public static final String COLLECTION_ID = "test_collection";
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private NtpSyncedThemeBridge.Natives mNatives;
    @Mock private Profile mProfile;
    @Mock private Callback<CustomBackgroundInfo> mCallback;
    private NtpSyncedThemeBridge mNtpSyncedThemeBridge;

    @Before
    public void setUp() {
        NtpSyncedThemeBridgeJni.setInstanceForTesting(mNatives);
        when(mNatives.init(any(), any())).thenReturn(NATIVE_NTP_SYNCED_THEME_BRIDGE);
        mNtpSyncedThemeBridge = new NtpSyncedThemeBridge(mProfile, mCallback);
    }

    @Test
    public void testInitAndDestroy() {
        verify(mNatives).init(eq(mProfile), any(NtpSyncedThemeBridge.class));
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
        verify(mCallback).onResult(info);
    }
}
