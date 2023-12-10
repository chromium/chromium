// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.os.Build;
import android.util.Pair;

import org.chromium.base.BuildInfo;
import org.chromium.base.CollectionUtil;
import org.chromium.base.ContextUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.Map;

/** Grabs feedback about the device information - name and type. */
class DeviceInfoFeedbackSource implements FeedbackSource {
    private static final String DEVICE_NAME_KEY = "device_name";
    private static final String DEVICE_TYPE_KEY = "device_type";
    private static final String TYPE_PHONE = "phone";
    private static final String TYPE_TABLET = "tablet";
    private static final String TYPE_AUTO = "automotive";

    @Override
    public Map<String, String> getFeedback() {
        // "device_name" must use the same value that is used by Play Store feedback reports,
        // as in https://storage.googleapis.com/play_public/supported_devices.html
        // (column 'device'). This can be obtained from system property 'ro.product.device'
        // via android.os.Build.DEVICE.
        String name = Build.DEVICE;
        String type;
        if (BuildInfo.getInstance().isAutomotive) {
            type = TYPE_AUTO;
        } else if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                ContextUtils.getApplicationContext())) {
            type = TYPE_TABLET;
        } else {
            type = TYPE_PHONE;
        }

        return CollectionUtil.newHashMap(
                Pair.create(DEVICE_NAME_KEY, name), Pair.create(DEVICE_TYPE_KEY, type));
    }
}
