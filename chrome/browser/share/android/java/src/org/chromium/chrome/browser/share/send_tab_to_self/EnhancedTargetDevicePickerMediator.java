// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Supplier;

/** Mediator for the Send Tab To Self Enhanced Bottom Sheet. */
@NullMarked
class EnhancedTargetDevicePickerMediator {
    private final String mUrl;
    private final String mTitle;
    private final Profile mProfile;
    private final Supplier<@Nullable Tab> mTabProvider;
    private final PropertyModel mModel;

    private final @ShareEntryPoint int mEntryPoint;
    private final Map<String, PropertyModel> mDeviceModels = new HashMap<>();
    private @Nullable PropertyModel mSelectedModel;
    private boolean mIsActionStarted;

    EnhancedTargetDevicePickerMediator(
            String url,
            String title,
            List<TargetDeviceInfo> targetDevices,
            Profile profile,
            Supplier<@Nullable Tab> tabProvider,
            PropertyModel model,
            @ShareEntryPoint int entryPoint) {
        mUrl = url;
        mTitle = title;
        mProfile = profile;
        mTabProvider = tabProvider;
        mModel = model;
        mEntryPoint = entryPoint;

        mModel.set(EnhancedTargetDevicePickerProperties.SEND_CALLBACK, this::onSendClick);

        populateModel(targetDevices);
    }

    private void populateModel(List<TargetDeviceInfo> targetDevices) {
        ModelList deviceList = mModel.get(EnhancedTargetDevicePickerProperties.DEVICE_LIST);
        for (TargetDeviceInfo deviceInfo : targetDevices) {
            PropertyModel itemModel =
                    new PropertyModel.Builder(
                                    EnhancedTargetDevicePickerProperties.DeviceItemProperties
                                            .ALL_KEYS)
                            .with(
                                    EnhancedTargetDevicePickerProperties.DeviceItemProperties
                                            .DEVICE_INFO,
                                    deviceInfo)
                            .with(
                                    EnhancedTargetDevicePickerProperties.DeviceItemProperties
                                            .ON_CLICK_CALLBACK,
                                    () -> onDeviceClick(deviceInfo))
                            .with(
                                    EnhancedTargetDevicePickerProperties.DeviceItemProperties
                                            .IS_SELECTED,
                                    false)
                            .build();
            deviceList.add(new ListItem(EnhancedTargetDevicePickerView.ItemType.DEVICE, itemModel));
            mDeviceModels.put(deviceInfo.cacheGuid, itemModel);
        }

        // Auto-select the first device by default if available.
        if (!targetDevices.isEmpty()) {
            onDeviceClick(targetDevices.get(0));
        }
    }

    private void onDeviceClick(TargetDeviceInfo targetDeviceInfo) {
        mModel.set(
                EnhancedTargetDevicePickerProperties.SELECTED_DEVICE_ID,
                targetDeviceInfo.cacheGuid);

        PropertyModel newSelectedModel = mDeviceModels.get(targetDeviceInfo.cacheGuid);
        if (mSelectedModel != null) {
            mSelectedModel.set(
                    EnhancedTargetDevicePickerProperties.DeviceItemProperties.IS_SELECTED, false);
        }
        if (newSelectedModel != null) {
            newSelectedModel.set(
                    EnhancedTargetDevicePickerProperties.DeviceItemProperties.IS_SELECTED, true);
        }
        mSelectedModel = newSelectedModel;
    }

    private void onSendClick() {
        if (mIsActionStarted || mSelectedModel == null) {
            return;
        }
        mIsActionStarted = true;

        mModel.set(EnhancedTargetDevicePickerProperties.VISIBLE, false);

        SendTabToSelfMetricsRecorder.recordCrossDeviceTabJourney();

        Tab tab = mTabProvider.get();
        WebContents webContents = (tab != null) ? tab.getWebContents() : null;

        TargetDeviceInfo selectedDevice =
                mSelectedModel.get(
                        EnhancedTargetDevicePickerProperties.DeviceItemProperties.DEVICE_INFO);
        SendTabToSelfAndroidBridge.sendTabToDevice(
                mProfile,
                webContents,
                selectedDevice.cacheGuid,
                selectedDevice.deviceName,
                mUrl,
                mTitle,
                mEntryPoint);
    }
}
