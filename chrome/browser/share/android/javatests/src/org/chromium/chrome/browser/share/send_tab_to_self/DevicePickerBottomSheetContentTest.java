// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.Arrays;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DevicePickerBottomSheetContentTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    private WebContents mWebContents;

    @Mock private SendTabToSelfAndroidBridge.Natives mNativeMock;
    @Mock private IdentityManager mIdentityManager;
    private CoreAccountInfo mCoreAccountInfo;
    private AccountInfo mAccountInfo;

    private Activity mContext;
    private List<TargetDeviceInfo> mDevices;

    @Before
    public void setUp() {
        mWebContents = new MockWebContents();

        IdentityServicesProvider identityServicesProvider = mock(IdentityServicesProvider.class);
        IdentityServicesProvider.setInstanceForTests(identityServicesProvider);
        when(identityServicesProvider.getIdentityManager(any())).thenReturn(mIdentityManager);

        mCoreAccountInfo =
                CoreAccountInfo.createFromEmailAndGaiaId("test@example.com", new GaiaId("test_id"));
        mAccountInfo = new AccountInfo.Builder(mCoreAccountInfo).build();

        when(mIdentityManager.getPrimaryAccountInfo()).thenReturn(mCoreAccountInfo);
        when(mIdentityManager.findExtendedAccountInfoByAccountId(any())).thenReturn(mAccountInfo);

        mContext = Robolectric.buildActivity(Activity.class).create().get();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);

        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mNativeMock);

        mDevices =
                Arrays.asList(
                        new TargetDeviceInfo(
                                "Pixel 10", "guid", FormFactor.DESKTOP, "Active today"));

        when(mTab.getWebContents()).thenReturn(mWebContents);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testOnItemClick() {
        DevicePickerBottomSheetContent content =
                new DevicePickerBottomSheetContent(
                        mContext,
                        "https://example.com/",
                        "Title",
                        mBottomSheetController,
                        mDevices,
                        mProfile,
                        () -> mTab);
        content.onItemClick(null, null, 0, 0);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq("guid"),
                        eq("https://example.com/"),
                        eq("Title"),
                        any());
        verify(mBottomSheetController).hideContent(content, true);
    }


}
