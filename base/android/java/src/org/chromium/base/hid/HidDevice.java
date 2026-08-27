// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.hid;

import android.os.OutcomeReceiver;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.concurrent.Executor;

/** Intermediary interface for `android.hardware.hid.HidDevice`. */
@NullMarked
public interface HidDevice {
    /** Interface for receiving input reports from an open HID device. */
    interface HidEventListener {
        /**
         * Called when an input report is received from the HID device.
         *
         * @param reportId The report ID (0 if device does not use report IDs).
         * @param data The raw payload bytes of the input report.
         */
        void onInputReport(int reportId, byte[] data);
    }

    /** Returns the vendor ID of the HID device. */
    int getVendorId();

    /** Returns the product ID of the HID device. */
    int getProductId();

    /** Returns the product name of the HID device. */
    @Nullable String getName();

    /** Returns the transport type of the HID device. */
    int getTransport();

    /** Returns the report descriptor of the HID device. */
    byte @Nullable [] getReportDescriptor();

    /** Returns the unique identifier of the HID device. */
    @Nullable String getUniqueId();

    /** Returns the physical address of the HID device. */
    @Nullable String getPhysicalAddress();

    /** Opens the HID device. */
    void open(Executor executor, OutcomeReceiver<HidDevice, Exception> callback);

    /** Returns whether the connection to the HID device is open. */
    boolean isOpen();

    /** Closes the connection to the HID device. */
    void close();

    /** Sends an output report to the HID device. */
    void sendOutputReport(
            int reportId,
            byte[] data,
            Executor executor,
            OutcomeReceiver<Void, Exception> callback);

    /** Sends a feature report to the HID device. */
    void sendFeatureReport(
            int reportId,
            byte[] data,
            Executor executor,
            OutcomeReceiver<Void, Exception> callback);

    /** Gets a feature report from the HID device. */
    void getFeatureReport(
            int reportId, Executor executor, OutcomeReceiver<byte[], Exception> callback);

    /**
     * Registers a listener for input report events.
     *
     * @param executor The executor on which the listener callback should be invoked.
     * @param listener The listener receiving input report notifications.
     */
    void registerEventListener(Executor executor, HidEventListener listener);

    /**
     * Unregisters a previously registered input report listener.
     *
     * @param listener The listener to remove.
     */
    void unregisterEventListener(HidEventListener listener);
}
