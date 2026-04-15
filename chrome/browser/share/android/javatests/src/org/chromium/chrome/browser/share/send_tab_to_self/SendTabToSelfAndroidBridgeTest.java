// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.mock.MockWebContents;

import java.util.List;

/** Tests for SendTabToSelfAndroidBridge */
@RunWith(BaseRobolectricTestRunner.class)
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
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mNativeMock);
        mWebContents = new MockWebContents();
    }

    @Test
    @SmallTest
    public void testSendTabToDevice() {
        SendTabToSelfAndroidBridge.sendTabToDevice(
                mWebContents, TARGET_DEVICE_SYNC_CACHE_GUID, URL, TITLE, null);
        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mWebContents),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID),
                        eq(URL),
                        eq(TITLE),
                        eq(null));
    }

    @Test
    @SmallTest
    public void testDeleteEntry() {
        String guid = "guid";
        SendTabToSelfAndroidBridge.deleteEntry(mProfile, guid);
        verify(mNativeMock).deleteEntry(eq(mProfile), eq(guid));
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
    public void testUpdateActiveWebContents() {
        SendTabToSelfAndroidBridge.updateActiveWebContents(mWebContents);
        verify(mNativeMock).updateActiveWebContents(eq(mWebContents));
    }

    @Test
    @SmallTest
    public void testGetEntryPointDisplayReason() {
        SendTabToSelfAndroidBridge.getEntryPointDisplayReason(mProfile, URL);
        verify(mNativeMock).getEntryPointDisplayReason(eq(mProfile), eq(URL));
    }
}
