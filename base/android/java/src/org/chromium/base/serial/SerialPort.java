// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.serial;

import android.os.OutcomeReceiver;

import org.chromium.build.annotations.NullMarked;

import java.util.concurrent.Executor;

/** This is a wrapper interface for `android.hardware.serial.SerialPort`. */
@NullMarked
public interface SerialPort {
    /**
     * Value returned by {@link #getVendorId()} and {@link #getProductId()} if this serial port
     * isn't a USB device.
     */
    static final int INVALID_ID = -1;

    /** For use with {@link #requestOpen}: open for reading only. */
    static final int OPEN_FLAG_READ_ONLY = 0;

    /** For use with {@link #requestOpen}: open for writing only. */
    static final int OPEN_FLAG_WRITE_ONLY = 1;

    /** For use with {@link #requestOpen}: open for reading and writing. */
    static final int OPEN_FLAG_READ_WRITE = 1 << 1;

    /** For use with {@link #requestOpen}: when possible, the file is opened in nonblocking mode. */
    static final int OPEN_FLAG_NONBLOCK = 1 << 11;

    /**
     * For use with {@link #requestOpen}: write operations on the file will complete according to
     * the requirements of synchronized I/O data integrity completion (while file metadata may not
     * be synchronized).
     */
    static final int OPEN_FLAG_DATA_SYNC = 1 << 12;

    /**
     * For use with {@link #requestOpen}: write operations on the file will complete according to
     * the requirements of synchronized I/O file integrity completion (by contrast with the
     * synchronized I/O data integrity completion provided by FLAG_DATA_SYNC).
     */
    static final int OPEN_FLAG_SYNC = 1 << 20;

    /** Get the device name. It is the dev node name under /dev, e.g. ttyUSB0, ttyACM1. */
    String getName();

    /**
     * Return the vendor ID of this serial port if it is a USB device. Otherwise, it returns {@link
     * #INVALID_ID}.
     */
    int getVendorId();

    /**
     * Return the product ID of this serial port if it is a USB device. Otherwise, it returns {@link
     * #INVALID_ID}.
     */
    int getProductId();

    /**
     * Request to open the port.
     *
     * <p>Exceptions passed to {@code receiver} may be
     *
     * <ul>
     *   <li>{@link ErrnoException} with ENOENT if the port is detached or any syscall to open the
     *       port fails that come with an errno
     *   <li>{@link IOException} if other required operations fail that don't come with errno
     *   <li>{@link SecurityException} if the user rejects the open request
     * </ul>
     *
     * @param flags open flags that define read/write mode and other options.
     * @param exclusive whether the app needs exclusive access with TIOCEXCL(2const)
     * @param executor the executor used to run receiver
     * @param receiver the outcome receiver
     * @throws IllegalArgumentException if the set of flags is not correct.
     * @throws NullPointerException if any parameters are {@code null}.
     */
    void requestOpen(
            int flags,
            boolean exclusive,
            Executor executor,
            OutcomeReceiver<SerialPortResponse, Exception> receiver);
}
