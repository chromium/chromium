// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.chromium.base.FeatureList;

/**
 * Flags of this type are un-cached flags that may be called before native,
 * but not primarily. They have good default values to use before native is loaded,
 * and will switch to using the native value once native is loaded.
 * These flags replace code like:
 * if (FeatureList.isInitialized() && ChromeFeatureList.isEnabled(featureName))
 * or
 * if (!FeatureList.isInitialized() || ChromeFeatureList.isEnabled(featureName)).
 */
public class MutableFlagWithSafeDefault extends Flag {
    private final boolean mDefaultValue;

    public MutableFlagWithSafeDefault(String featureName, boolean defaultValue) {
        super(featureName);
        mDefaultValue = defaultValue;
    }

    @Override
    public boolean isEnabled() {
        if (isFeatureListInitialized(mFeatureName)) {
            return ChromeFeatureList.isEnabled(mFeatureName);
        } else {
            return mDefaultValue;
        }
    }

    private static boolean isFeatureListInitialized(String featureName) {
        return FeatureList.hasTestFeature(featureName) || FeatureList.isNativeInitialized();
    }
}
