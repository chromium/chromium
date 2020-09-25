// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.secure_dns;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.net.SecureDnsManagementMode;
import org.chromium.net.SecureDnsMode;

import java.util.ArrayList;
import java.util.List;

/**
 * Reads and writes preferences related to Secure DNS.
 */
class SecureDnsBridge {
    /**
     * An Entry represents the subset of a net::DohProviderEntry that is relevant
     * for display in the UI.
     */
    static class Entry {
        public final @NonNull String name; // Display name
        public final @NonNull String template; // URI template, or "" for the custom entry.
        public final @NonNull String privacy; // Privacy policy link

        Entry(String name, String template, String privacy) {
            this.name = name;
            this.template = template;
            this.privacy = privacy;
        }

        @Override
        public String toString() {
            return name;
        }
    }

    /**
     * @return Whether the DoH UI field trial is active on this instance.
     */
    static boolean isUiEnabled() {
        // Must match features::kDnsOverHttpsShowUiParam.
        final String showUiParam = "ShowUi";
        // Must match the default value for this param.
        final boolean showUiParamDefault = true;

        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.DNS_OVER_HTTPS, showUiParam, showUiParamDefault);
    }

    /**
     * @return The current Secure DNS mode (off, automatic, or secure).
     */
    public static @SecureDnsMode int getMode() {
        return SecureDnsBridgeJni.get().getMode();
    }

    /**
     * Sets the current Secure DNS mode.  Callers must successfully set a DoH template
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
     * @return The templates (separated by spaces) of the DoH server
     *     currently selected for use in "secure" mode, or "" if there is none.
     */
    static String getTemplates() {
        return SecureDnsBridgeJni.get().getTemplates();
    }

    /**
     * Sets the templates to use for DoH in secure mode, if they are valid.
     *
     * @param templates The templates (separated by spaces) to store, or "" to clear
     *     the setting.
     * @return True if the input was valid.
     */
    static boolean setTemplates(String templates) {
        return SecureDnsBridgeJni.get().setTemplates(templates);
    }

    /**
     * @return The current Secure DNS management mode.  Note that this is entirely
     *     independent of isDnsOverHttpsModeManaged.
     */
    static @SecureDnsManagementMode int getManagementMode() {
        return SecureDnsBridgeJni.get().getManagementMode();
    }

    /**
     * Record a DoH selection action for statistical purposes.
     * @param oldEntry The previous selection.
     * @param newEntry The current selection.
     */
    static void updateDropdownHistograms(Entry oldEntry, Entry newEntry) {
        SecureDnsBridgeJni.get().updateDropdownHistograms(oldEntry.template, newEntry.template);
    }

    /**
     * Record whether a custom template entered was valid for statistical purposes.
     * @param valid True if the template was valid.
     */
    static void updateValidationHistogram(boolean valid) {
        SecureDnsBridgeJni.get().updateValidationHistogram(valid);
    }

    /**
     * @param group A collection of DoH templates in textual format.
     * @return All templates in the group, or an empty array if the group is not valid.
     */
    static String[] splitTemplateGroup(String group) {
        return SecureDnsBridgeJni.get().splitTemplateGroup(group);
    }

    /**
     * Perform a probe to see if a server is working.  This function blocks until the
     * probe completes.
     *
     * @param template A valid DoH URI template.
     * @return True if the server is reachable and functioning correctly.
     */
    static boolean probeServer(String template) {
        return SecureDnsBridgeJni.get().probeServer(template);
    }

    @NativeMethods
    interface Natives {
        @SecureDnsMode
        int getMode();
        void setMode(@SecureDnsMode int mode);
        boolean isModeManaged();
        String[][] getProviders();
        String getTemplates();
        boolean setTemplates(String templates);
        @SecureDnsManagementMode
        int getManagementMode();
        void updateDropdownHistograms(String oldTemplate, String newTemplate);
        void updateValidationHistogram(boolean valid);
        String[] splitTemplateGroup(String group);
        boolean probeServer(String dohTemplate);
    }
}
