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
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.content_public.browser.WebContents;

import java.util.List;

/** Tests for SendTabToSelfAndroidBridge */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SendTabToSelfAndroidBridgeTest {
    @Rule public JniMocker mocker = new JniMocker();

    @Mock SendTabToSelfAndroidBridge.Natives mNativeMock;
    private Profile mProfile;
    private WebContents mWebContents;

    private static final String GUID = "randomguid";
    private static final String URL = "http://www.tanyastacos.com";
    private static final String TITLE = "Come try Tanya's famous tacos";
    private static final String TARGET_DEVICE_SYNC_CACHE_GUID = "randomguid2";

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(SendTabToSelfAndroidBridgeJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    @SmallTest
    public void testAddEntry() {
        SendTabToSelfAndroidBridge.addEntry(mProfile, URL, TITLE, TARGET_DEVICE_SYNC_CACHE_GUID);
        verify(mNativeMock)
                .addEntry(eq(mProfile), eq(URL), eq(TITLE), eq(TARGET_DEVICE_SYNC_CACHE_GUID));
    }

    @Test
    @SmallTest
    @SuppressWarnings("unchecked")
    public void testGetAllTargetDeviceInfos() {
        List<TargetDeviceInfo> expected =
                List.of(
                        new TargetDeviceInfo("name1", "guid1", FormFactor.DESKTOP, 123L),
                        new TargetDeviceInfo("name2", "guid2", FormFactor.DESKTOP, 456L),
                        new TargetDeviceInfo("name3", "guid3", FormFactor.PHONE, 789L));
        when(mNativeMock.getAllTargetDeviceInfos(eq(mProfile))).thenReturn(expected);

        List<TargetDeviceInfo> actual =
                SendTabToSelfAndroidBridge.getAllTargetDeviceInfos(mProfile);

        verify(mNativeMock).getAllTargetDeviceInfos(eq(mProfile));
        Assert.assertEquals(3, actual.size());
        Assert.assertEquals(expected, actual);
    }

    @Test
    @SmallTest
    public void testDismissEntry() {
        SendTabToSelfAndroidBridge.dismissEntry(mProfile, GUID);
        verify(mNativeMock).dismissEntry(eq(mProfile), eq(GUID));
    }

    @Test
    @SmallTest
    public void testDeleteEntry() {
        SendTabToSelfAndroidBridge.deleteEntry(mProfile, GUID);
        verify(mNativeMock).deleteEntry(eq(mProfile), eq(GUID));
    }

    @Test
    @SmallTest
    public void testUpdateActiveWebContents() {
        SendTabToSelfAndroidBridge.updateActiveWebContents(mWebContents);
        verify(mNativeMock).updateActiveWebContents(eq(mWebContents));
    }
}
