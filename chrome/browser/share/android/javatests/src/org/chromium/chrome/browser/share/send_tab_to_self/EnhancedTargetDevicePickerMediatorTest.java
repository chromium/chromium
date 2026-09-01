// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.components.sync_device_info.OsType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link EnhancedTargetDevicePickerMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class EnhancedTargetDevicePickerMediatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private SendTabToSelfAndroidBridge.Natives mNativeMock;

    private PropertyModel mModel;

    private static final String SHARING_URL = "https://example.com";
    private static final String SHARING_TITLE = "Example Page";

    private final TargetDeviceInfo mDevice1 =
            new TargetDeviceInfo(
                    "Device 1", "guid_1", FormFactor.PHONE, OsType.ANDROID, "1 day ago");
    private final TargetDeviceInfo mDevice2 =
            new TargetDeviceInfo(
                    "Device 2", "guid_2", FormFactor.DESKTOP, OsType.WINDOWS, "2 days ago");

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        SendTabToSelfAndroidBridgeJni.setInstanceForTesting(mNativeMock);

        mModel =
                new PropertyModel.Builder(EnhancedTargetDevicePickerProperties.ALL_KEYS)
                        .with(EnhancedTargetDevicePickerProperties.DEVICE_LIST, new ModelList())
                        .build();
    }

    private void createMediator(List<TargetDeviceInfo> devices) {
        new EnhancedTargetDevicePickerMediator(
                SHARING_URL,
                SHARING_TITLE,
                devices,
                mProfile,
                () -> mTab,
                mModel,
                ShareEntryPoint.SHARE_SHEET);
    }

    @Test
    public void testPopulateModelAndAutoSelect() {
        createMediator(Arrays.asList(mDevice1, mDevice2));

        ModelList deviceList = mModel.get(EnhancedTargetDevicePickerProperties.DEVICE_LIST);
        assertEquals(2, deviceList.size());

        // Verify mapping of device info
        ListItem item1 = deviceList.get(0);
        TargetDeviceInfo info1 =
                item1.model.get(
                        EnhancedTargetDevicePickerProperties.DeviceItemProperties.DEVICE_INFO);
        assertEquals(mDevice1.deviceName, info1.deviceName);
        assertEquals(mDevice1.cacheGuid, info1.cacheGuid);

        ListItem item2 = deviceList.get(1);
        TargetDeviceInfo info2 =
                item2.model.get(
                        EnhancedTargetDevicePickerProperties.DeviceItemProperties.DEVICE_INFO);
        assertEquals(mDevice2.deviceName, info2.deviceName);

        // Verify auto-selection of first device
        assertEquals(
                mDevice1.cacheGuid,
                mModel.get(EnhancedTargetDevicePickerProperties.SELECTED_DEVICE_ID));
        assertTrue(
                item1.model.get(
                        EnhancedTargetDevicePickerProperties.DeviceItemProperties.IS_SELECTED));
        assertFalse(
                item2.model.get(
                        EnhancedTargetDevicePickerProperties.DeviceItemProperties.IS_SELECTED));
    }

    /**
     * Tests that populating more than 4 devices retains all devices in the property model for full
     * sheet expansion.
     */
    @Test
    public void testPopulateModelWithoutTruncation() {
        TargetDeviceInfo device3 =
                new TargetDeviceInfo(
                        "Device 3", "guid_3", FormFactor.TABLET, OsType.ANDROID, "3 days ago");
        TargetDeviceInfo device4 =
                new TargetDeviceInfo(
                        "Device 4", "guid_4", FormFactor.PHONE, OsType.ANDROID, "4 days ago");
        TargetDeviceInfo device5 =
                new TargetDeviceInfo(
                        "Device 5", "guid_5", FormFactor.DESKTOP, OsType.WINDOWS, "5 days ago");
        TargetDeviceInfo device6 =
                new TargetDeviceInfo(
                        "Device 6", "guid_6", FormFactor.TABLET, OsType.ANDROID, "6 days ago");
        createMediator(Arrays.asList(mDevice1, mDevice2, device3, device4, device5, device6));

        ModelList deviceList = mModel.get(EnhancedTargetDevicePickerProperties.DEVICE_LIST);
        assertEquals(6, deviceList.size());

        ListItem item5 = deviceList.get(4);
        TargetDeviceInfo info5 =
                item5.model.get(
                        EnhancedTargetDevicePickerProperties.DeviceItemProperties.DEVICE_INFO);
        assertEquals("Device 5", info5.deviceName);
        assertEquals("guid_5", info5.cacheGuid);

        ListItem item6 = deviceList.get(5);
        TargetDeviceInfo info6 =
                item6.model.get(
                        EnhancedTargetDevicePickerProperties.DeviceItemProperties.DEVICE_INFO);
        assertEquals("Device 6", info6.deviceName);
        assertEquals("guid_6", info6.cacheGuid);
        assertEquals(
                mDevice1.cacheGuid,
                mModel.get(EnhancedTargetDevicePickerProperties.SELECTED_DEVICE_ID));
        assertTrue(
                deviceList
                        .get(0)
                        .model
                        .get(
                                EnhancedTargetDevicePickerProperties.DeviceItemProperties
                                        .IS_SELECTED));
    }

    @Test
    public void testDeviceSelectionUpdate() {
        createMediator(Arrays.asList(mDevice1, mDevice2));
        ModelList deviceList = mModel.get(EnhancedTargetDevicePickerProperties.DEVICE_LIST);

        // Click second device
        ListItem item2 = deviceList.get(1);
        Runnable clickCallback2 =
                item2.model.get(
                        EnhancedTargetDevicePickerProperties.DeviceItemProperties
                                .ON_CLICK_CALLBACK);
        assertNotNull(clickCallback2);
        clickCallback2.run();

        // Verify selection updated
        assertEquals(
                mDevice2.cacheGuid,
                mModel.get(EnhancedTargetDevicePickerProperties.SELECTED_DEVICE_ID));
        assertFalse(
                deviceList
                        .get(0)
                        .model
                        .get(
                                EnhancedTargetDevicePickerProperties.DeviceItemProperties
                                        .IS_SELECTED));
        assertTrue(
                item2.model.get(
                        EnhancedTargetDevicePickerProperties.DeviceItemProperties.IS_SELECTED));
    }

    @Test
    public void testSendClick() {
        createMediator(Arrays.asList(mDevice1, mDevice2));

        Runnable sendCallback = mModel.get(EnhancedTargetDevicePickerProperties.SEND_CALLBACK);
        assertNotNull(sendCallback);
        sendCallback.run();

        // Verify sendTabToDevice called with auto-selected device1
        verify(mNativeMock)
                .sendTabToDevice(
                        eq(mProfile),
                        any(),
                        eq(mDevice1.cacheGuid),
                        eq(SHARING_URL),
                        eq(SHARING_TITLE),
                        any(),
                        eq(ShareEntryPoint.SHARE_SHEET));
        assertFalse(mModel.get(EnhancedTargetDevicePickerProperties.VISIBLE));
    }

    /**
     * Tests that populating with a large list of devices handles auto-selection without
     * NullPointerException or out-of-bounds indexing.
     */
    @Test
    public void testPopulateModelWithManyDevices_autoSelectsFirstDevice() {
        TargetDeviceInfo device1 =
                new TargetDeviceInfo(
                        "Device 1", "guid_1", FormFactor.PHONE, OsType.ANDROID, "Just now");
        TargetDeviceInfo device2 =
                new TargetDeviceInfo(
                        "Device 2", "guid_2", FormFactor.PHONE, OsType.ANDROID, "Just now");
        TargetDeviceInfo device3 =
                new TargetDeviceInfo(
                        "Device 3", "guid_3", FormFactor.PHONE, OsType.ANDROID, "Just now");
        TargetDeviceInfo device4 =
                new TargetDeviceInfo(
                        "Device 4", "guid_4", FormFactor.PHONE, OsType.ANDROID, "Just now");
        TargetDeviceInfo device5 =
                new TargetDeviceInfo(
                        "Device 5", "guid_5", FormFactor.PHONE, OsType.ANDROID, "Just now");

        // Simulate passing 5 devices.
        createMediator(Arrays.asList(device1, device2, device3, device4, device5));

        ModelList deviceList = mModel.get(EnhancedTargetDevicePickerProperties.DEVICE_LIST);
        assertEquals(5, deviceList.size());

        // Verify the first device is safely auto-selected without NPE or indexing errors.
        assertEquals("guid_1", mModel.get(EnhancedTargetDevicePickerProperties.SELECTED_DEVICE_ID));
        assertTrue(
                deviceList
                        .get(0)
                        .model
                        .get(
                                EnhancedTargetDevicePickerProperties.DeviceItemProperties
                                        .IS_SELECTED));
    }

    /**
     * Tests that populating with a mix of active and inactive/older devices retains all items and
     * safely auto-selects the first active device.
     */
    @Test
    public void testPopulateModelWithManyDevicesMixedActivity() {
        TargetDeviceInfo device3 =
                new TargetDeviceInfo(
                        "Device 3", "guid_3", FormFactor.PHONE, OsType.ANDROID, "Just now");
        TargetDeviceInfo device4 =
                new TargetDeviceInfo(
                        "Device 4", "guid_4", FormFactor.PHONE, OsType.ANDROID, "Just now");
        TargetDeviceInfo device5 =
                new TargetDeviceInfo(
                        "Device 5", "guid_5", FormFactor.PHONE, OsType.ANDROID, "Just now");
        TargetDeviceInfo device6 =
                new TargetDeviceInfo(
                        "Device 6", "guid_6", FormFactor.PHONE, OsType.ANDROID, "Just now");
        TargetDeviceInfo device7 =
                new TargetDeviceInfo(
                        "Device 7", "guid_7", FormFactor.PHONE, OsType.ANDROID, "Just now");

        // Simulate passing 2 active devices (from setUp) + 5 extra devices.
        createMediator(
                Arrays.asList(mDevice1, mDevice2, device3, device4, device5, device6, device7));

        ModelList deviceList = mModel.get(EnhancedTargetDevicePickerProperties.DEVICE_LIST);
        assertEquals(7, deviceList.size());

        // Verify the first active device (mDevice1) is auto-selected.
        assertEquals(
                mDevice1.cacheGuid,
                mModel.get(EnhancedTargetDevicePickerProperties.SELECTED_DEVICE_ID));
        assertTrue(
                deviceList
                        .get(0)
                        .model
                        .get(
                                EnhancedTargetDevicePickerProperties.DeviceItemProperties
                                        .IS_SELECTED));
        for (int i = 1; i < deviceList.size(); i++) {
            assertFalse(
                    deviceList
                            .get(i)
                            .model
                            .get(
                                    EnhancedTargetDevicePickerProperties.DeviceItemProperties
                                            .IS_SELECTED));
        }
    }
}
