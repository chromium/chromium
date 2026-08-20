// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.hid;

import org.chromium.build.annotations.NullMarked;

/** Intermediary interface for `android.hardware.hid.HidDeviceListener`. */
@NullMarked
public interface HidDeviceListener {
    /** Called when a supported HID device is added. */
    void onHidDeviceAdded(HidDevice device);

    /** Called when a supported HID device is removed. */
    void onHidDeviceRemoved(HidDevice device);
}
