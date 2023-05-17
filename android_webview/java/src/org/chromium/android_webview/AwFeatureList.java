// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.base.ContextUtils;
import org.chromium.base.PackageUtils;

/**
 * Java accessor for base/feature_list.h state.
 */
public final class AwFeatureList {
    // Do not instantiate this class.
    private AwFeatureList() {}

    private static final String GMS_PACKAGE = "com.google.android.gms";

    private static Boolean sPageStartedOnCommitForBrowserNavigations;

    private static boolean computePageStartedOnCommitForBrowserNavigations() {
        if (GMS_PACKAGE.equals(ContextUtils.getApplicationContext().getPackageName())) {
            int gmsPackageVersion = PackageUtils.getPackageVersion(GMS_PACKAGE);
            return gmsPackageVersion >= 15000000;
        }
        return true;
    }

    public static boolean pageStartedOnCommitEnabled(boolean isRendererInitiated) {
        // Always enable for renderer-initiated navigations.
        if (isRendererInitiated) return true;
        if (sPageStartedOnCommitForBrowserNavigations != null) {
            return sPageStartedOnCommitForBrowserNavigations;
        }
        sPageStartedOnCommitForBrowserNavigations =
                computePageStartedOnCommitForBrowserNavigations();
        return sPageStartedOnCommitForBrowserNavigations;
    }

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in android_webview/browser/aw_feature_map.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        return AwFeatureMap.getInstance().isEnabled(featureName);
    }

    /**
     * Returns the configured feature parameter value as an integer.
     *
     * If the feature is not enabled or the parameter does not exist, this method
     * will return the |defaultValue|.
     *
     * Calling this method will mark the field trial as active. See details
     * in base/metrics/field_trial_params.h
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in android_webview/browser/aw_feature_map.cc
     *
     * @param featureName The name of the feature to query.
     * @param paramName The name of the feature parameter to query.
     * @param defaultValue The default value to return if the feature or parameter is not found.
     * @return The configured parameter value as an integer.
     */
    public static int getFeatureParamValueAsInt(
            String featureName, String paramName, int defaultValue) {
        return AwFeatureMap.getInstance().getFieldTrialParamByFeatureAsInt(
                featureName, paramName, defaultValue);
    }
}
