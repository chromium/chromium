// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.serial;

import org.chromium.build.annotations.NullMarked;

/** This is a copy of `android.hardware.serial.SerialPortListener`. */
@NullMarked
public interface SerialPortListener {
    /** Called when a supported serial port is connected. */
    void onSerialPortConnected(SerialPort port);

    /** Called when a supported serial port is disconnected. */
    void onSerialPortDisconnected(SerialPort port);
}
