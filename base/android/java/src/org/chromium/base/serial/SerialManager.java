// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.serial;

import org.chromium.build.annotations.NullMarked;

import java.util.List;
import java.util.concurrent.Executor;

/**
 * This is an intermediary interface for communication with `android.hardware.serial.SerialManager`.
 */
@NullMarked
public interface SerialManager {
    /** Enumerates serial ports. */
    List<SerialPort> getPorts();

    /**
     * Register a listener to monitor serial port connections and disconnections.
     *
     * @throws IllegalStateException if this listener has already been registered.
     */
    void registerSerialPortListener(Executor executor, SerialPortListener listener);

    /** Unregister a listener that monitored serial port connections and disconnections. */
    void unregisterSerialPortListener(SerialPortListener listener);
}
