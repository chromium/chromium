// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.secure_dns;

import androidx.annotation.NonNull;

import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.net.SecureDnsManagementMode;
import org.chromium.net.SecureDnsMode;

import java.util.ArrayList;
import java.util.List;

/** Reads and writes preferences related to Secure DNS. */
class SecureDnsBridge {
    /**
     * An Entry represents the subset of a net::DohProviderEntry that is relevant
     * for display in the UI.
     */
    static class Entry {
        public final @NonNull String name; // Display name
        public final @NonNull String config; // DoH config, or "" for the custom entry.
        public final @NonNull String privacy; // Privacy policy link

        Entry(String name, String config, String privacy) {
            this.name = name;
            this.config = config;
            this.privacy = privacy;
        }

        @Override
        public String toString() {
            return name;
        }
    }

    /**
     * @return The current Secure DNS mode (off, automatic, or secure).
     */
    public static @SecureDnsMode int getMode() {
        return SecureDnsBridgeJni.get().getMode();
    }

    /**
     * Sets the current Secure DNS mode.  Callers must successfully set a DoH config
     * before changing the mode to "secure".
     *
     * @param mode The desired new Secure DNS mode.
     */
    static void setMode(@SecureDnsMode int mode) {
        SecureDnsBridgeJni.get().setMode(mode);
    }

    /**
     * @return True if the Secure DNS mode is controlled by a policy.
     */
    static boolean isModeManaged() {
        return SecureDnsBridgeJni.get().isModeManaged();
    }

    /**
     * @return The built-in DoH providers that should be displayed to the user.
     */
    static List<Entry> getProviders() {
        String[][] values = SecureDnsBridgeJni.get().getProviders();
        ArrayList<Entry> entries = new ArrayList<>(values.length);
        for (String[] v : values) {
            entries.add(new Entry(v[0], v[1], v[2]));
        }
        return entries;
    }

    /**
     * Get the raw preference value, which can represent multiple templates separated
     * by whitespace.  The raw value is needed in order to allow direct editing while
     * preserving whitespace.
     *
     * @return The current DoH config for use in "secure" mode, or "" if there is none.
     */
    static String getConfig() {
        return SecureDnsBridgeJni.get().getConfig();
    }

    /**
     * Sets the DoH config to use in secure mode, if it is valid.
     *
     * @param config The DoH config to store, or "" to clear the setting.
     * @return True if the input was valid.
     */
    static boolean setConfig(String config) {
        return SecureDnsBridgeJni.get().setConfig(config);
    }

    /**
     * @return The current Secure DNS management mode.  Note that this is entirely
     *     independent of isDnsOverHttpsModeManaged.
     */
    static @SecureDnsManagementMode int getManagementMode() {
        return SecureDnsBridgeJni.get().getManagementMode();
    }

    /**
     * Record whether a custom DoH config entered was valid for statistical purposes.
     * @param valid True if the config was valid.
     */
    static void updateValidationHistogram(boolean valid) {
        SecureDnsBridgeJni.get().updateValidationHistogram(valid);
    }

    /**
     * Perform a probe to see if a server set is working.  This function blocks until the
     * probes complete.
     *
     * @param dohConfig A valid DoH configuration string.
     * @return True if any server is reachable and functioning correctly.
     */
    static boolean probeConfig(String dohConfig) {
        return SecureDnsBridgeJni.get().probeConfig(dohConfig);
    }

    @NativeMethods
    interface Natives {
        @SecureDnsMode
        int getMode();

        void setMode(@SecureDnsMode int mode);

        boolean isModeManaged();

        String[][] getProviders();

        String getConfig();

        boolean setConfig(String config);

        @SecureDnsManagementMode
        int getManagementMode();

        void updateValidationHistogram(boolean valid);

        boolean probeConfig(String dohConfig);
    }
}
