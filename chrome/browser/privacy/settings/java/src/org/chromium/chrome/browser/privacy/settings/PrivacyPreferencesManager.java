// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;

/**
 * Manages preferences related to privacy, metrics reporting, prerendering, and network prediction.
 */
public interface PrivacyPreferencesManager extends CrashReportingPermissionManager {
    /**
     * Checks whether prerender should be allowed and updates the preference if it is not set yet.
     * @return Whether prerendering should be allowed.
     */
    boolean shouldPrerender();

    /**
     * Sets the usage and crash reporting preference ON or OFF.
     *
     * @param enabled A boolean corresponding whether usage and crash reports uploads are allowed.
     */
    void setUsageAndCrashReporting(boolean enabled);

    /**
     * Update usage and crash preferences based on Android preferences if possible in case they are
     * out of sync.
     */
    void syncUsageAndCrashReportingPrefs();

    /**
     * Sets whether this client is in-sample for usage metrics and crash reporting. See
     * {@link org.chromium.chrome.browser.metrics.UmaUtils#isClientInMetricsSample} for details.
     */
    void setClientInMetricsSample(boolean inSample);

    /**
     * Checks whether this client is in-sample for usage metrics and crash reporting. See
     * {@link org.chromium.chrome.browser.metrics.UmaUtils#isClientInMetricsSample} for details.
     *
     * @returns boolean Whether client is in-sample.
     */
    @Override
    boolean isClientInMetricsSample();

    /**
     * Checks whether uploading of crash dumps is permitted for the available network(s).
     *
     * @return whether uploading crash dumps is permitted.
     */
    @Override
    boolean isNetworkAvailableForCrashUploads();

    /**
     * Checks whether uploading of usage metrics and crash dumps is currently permitted, based on
     * user consent only. This doesn't take network condition or experimental state (i.e. disabling
     * upload) into consideration. A crash dump may be retried if this check passes.
     *
     * @return whether the user has consented to reporting usage metrics and crash dumps.
     */
    @Override
    boolean isUsageAndCrashReportingPermittedByUser();

    /**
     * Check whether the command line switch is used to force uploading if at all possible. Used by
     * test devices to avoid UI manipulation.
     *
     * @return whether uploading should be enabled if at all possible.
     */
    @Override
    boolean isUploadEnabledForTests();

    /**
     * @return Whether uploading usage metrics is currently permitted.
     */
    boolean isMetricsUploadPermitted();

    /**
     * @return Whether usage and crash reporting pref is enabled.
     */
    boolean isMetricsReportingEnabled();

    /**
     * Sets whether the usage and crash reporting pref should be enabled.
     */
    void setMetricsReportingEnabled(boolean enabled);

    /**
     * @return Whether usage and crash report pref is managed.
     */
    boolean isMetricsReportingManaged();

    /**
     * @return Network predictions preference.
     */
    boolean getNetworkPredictionEnabled();

    /**
     * Sets network predictions preference.
     */
    void setNetworkPredictionEnabled(boolean enabled);

    /**
     * @return Whether Network Predictions is configured by policy.
     */
    boolean isNetworkPredictionManaged();
}
