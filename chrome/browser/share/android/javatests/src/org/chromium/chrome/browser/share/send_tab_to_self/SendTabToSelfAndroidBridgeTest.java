// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.AdditionalAnswers.answerVoid;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.stubbing.VoidAnswer2;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.send_tab_to_self.TargetDeviceInfo.DeviceType;
import org.chromium.content_public.browser.WebContents;

import java.util.Arrays;
import java.util.List;

/** Tests for SendTabToSelfAndroidBridge */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SendTabToSelfAndroidBridgeTest {
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    SendTabToSelfAndroidBridge.Natives mNativeMock;
    private Profile mProfile;
    private WebContents mWebContents;

    private static final String GUID = "randomguid";
    private static final String URL = "http://www.tanyastacos.com";
    private static final String TITLE = "Come try Tanya's famous tacos";
    private static final String DEVICE_NAME = "Macbook Pro";
    private static final String TARGET_DEVICE_SYNC_CACHE_GUID = "randomguid2";
    private static final long NAVIGATION_TIME_MS = 123L;
    private static final long SHARE_TIME_MS = 456L;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(SendTabToSelfAndroidBridgeJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    @SmallTest
    public void testAddEntry() {
        SendTabToSelfAndroidBridge.addEntry(
                mProfile, URL, TITLE, NAVIGATION_TIME_MS, TARGET_DEVICE_SYNC_CACHE_GUID);
        verify(mNativeMock)
                .addEntry(eq(mProfile), eq(URL), eq(TITLE), eq(NAVIGATION_TIME_MS),
                        eq(TARGET_DEVICE_SYNC_CACHE_GUID));
    }

    @Test
    @SmallTest
    @SuppressWarnings("unchecked")
    public void testGetAllTargetDeviceInfos() {
        TargetDeviceInfo one = new TargetDeviceInfo("name1", "guid1", DeviceType.CHROMEOS, 123L);
        TargetDeviceInfo two = new TargetDeviceInfo("name2", "guid2", DeviceType.LINUX, 456L);
        TargetDeviceInfo three = new TargetDeviceInfo("name3", "guid3", DeviceType.PHONE, 789L);
        doAnswer(answerVoid(new VoidAnswer2<Profile, List<TargetDeviceInfo>>() {
            @Override
            public void answer(Profile profile, List<TargetDeviceInfo> deviceInfos) {
                deviceInfos.add(one);
                deviceInfos.add(two);
                deviceInfos.add(three);
            }
        }))
                .when(mNativeMock)
                .getAllTargetDeviceInfos(eq(mProfile), any(List.class));

        List<TargetDeviceInfo> actual =
                SendTabToSelfAndroidBridge.getAllTargetDeviceInfos(mProfile);

        verify(mNativeMock).getAllTargetDeviceInfos(eq(mProfile), any(List.class));
        Assert.assertEquals(3, actual.size());
        List<TargetDeviceInfo> expected = Arrays.asList(one, two, three);
        Assert.assertArrayEquals(expected.toArray(), actual.toArray());
    }

    @Test
    @SmallTest
    @SuppressWarnings("unchecked")
    public void testGetAllGuids() {
        doAnswer(answerVoid(new VoidAnswer2<Profile, List<String>>() {
            @Override
            public void answer(Profile profile, List<String> guids) {
                guids.add("one");
                guids.add("two");
                guids.add("three");
            }
        }))
                .when(mNativeMock)
                .getAllGuids(eq(mProfile), any(List.class));

        List<String> actual = SendTabToSelfAndroidBridge.getAllGuids(mProfile);

        verify(mNativeMock).getAllGuids(eq(mProfile), any(List.class));
        Assert.assertEquals(3, actual.size());
        Assert.assertArrayEquals(new String[] {"one", "two", "three"}, actual.toArray());
    }

    @Test
    @SmallTest
    public void testGetEntryByGUID() {
        SendTabToSelfEntry expected = new SendTabToSelfEntry(GUID, URL, TITLE, SHARE_TIME_MS,
                NAVIGATION_TIME_MS, DEVICE_NAME, TARGET_DEVICE_SYNC_CACHE_GUID);
        when(mNativeMock.getEntryByGUID(eq(mProfile), anyString())).thenReturn(expected);
        // Note that the GUID passed in this function does not match the GUID of the returned entry.
        // This is okay because the purpose of the test is to make sure that the JNI layer passes
        // the entry returned by the native code. The native code does the actual matching of
        // the GUID but since that is mocked out and not the purpose of the test, this is fine.
        SendTabToSelfEntry actual = SendTabToSelfAndroidBridge.getEntryByGUID(mProfile, "guid");
        Assert.assertEquals(expected.guid, actual.guid);
        Assert.assertEquals(expected.url, actual.url);
        Assert.assertEquals(expected.title, actual.title);
        Assert.assertEquals(expected.sharedTime, actual.sharedTime);
        Assert.assertEquals(expected.originalNavigationTime, actual.originalNavigationTime);
        Assert.assertEquals(expected.deviceName, actual.deviceName);
        Assert.assertEquals(expected.targetDeviceSyncCacheGuid, actual.targetDeviceSyncCacheGuid);
    }

    @Test
    @SmallTest
    public void testDeleteAllEntries() {
        SendTabToSelfAndroidBridge.deleteAllEntries(mProfile);
        verify(mNativeMock).deleteAllEntries(eq(mProfile));
    }

    @Test
    @SmallTest
    public void testDismissEntry() {
        SendTabToSelfAndroidBridge.dismissEntry(mProfile, GUID);
        verify(mNativeMock).dismissEntry(eq(mProfile), eq(GUID));
    }

    @Test
    @SmallTest
    public void testMarkEntryOpened() {
        SendTabToSelfAndroidBridge.markEntryOpened(mProfile, GUID);
        verify(mNativeMock).markEntryOpened(eq(mProfile), eq(GUID));
    }

    @Test
    @SmallTest
    public void testDeleteEntry() {
        SendTabToSelfAndroidBridge.deleteEntry(mProfile, GUID);
        verify(mNativeMock).deleteEntry(eq(mProfile), eq(GUID));
    }

    @Test
    @SmallTest
    public void testIsFeatureAvailable() {
        boolean expected = true;
        when(mNativeMock.isFeatureAvailable(eq(mWebContents))).thenReturn(expected);

        boolean actual = SendTabToSelfAndroidBridge.isFeatureAvailable(mWebContents);
        Assert.assertEquals(expected, actual);
    }
}
