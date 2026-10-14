// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.chromium.chrome.browser.share.send_tab_to_self.EnhancedTargetDevicePickerProperties.DEVICE_LIST;
import static org.chromium.chrome.browser.share.send_tab_to_self.EnhancedTargetDevicePickerProperties.DISMISS_CALLBACK;
import static org.chromium.chrome.browser.share.send_tab_to_self.EnhancedTargetDevicePickerProperties.DeviceItemProperties.DEVICE_INFO;
import static org.chromium.chrome.browser.share.send_tab_to_self.EnhancedTargetDevicePickerProperties.DeviceItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.share.send_tab_to_self.EnhancedTargetDevicePickerProperties.DeviceItemProperties.ON_CLICK_CALLBACK;
import static org.chromium.chrome.browser.share.send_tab_to_self.EnhancedTargetDevicePickerProperties.MANAGE_DEVICES_CALLBACK;
import static org.chromium.chrome.browser.share.send_tab_to_self.EnhancedTargetDevicePickerProperties.SELECTED_DEVICE_ID;
import static org.chromium.chrome.browser.share.send_tab_to_self.EnhancedTargetDevicePickerProperties.SEND_CALLBACK;
import static org.chromium.chrome.browser.share.send_tab_to_self.EnhancedTargetDevicePickerProperties.VISIBLE;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.components.sync_device_info.RemoteDeviceInfo;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** ViewBinder for the Send Tab To Self Enhanced Bottom Sheet. */
@NullMarked
class EnhancedTargetDevicePickerViewBinder {
    static void bind(PropertyModel model, EnhancedTargetDevicePickerView view, PropertyKey key) {
        if (key == DEVICE_LIST) {
            setupDeviceListAdapter(model, view);
        } else if (key == DISMISS_CALLBACK) {
            view.setDismissHandler(model.get(DISMISS_CALLBACK));
        } else if (key == SELECTED_DEVICE_ID) {
            updateSendButtonState(model, view);
        } else if (key == SEND_CALLBACK) {
            setupSendButtonCallback(model, view);
        } else if (key == MANAGE_DEVICES_CALLBACK) {
            setupManageDevicesCallback(model, view);
        } else if (key == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        }
    }

    private static void setupDeviceListAdapter(
            PropertyModel model, EnhancedTargetDevicePickerView view) {
        ModelList deviceList = model.get(DEVICE_LIST);
        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(deviceList);
        adapter.registerType(
                EnhancedTargetDevicePickerView.ItemType.DEVICE,
                EnhancedTargetDevicePickerViewBinder::createDeviceItemView,
                EnhancedTargetDevicePickerViewBinder::bindDeviceItemView);
        view.setSheetItemListAdapter(adapter);
    }

    private static void updateSendButtonState(
            PropertyModel model, EnhancedTargetDevicePickerView view) {
        String selectedId = model.get(SELECTED_DEVICE_ID);
        view.mSendButton.setEnabled(selectedId != null);
    }

    private static void setupSendButtonCallback(
            PropertyModel model, EnhancedTargetDevicePickerView view) {
        view.mSendButton.setOnClickListener(
                v -> {
                    Runnable callback = model.get(SEND_CALLBACK);
                    if (callback != null) {
                        callback.run();
                    }
                });
    }

    private static void setupManageDevicesCallback(
            PropertyModel model, EnhancedTargetDevicePickerView view) {
        view.mManageDevicesLink.setOnClickListener(
                v -> {
                    Runnable callback = model.get(MANAGE_DEVICES_CALLBACK);
                    if (callback != null) {
                        callback.run();
                    }
                });
    }

    private static View createDeviceItemView(android.view.ViewGroup parent) {
        View view =
                android.view.LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.send_tab_to_self_enhanced_list_item, parent, false);
        view.setTag(new DeviceItemViewHolder(view));
        return view;
    }

    private static void bindDeviceItemView(PropertyModel model, View view, PropertyKey key) {
        DeviceItemViewHolder holder = (DeviceItemViewHolder) view.getTag();
        if (key == DEVICE_INFO) {
            bindDeviceInfo(view.getContext(), holder, model.get(DEVICE_INFO));
        } else if (key == ON_CLICK_CALLBACK) {
            view.setOnClickListener(
                    v -> {
                        Runnable callback = model.get(ON_CLICK_CALLBACK);
                        if (callback != null) {
                            callback.run();
                        }
                    });
        } else if (key == IS_SELECTED) {
            bindSelectionState(view, holder, model.get(IS_SELECTED));
        }
    }

    private static void bindDeviceInfo(
            Context context, DeviceItemViewHolder holder, RemoteDeviceInfo deviceInfo) {
        holder.mDeviceIcon.setImageDrawable(getDrawableForDeviceType(context, deviceInfo));
        holder.mDeviceIcon.setVisibility(View.VISIBLE);
        holder.mDeviceName.setText(deviceInfo.deviceName);
        holder.mLastActive.setText(deviceInfo.lastActiveTimeForDisplay);
    }

    private static void bindSelectionState(
            View view, DeviceItemViewHolder holder, boolean isSelected) {
        holder.mCheckIcon.setVisibility(isSelected ? View.VISIBLE : View.INVISIBLE);
        view.setSelected(isSelected);
    }

    private static Drawable getDrawableForDeviceType(
            Context context, RemoteDeviceInfo targetDevice) {
        switch (targetDevice.formFactor) {
            case FormFactor.DESKTOP:
                return AppCompatResources.getDrawable(context, R.drawable.computer_black_24dp);
            case FormFactor.PHONE:
                return AppCompatResources.getDrawable(context, R.drawable.smartphone_black_24dp);
            case FormFactor.TABLET:
                return AppCompatResources.getDrawable(context, R.drawable.tablet_black_24dp);
        }
        return AppCompatResources.getDrawable(context, R.drawable.devices_black_24dp);
    }

    private static class DeviceItemViewHolder {
        final ImageView mDeviceIcon;
        final TextView mDeviceName;
        final TextView mLastActive;
        final View mCheckIcon;

        DeviceItemViewHolder(View view) {
            mDeviceIcon = view.findViewById(R.id.device_icon);
            mDeviceName = view.findViewById(R.id.device_name);
            mLastActive = view.findViewById(R.id.last_active);
            mCheckIcon = view.findViewById(R.id.check_icon);
        }
    }
}
