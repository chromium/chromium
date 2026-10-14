// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.sync_device_info.RemoteDeviceInfo;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the Send Tab To Self Enhanced Bottom Sheet. */
@NullMarked
class EnhancedTargetDevicePickerProperties {
    static final ReadableObjectPropertyKey<ModelList> DEVICE_LIST =
            new ReadableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Callback<Integer>> DISMISS_CALLBACK =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<String> SELECTED_DEVICE_ID =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Runnable> SEND_CALLBACK =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Runnable> MANAGE_DEVICES_CALLBACK =
            new WritableObjectPropertyKey<>();
    static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS = {
        DEVICE_LIST,
        DISMISS_CALLBACK,
        SELECTED_DEVICE_ID,
        SEND_CALLBACK,
        MANAGE_DEVICES_CALLBACK,
        VISIBLE
    };

    static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS).with(DEVICE_LIST, new ModelList()).build();
    }

    /** Properties for each device item in the list. */
    static class DeviceItemProperties {
        static final ReadableObjectPropertyKey<RemoteDeviceInfo> DEVICE_INFO =
                new ReadableObjectPropertyKey<>();
        static final ReadableObjectPropertyKey<Runnable> ON_CLICK_CALLBACK =
                new ReadableObjectPropertyKey<>();
        static final WritableBooleanPropertyKey IS_SELECTED = new WritableBooleanPropertyKey();

        static final PropertyKey[] ALL_KEYS = {DEVICE_INFO, ON_CLICK_CALLBACK, IS_SELECTED};
    }
}
