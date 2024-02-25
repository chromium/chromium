// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;

import org.chromium.base.CommandLine;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import javax.inject.Inject;

/**
 * Class that allows the Custom Tab client to override features for a single session. Note that this
 * class is meant to be used for experimentation purposes only, and the features will be removed
 * from the `ALLOWED_FEATURES` list once they fully ship to Stable.
 */
@ActivityScope
public class CustomTabFeatureOverridesManager {
    private static final String TAG = "CTFeatureOvrdMgr";
    private static final Set<String> ALLOWED_FEATURES =
            new HashSet<>(Arrays.asList(ChromeFeatureList.CCT_MINIMIZED));

    private static Set<String> sAllowedFeaturesForTesting;

    private Map<String, Boolean> mFeatureOverrides;

    @Inject
    CustomTabFeatureOverridesManager(BrowserServicesIntentDataProvider intentDataProvider) {
        if (ChromeFeatureList.sCctIntentFeatureOverrides.isEnabled()
                && (CommandLine.getInstance().hasSwitch("cct-client-firstparty-override")
                        || intentDataProvider.isTrustedIntent())) {
            setUpFeatureOverrides(
                    intentDataProvider.getIntent(),
                    sAllowedFeaturesForTesting != null
                            ? sAllowedFeaturesForTesting
                            : ALLOWED_FEATURES);
        }
    }

    /**
     * @param feature The feature to check for an override value.
     * @return Whether the feature is overridden and enabled, null if it's not overridden or if
     *     overrides aren't allowed.
     */
    public Boolean isFeatureEnabled(String feature) {
        if (mFeatureOverrides == null || mFeatureOverrides.isEmpty()) return null;
        return mFeatureOverrides.get(feature);
    }

    private void setUpFeatureOverrides(Intent intent, Set<String> allowedFeatures) {
        mFeatureOverrides = new HashMap<>();
        ArrayList<String> enabledFeatures =
                IntentUtils.safeGetStringArrayListExtra(
                        intent, CustomTabIntentDataProvider.EXPERIMENTS_ENABLE);
        ArrayList<String> disabledFeatures =
                IntentUtils.safeGetStringArrayListExtra(
                        intent, CustomTabIntentDataProvider.EXPERIMENTS_DISABLE);
        if (enabledFeatures != null) {
            for (var feature : enabledFeatures) {
                if (!allowedFeatures.contains(feature)) {
                    Log.e(TAG, "The feature " + feature + " is not allowed to be overridden.");
                    continue;
                }
                mFeatureOverrides.put(feature, true);
            }
        }
        if (disabledFeatures != null) {
            for (var feature : disabledFeatures) {
                if (!allowedFeatures.contains(feature)) {
                    Log.e(TAG, "The feature " + feature + " is not allowed to be overridden.");
                    continue;
                }
                if (mFeatureOverrides.containsKey(feature)) {
                    mFeatureOverrides.put(feature, null);
                    Log.e(TAG, "There are conflicting override values for the feature " + feature);
                    continue;
                }
                mFeatureOverrides.put(feature, false);
            }
        }
    }

    public static void setAllowedFeaturesForTesting(Set<String> allowedFeatures) {
        sAllowedFeaturesForTesting = allowedFeatures;
        ResettersForTesting.register(() -> sAllowedFeaturesForTesting = null);
    }
}
