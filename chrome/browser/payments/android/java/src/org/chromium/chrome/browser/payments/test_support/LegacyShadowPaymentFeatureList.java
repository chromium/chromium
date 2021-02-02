// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.test_support;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.components.payments.PaymentFeatureList;

import java.util.HashMap;
import java.util.Map;

/**
 * TODO(crbug.com/1170916): Removed soon. This class is a temporary replacement of
 * ShadowPaymentFeatureList, used to transition downstream dependencies.
 */
@Implements(PaymentFeatureList.class)
public class LegacyShadowPaymentFeatureList {
    private static final Map<String, Boolean> sFeatureStatuses = new HashMap<>();

    @Resetter
    public static void reset() {
        sFeatureStatuses.clear();
    }

    @Implementation
    public static boolean isEnabled(String featureName) {
        assert sFeatureStatuses.containsKey(featureName) : "The feature state has yet been set.";
        return sFeatureStatuses.get(featureName);
    }

    /**
     * Set the given feature to be enabled.
     * @param featureName The name of the feature.
     * @param enabled Whether to enable the feature.
     */
    public static void setFeatureEnabled(String featureName, boolean enabled) {
        sFeatureStatuses.put(featureName, enabled);
    }
}
