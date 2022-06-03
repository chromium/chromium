// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

/**
 * A class that serves as a bridge to native code to check the status of feature switches.
 *
 * Each subclass represents a set of related features. Each instance of such a class correlates to a
 * single C++ Feature.
 */
@JNINamespace("base::android")
@MainDex
public abstract class Features {
    private final String mName;

    protected Features(String name) {
        mName = name;
    }

    /** Returns the string value which is the `name` field in the native Feature object. */
    public String getName() {
        return mName;
    }

    /** Returns true if the given feature is enabled. */
    public boolean isEnabled() {
        return FeaturesJni.get().isEnabled(getFeaturePointer());
    }

    /**
     * Returns a field trial param as a boolean for the specified feature.
     *
     * @param paramName The name of the param.
     * @param defaultValue The boolean value to use if the param is not available.
     * @return The parameter value as a boolean. Default value if the feature does not exist or the
     *         specified parameter does not exist or its string value is neither "true" nor "false".
     */
    public boolean getFieldTrialParamByFeatureAsBoolean(String paramName, boolean defaultValue) {
        return FeaturesJni.get().getFieldTrialParamByFeatureAsBoolean(
                getFeaturePointer(), paramName, defaultValue);
    }

    /** Returns a pointer to the native Feature object represented by this object instance. */
    protected abstract long getFeaturePointer();

    @NativeMethods
    interface Natives {
        boolean isEnabled(long featurePointer);
        boolean getFieldTrialParamByFeatureAsBoolean(
                long featurePointer, String paramName, boolean defaultValue);
    }
}
