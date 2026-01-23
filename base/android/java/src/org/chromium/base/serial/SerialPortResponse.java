// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.serial;

import android.os.ParcelFileDescriptor;

import org.chromium.build.annotations.NullMarked;

/**
 * This is an intermediary interface for communication with `android.hardware.serial.SerialPort`.
 */
@NullMarked
public interface SerialPortResponse {
    /** The serial port for which this response is. */
    SerialPort getPort();

    /**
     * The file descriptor obtained by opening the device node of the serial port.
     *
     * <p>The client of the API is responsible for closing the file descriptor after use.
     */
    ParcelFileDescriptor getFileDescriptor();
}
