// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.widget.Toast;

import java.util.List;

/**
 * Bottom sheet content to display a list of devices a user can send a tab to after they have chosen
 * to share it with themselves through the send-tab-to-self feature.
 */
class DevicePickerBottomSheetContent implements BottomSheetContent, OnItemClickListener {
    private final Context mContext;
    private final BottomSheetController mController;
    private ViewGroup mToolbarView;
    private ViewGroup mContentView;
    private final DevicePickerBottomSheetAdapter mAdapter;
    private final Profile mProfile;
    private final String mUrl;
    private final String mTitle;

    public DevicePickerBottomSheetContent(
            Context context,
            String url,
            String title,
            BottomSheetController controller,
            List<TargetDeviceInfo> targetDevices,
            Profile profile) {
        mContext = context;
        mController = controller;
        mProfile = profile;
        mAdapter = new DevicePickerBottomSheetAdapter(targetDevices);
        mUrl = url;
        mTitle = title;

        createToolbarView();
        createContentView();
    }

    private void createToolbarView() {
        mToolbarView =
                (ViewGroup)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.send_tab_to_self_device_picker_toolbar, null);
        TextView toolbarText = mToolbarView.findViewById(R.id.device_picker_toolbar);
        toolbarText.setText(R.string.send_tab_to_self_sheet_toolbar);
    }

    private void createContentView() {
        mContentView =
                (ViewGroup)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.send_tab_to_self_device_picker_list, null);
        ListView listView = mContentView.findViewById(R.id.device_picker_list);
        listView.setAdapter(mAdapter);
        listView.setOnItemClickListener(this);

        ViewGroup footerView =
                (ViewGroup)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.send_tab_to_self_device_picker_footer, null);
        ((ManageAccountDevicesLinkView) footerView.findViewById(R.id.manage_account_devices_link))
                .setProfile(mProfile);
        listView.addFooterView(footerView);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // This ensures that the bottom sheet reappears after the first time. Otherwise, the
        // second time that a user initiates a share, the bottom sheet does not re-appear.
        return true;
    }

    @Override
    public int getPeekHeight() {
        // Return DISABLED to ensure that the entire bottom sheet is shown.
        return BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        // Return WRAP_CONTENT to have the bottom sheet only open as far as it needs to display the
        // list of devices and nothing beyond that.
        return BottomSheetContent.HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.send_tab_to_self_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_half_height;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.send_tab_to_self_sheet_closed;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        MetricsRecorder.recordCrossDeviceTabJourney();
        TargetDeviceInfo targetDeviceInfo = mAdapter.getItem(position);

        SendTabToSelfAndroidBridge.addEntry(mProfile, mUrl, mTitle, targetDeviceInfo.cacheGuid);

        Resources res = mContext.getResources();

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SEND_TAB_TO_SELF_V2)) {
            String deviceType = res.getString(R.string.send_tab_to_self_device_type_generic);
            if (targetDeviceInfo.formFactor == FormFactor.PHONE) {
                deviceType = res.getString(R.string.send_tab_to_self_device_type_phone);
            } else if (targetDeviceInfo.formFactor == FormFactor.DESKTOP) {
                deviceType = res.getString(R.string.send_tab_to_self_device_type_computer);
            }

            String toastMessage = res.getString(R.string.send_tab_to_self_v2_toast, deviceType);
            Toast.makeText(mContext, toastMessage, Toast.LENGTH_SHORT).show();
        } else {
            String toastMessage =
                    res.getString(R.string.send_tab_to_self_toast, targetDeviceInfo.deviceName);
            Toast.makeText(mContext, toastMessage, Toast.LENGTH_SHORT).show();
        }

        mController.hideContent(this, true);
    }
}
