// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.hid;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;
import java.util.concurrent.Executor;

/** Intermediary interface for `android.hardware.hid.HidManager`. */
@NullMarked
public interface HidManager {
    /** Checks whether device enumeration is permitted. */
    boolean canEnumerateDevices();

    /** Enumerates connected HID devices. */
    @Nullable List<HidDevice> getDevices();

    /** Registers a listener for HID device connection and disconnection events. */
    void registerListener(HidDeviceListener listener, Executor executor);

    /** Unregisters a previously registered HID device listener. */
    void unregisterListener(HidDeviceListener listener);
}
