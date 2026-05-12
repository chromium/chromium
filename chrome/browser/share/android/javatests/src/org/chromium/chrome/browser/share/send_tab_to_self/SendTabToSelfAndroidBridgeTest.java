// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.mock.MockWebContents;

import java.util.List;

/** Tests for SendTabToSelfAndroidBridge */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowToast.class})
public class SendTabToSelfAndroidBridgeTest {
    private static final String URL = "https://www.google.com";
    private static final String TITLE = "Google";
    private static final String TARGET_DEVICE_SYNC_CACHE_GUID = "device_guid";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SendTabToSelfAndroidBridge.Natives mNativeMock;
    @Mock private Profile mProfile;
    private WebContents mWebContents;

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.getApplication());
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mNativeMock);
        mWebContents = new MockWebContents();
    }

    @Test
    @SmallTest
    public void testSendTabToDevice() {
        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                mWebContents,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "device_name",
                URL,
                TITLE,
                null);
        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        any());
    }

    @Test
    @SmallTest
    public void testMarkEntryOpened() {
        String guid = "guid";
        SendTabToSelfAndroidBridge.markEntryOpened(mProfile, guid);
        verify(mNativeMock).markEntryOpened(eq(mProfile), eq(guid));
    }

    @Test
    @SmallTest
    public void testDismissEntry() {
        String guid = "guid";
        SendTabToSelfAndroidBridge.dismissEntry(mProfile, guid);
        verify(mNativeMock).dismissEntry(eq(mProfile), eq(guid));
    }

    @Test
    @SmallTest
    @SuppressWarnings("unchecked")
    public void testGetAllTargetDeviceInfos() {
        List<TargetDeviceInfo> expected =
                List.of(
                        new TargetDeviceInfo("name1", "guid1", FormFactor.DESKTOP, "Active today"),
                        new TargetDeviceInfo("name2", "guid2", FormFactor.DESKTOP, "Active today"),
                        new TargetDeviceInfo("name3", "guid3", FormFactor.PHONE, "Active today"));
        when(mNativeMock.getAllTargetDeviceInfos(eq(mProfile))).thenReturn(expected);

        List<TargetDeviceInfo> actual =
                SendTabToSelfAndroidBridge.getAllTargetDeviceInfos(mProfile);

        verify(mNativeMock).getAllTargetDeviceInfos(eq(mProfile));
        Assert.assertEquals(3, actual.size());
        Assert.assertEquals(expected, actual);
    }


    @Test
    @SmallTest
    public void testGetEntryPointDisplayReason() {
        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(mProfile, URL);
        verify(mNativeMock).getEntryPointDisplayReason(eq(mProfile), eq(URL));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsSuccessToast() {
        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback>
                confirmationCallbackCaptor =
                        ArgumentCaptor.forClass(
                                SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);

        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                mWebContents,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "Pixel 10",
                URL,
                TITLE,
                null);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        confirmationCallbackCaptor.capture());

        confirmationCallbackCaptor.getValue().onResult(SendTabToSelfResult.SUCCESS);

        Assert.assertTrue(
                ShadowToast.showedCustomToast("Sent to Chrome on your Pixel 10.", R.id.toast_text));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsSuccessToast_Throttled() {
        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback>
                confirmationCallbackCaptor =
                        ArgumentCaptor.forClass(
                                SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);

        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                mWebContents,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "Pixel 10",
                URL,
                TITLE,
                null);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        confirmationCallbackCaptor.capture());

        confirmationCallbackCaptor.getValue().onResult(SendTabToSelfResult.SUCCESS_THROTTLED);

        Assert.assertTrue(
                ShadowToast.showedCustomToast(
                        "Already sent to Chrome on your Pixel 10", R.id.toast_text));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_ShowsNoToast_OnFailure() {
        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback>
                confirmationCallbackCaptor =
                        ArgumentCaptor.forClass(
                                SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);

        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                mWebContents,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "Pixel 10",
                URL,
                TITLE,
                null);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        confirmationCallbackCaptor.capture());

        confirmationCallbackCaptor.getValue().onResult(SendTabToSelfResult.FAILURE_INVALID_URL);

        Assert.assertEquals(0, ShadowToast.shownToastCount());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SEND_TAB_TO_SELF_POST_SEND_TOAST)
    public void testSendTabToDevice_PostSendToastFeatureDisabled() {
        ArgumentCaptor<SendTabToSelfAndroidBridge.CommitConfirmationCallback>
                confirmationCallbackCaptor =
                        ArgumentCaptor.forClass(
                                SendTabToSelfAndroidBridge.CommitConfirmationCallback.class);

        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                mWebContents,
                TARGET_DEVICE_SYNC_CACHE_GUID,
                "Pixel 10",
                URL,
                TITLE,
                null);

        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        eq(mWebContents),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        confirmationCallbackCaptor.capture());

        confirmationCallbackCaptor.getValue().onResult(SendTabToSelfResult.SUCCESS);

        Assert.assertEquals(0, ShadowToast.shownToastCount());
    }
}
