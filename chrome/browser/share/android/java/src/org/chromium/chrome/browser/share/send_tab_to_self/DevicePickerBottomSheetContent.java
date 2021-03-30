// Copyright 2019 The Chromium Authors. All rights reserved.
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

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfMetrics.SendTabToSelfShareClickResult;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.Toast;

import java.util.ArrayList;
import java.util.List;

/**
 * Bottom sheet content to display a list of devices a user can send a tab to after they have
 * chosen to share it with themselves through the SendTabToSelfFeature. If sync is disabled
 * or no target devices are available an prompt will be shown indicating to the user that
 * they must sign in to use the feature.
 */
public class DevicePickerBottomSheetContent implements BottomSheetContent, OnItemClickListener {
    private final Context mContext;
    private final BottomSheetController mController;
    private ViewGroup mToolbarView;
    private ViewGroup mContentView;
    private final DevicePickerBottomSheetAdapter mAdapter;
    private final Profile mProfile;
    private final String mUrl;
    private final String mTitle;
    private final long mNavigationTime;
    private final SettingsLauncher mSettingsLauncher;
    private final boolean mIsSyncEnabled;

    public DevicePickerBottomSheetContent(Context context, String url, String title,
            long navigationTime, BottomSheetController controller,
            SettingsLauncher settingsLauncher, boolean isSyncEnabled) {
        mContext = context;
        mController = controller;
        mProfile = Profile.getLastUsedRegularProfile();
        mAdapter = new DevicePickerBottomSheetAdapter(mProfile);
        mUrl = url;
        mTitle = title;
        mNavigationTime = navigationTime;
        mSettingsLauncher = settingsLauncher;
        mIsSyncEnabled = isSyncEnabled;

        createToolbarView();
        createContentView();
    }

    private void createToolbarView() {
        mToolbarView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                R.layout.send_tab_to_self_device_picker_toolbar, null);
        TextView toolbarText = mToolbarView.findViewById(R.id.device_picker_toolbar);
        toolbarText.setText(R.string.send_tab_to_self_sheet_toolbar);
    }

    private void createContentView() {
        List<TargetDeviceInfo> targetDeviceList = new ArrayList<TargetDeviceInfo>();
        SendTabToSelfAndroidBridgeJni.get().getAllTargetDeviceInfos(mProfile, targetDeviceList);

        if (!mIsSyncEnabled) {
            RecordUserAction.record("SharingHubAndroid.SendTabToSelf.NotSyncing");
            mContentView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                    R.layout.send_tab_to_self_feature_unavailable_prompt, null);
            mToolbarView.setVisibility(View.GONE);
            enableSettingsButton();
        } else if (targetDeviceList.isEmpty()) {
            RecordUserAction.record("SharingHubAndroid.SendTabToSelf.NoTargetDevices");
            mContentView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                    R.layout.send_tab_to_self_feature_unavailable_prompt, null);
            mToolbarView.setVisibility(View.GONE);
            TextView textView = mContentView.findViewById(R.id.enable_sync_text_field);
            textView.setText(mContext.getResources().getString(
                    R.string.sharing_hub_no_devices_available_text));
        } else {
            mContentView = (ViewGroup) LayoutInflater.from(mContext).inflate(
                    R.layout.send_tab_to_self_device_picker_list, null);
            ListView listView = mContentView.findViewById(R.id.device_picker_list);
            listView.setAdapter(mAdapter);
            listView.setOnItemClickListener(this);
        }
    }

    private void enableSettingsButton() {
        if (mSettingsLauncher == null) {
            return;
        }
        ButtonCompat chromeSettingsButton = mContentView.findViewById(R.id.chrome_settings);
        chromeSettingsButton.setVisibility(View.VISIBLE);
        chromeSettingsButton.setOnClickListener(view -> {
            RecordUserAction.record("SharingHubAndroid.SendTabToSelf.ChromeSettingsClicked");
            mSettingsLauncher.launchSettingsActivity(ContextUtils.getApplicationContext());
        });
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
        SendTabToSelfShareClickResult.recordClickResult(
                SendTabToSelfShareClickResult.ClickType.CLICK_ITEM);
        TargetDeviceInfo targetDeviceInfo = mAdapter.getItem(position);

        SendTabToSelfAndroidBridge.addEntry(
                mProfile, mUrl, mTitle, mNavigationTime, targetDeviceInfo.cacheGuid);

        Resources res = mContext.getResources();
        String toastMessage =
                res.getString(R.string.send_tab_to_self_toast, targetDeviceInfo.deviceName);
        Toast.makeText(mContext, toastMessage, Toast.LENGTH_SHORT).show();

        mController.hideContent(this, true);
    }
}
