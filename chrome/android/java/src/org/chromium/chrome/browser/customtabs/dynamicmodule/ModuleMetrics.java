// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.StringDef;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.metrics.UmaSessionStats;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Records metrics related to custom tabs dynamic modules.
 */
public final class ModuleMetrics {
    private ModuleMetrics() {}

    private static final String TAG = "ModuleMetrics";

    /**
     * The name of the synthetic field trial for registering the module lifecycle state.
     */
    private static final String LIFECYCLE_STATE_TRIAL_NAME = "CCTModuleLifecycleState";

    /**
     * Possible results when loading a dynamic module. Keep in sync with the
     * CustomTabsDynamicModuleLoadResult enum in enums.xml. Do not remove
     * or change existing values other than NUM_ENTRIES.
     */
    @IntDef({LoadResult.SUCCESS_NEW, LoadResult.SUCCESS_CACHED, LoadResult.FEATURE_DISABLED,
            LoadResult.NOT_GOOGLE_SIGNED, LoadResult.PACKAGE_NAME_NOT_FOUND_EXCEPTION,
            LoadResult.CLASS_NOT_FOUND_EXCEPTION, LoadResult.INSTANTIATION_EXCEPTION,
            LoadResult.INCOMPATIBLE_VERSION, LoadResult.FAILED_TO_COPY_DEX_EXCEPTION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface LoadResult {
        /** A new instance of the module was loaded successfully. */
        int SUCCESS_NEW = 0;
        /** A cached instance of the module was used. */
        int SUCCESS_CACHED = 1;
        /** The module could not be loaded because the feature is disabled. */
        int FEATURE_DISABLED = 2;
        /** The module could not be loaded because the package is not Google-signed. */
        int NOT_GOOGLE_SIGNED = 3;
        /** The module could not be loaded because the package name could not be found. */
        int PACKAGE_NAME_NOT_FOUND_EXCEPTION = 4;
        /** The module could not be loaded because the entry point class could not be found. */
        int CLASS_NOT_FOUND_EXCEPTION = 5;
        /** The module could not be loaded because the entry point class could not be instantiated.
         */
        int INSTANTIATION_EXCEPTION = 6;
        /** The module was loaded but the host and module versions are incompatible. */
        int INCOMPATIBLE_VERSION = 7;
        /** The module dex could not be copied to local storage. */
        int FAILED_TO_COPY_DEX_EXCEPTION = 8;
        /** Upper bound for legal sample values - all sample values have to be strictly lower. */
        int NUM_ENTRIES = 9;
    }

    /**
     * Records the result of attempting to load a dynamic module.
     * @param result result key, one of {@link LoadResult}'s values.
     */
    public static void recordLoadResult(@LoadResult int result) {
        RecordHistogram.recordEnumeratedHistogram(
                "CustomTabs.DynamicModule.LoadResult", result, LoadResult.NUM_ENTRIES);
        if (result != LoadResult.SUCCESS_NEW && result != LoadResult.SUCCESS_CACHED) {
            Log.w(TAG, "Did not load module, result: %s", result);
        }
    }

    /**
     * Possible reasons for destroying a dynamic module. Keep in sync with the
     * CustomTabs.DynamicModule.DestructionReason enum in tools/metrics/histograms/enums.xml.
     * Do not remove or change existing values other than NUM_ENTRIES.
     */
    @IntDef({DestructionReason.NO_CACHING_UNUSED, DestructionReason.CACHED_SEVERE_MEMORY_PRESSURE,
            DestructionReason.CACHED_MILD_MEMORY_PRESSURE_TIME_EXCEEDED,
            DestructionReason.CACHED_UI_HIDDEN_TIME_EXCEEDED,
            DestructionReason.MODULE_LOADER_CHANGED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DestructionReason {
        /** No caching enabled and the module is unused. */
        int NO_CACHING_UNUSED = 0;
        /** Cached and severe memory pressure. */
        int CACHED_SEVERE_MEMORY_PRESSURE = 1;
        /** Cached and mild memory pressure and time exceeded. */
        int CACHED_MILD_MEMORY_PRESSURE_TIME_EXCEEDED = 2;
        /** Cached and app hidden and time exceeded. */
        int CACHED_UI_HIDDEN_TIME_EXCEEDED = 3;
        /** Module loader setup is changed. */
        int MODULE_LOADER_CHANGED = 4;
        /** Upper bound for legal sample values - all sample values have to be strictly lower. */
        int NUM_ENTRIES = 5;
    }

    /**
     * Records the reason for destroying a dynamic module.
     * @param reason reason key, one of {@link DestructionReason}'s values.
     */
    public static void recordDestruction(@DestructionReason int reason) {
        RecordHistogram.recordEnumeratedHistogram("CustomTabs.DynamicModule.DestructionReason",
                reason, DestructionReason.NUM_ENTRIES);
        Log.d(TAG, "Destroyed module, reason: %s", reason);
    }

    /**
     * SystemClock.uptimeMillis() is used here as it uses the same system call as all the native
     * side of Chrome, and this is the same clock used for page load metrics.
     *
     * @return Milliseconds since boot, not counting time spent in deep sleep.
     */
    public static long now() {
        return SystemClock.uptimeMillis();
    }

    public static void recordCreateActivityDelegateTime(long startTime) {
        RecordHistogram.recordMediumTimesHistogram(
                "CustomTabs.DynamicModule.CreateActivityDelegateTime", now() - startTime);
    }

    public static void recordCreatePackageContextTime(long startTime) {
        RecordHistogram.recordMediumTimesHistogram(
                "CustomTabs.DynamicModule.CreatePackageContextTime", now() - startTime);
    }

    public static void recordLoadClassTime(long startTime) {
        RecordHistogram.recordMediumTimesHistogram(
                "CustomTabs.DynamicModule.EntryPointLoadClassTime", now() - startTime);
    }

    public static void recordEntryPointNewInstanceTime(long startTime) {
        RecordHistogram.recordMediumTimesHistogram(
                "CustomTabs.DynamicModule.EntryPointNewInstanceTime", now() - startTime);
    }

    public static void recordEntryPointInitTime(long startTime) {
        RecordHistogram.recordMediumTimesHistogram(
                "CustomTabs.DynamicModule.EntryPointInitTime", now() - startTime);
    }

    /**
     * Possible lifecycle states for a dynamic module.
     */
    @StringDef({LifecycleState.NOT_LOADED, LifecycleState.INSTANTIATED, LifecycleState.DESTROYED})
    public @interface LifecycleState {
        /** The module has not yet been loaded. */
        String NOT_LOADED = "NotLoaded";
        /** The module has been loaded and instantiated. */
        String INSTANTIATED = "Instantiated";
        /** The module instance has been destroyed. */
        String DESTROYED = "Destroyed";
    }

    /**
     * Registers the module lifecycle state in a synthetic field trial.
     * @param state The module lifecycle state.
     */
    public static void registerLifecycleState(@LifecycleState String state) {
        UmaSessionStats.registerSyntheticFieldTrial(LIFECYCLE_STATE_TRIAL_NAME, state);
    }
}
