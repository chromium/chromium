// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.build.annotations.NullMarked;

/** Provides the state of the native FeatureList. */
@NullMarked
@JNINamespace("base::android")
public class FeatureList {
    /** Prevent access to default values of the native feature flag. */
    private static boolean sDisableNativeForTesting;

    private FeatureList() {}

    /**
     * TODO(crbug.com/345483622): Migrate usages and remove isInitialized().
     *
     * @deprecated For checking flags, use {@code MutableFlagWithSafeDefault} for flags that have a
     *     pre-native default value. Use {@code CachedFlag} for flags that should use a disk-cached
     *     value if checked pre-native. Otherwise, Use {@link #isNativeInitialized()} to check if
     *     the native FeatureList is initialized.
     */
    @Deprecated
    public static boolean isInitialized() {
        return (sDisableNativeForTesting && FeatureOverrides.sTestFeatures != null)
                || isNativeInitialized();
    }

    /**
     * @return Whether the native FeatureList is initialized or not.
     */
    public static boolean isNativeInitialized() {
        if (!LibraryLoader.getInstance().isInitialized()) return false;
        // Even if the native library is loaded, the C++ FeatureList might not be initialized yet.
        // In that case, accessing it will not immediately fail, but instead cause a crash later
        // when it is initialized. Return whether the native FeatureList has been initialized,
        // so the return value can be tested, or asserted for a more actionable stack trace
        // on failure.
        //
        // The FeatureList is however guaranteed to be initialized by the time
        // AsyncInitializationActivity#finishNativeInitialization is called.
        return FeatureListJni.get().isInitialized();
    }

    /**
     * Block (or unblock) querying feature values from native and instead rely only on test values.
     *
     * <p>When native is disabled, calling {@link FeatureMap#isEnabledInNative(String)} will cause
     * an exception to be thrown and calling {@link FeatureMap#getFieldTrialParamByFeature(String,
     * String)} will cause the default value to be returned.
     */
    public static void setDisableNativeForTesting(boolean value) {
        boolean prev = sDisableNativeForTesting;
        sDisableNativeForTesting = value;
        ResettersForTesting.register(() -> sDisableNativeForTesting = prev);
    }

    /**
     * Whether to block querying feature values from native and instead rely only on test values.
     */
    public static boolean getDisableNativeForTesting() {
        return sDisableNativeForTesting;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        boolean isInitialized();
    }
}
