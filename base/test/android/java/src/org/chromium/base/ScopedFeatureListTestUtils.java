// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

@JNINamespace("base::android")
@NullMarked
public class ScopedFeatureListTestUtils {
    private ScopedFeatureListTestUtils() {}

    // Use a ScopedFeatureList to update the feature states with the values in the command line.
    // Background information: Each test class and test method can override the feature value via
    // the @EnableFeatures and @DisableFeatures annotations. These annotations are picked up by the
    // the test fixture and the test fixture then appends the appropriate flags to the command line.
    // Afterwards, this function needs to be called to update the feature states with the values in
    // the command line.
    // This function is expected to be called multiple times when a test run involves multiple
    // tests. Internally, this manages a single ScopedFeatureList, which is never destroyed.
    // Calling this function subsequent times will reset that instance to the new state.
    public static void initScopedFeatureList() {
        ScopedFeatureListTestUtilsJni.get().initScopedFeatureList();
    }

    @NativeMethods
    interface Natives {
        void initScopedFeatureList();
    }
}
