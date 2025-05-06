// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import static java.lang.annotation.ElementType.TYPE_USE;

import android.content.SharedPreferences;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.AwFeatureMap;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.Collections;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * This class provides a mechanism for accessing flags before native initialization. It uses
 * SharedPreferences to cache two Sets of feature flag names. Once they're read from disk at
 * startup, the disk caches are deleted immediately so that if a crash happens, we don't use the
 * same flag configuration on the next startup. Once startup is completed, we write new values for
 * each flag to disk by querying finch.
 *
 * <p>To use this class, add your feature to the Map in init. You can then query your feature's
 * value in code using {@code WebViewCachedFlags.get().isCachedFeatureEnabled(FEATURE_NAME);}. Note:
 * if you use a feature through the cached mechanism you must not query its value through finch as
 * the two may differ in their values.
 */
@NullMarked
public class WebViewCachedFlags {
    private static final String CACHED_ENABLED_FLAGS_PREF = "CachedFlagsEnabled";
    private static final String CACHED_DISABLED_FLAGS_PREF = "CachedFlagsDisabled";
    private static final String MIGRATION_HISTOGRAM_NAME = "Android.WebView.CachedFlagMigration";

    @IntDef({DefaultState.DISABLED, DefaultState.ENABLED})
    @Retention(RetentionPolicy.SOURCE)
    @Target(TYPE_USE)
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public @interface DefaultState {
        int DISABLED = 0;
        int ENABLED = 1;
    }

    private static @Nullable WebViewCachedFlags sInstance;
    private static final Object sLock = new Object();

    private final Map<String, @DefaultState Integer> mDefaults;
    private final Set<String> mOverrideEnabled;
    private final Set<String> mOverrideDisabled;

    /**
     * Initializes the singleton instance and reads the cached values from prefs. This method must
     * be called before get().
     *
     * @param prefs the SharedPreferences from which to initialize the caches.
     */
    public static void init(SharedPreferences prefs) {
        synchronized (sLock) {
            if (sInstance != null) {
                throw new IllegalStateException(
                        "Cannot call WebViewCachedFlags.init more than once.");
            }
            sInstance =
                    new WebViewCachedFlags(
                            prefs,
                            Map.of(
                                    // Add new CachedFlags here along with their default state.
                                    AwFeatures.WEBVIEW_SEPARATE_RESOURCE_CONTEXT,
                                            DefaultState.DISABLED,
                                    AwFeatures.WEBVIEW_DISABLE_CHIPS, DefaultState.DISABLED,
                                    AwFeatures.WEBVIEW_USE_STARTUP_TASKS_LOGIC,
                                            DefaultState.DISABLED));
        }
    }

    /**
     * @return the singleton instance of this class.
     */
    public static WebViewCachedFlags get() {
        synchronized (sLock) {
            if (sInstance == null) {
                throw new IllegalStateException(
                        "Can't get WebViewCachedFlags instance before init is called");
            }
            return sInstance;
        }
    }

    /**
     * @param feature the name of the feature to query.
     * @return true if feature is enabled in the cache, false if it is disabled and the default
     *     value from mDefaults otherwise. Throws IllegalArgumentException if the flag is not
     *     registered in mDefaults or one of the caches.
     */
    public boolean isCachedFeatureEnabled(String feature) {
        if (mOverrideEnabled.contains(feature)) {
            return true;
        } else if (mOverrideDisabled.contains(feature)) {
            return false;
        } else if (mDefaults.containsKey(feature)) {
            return mDefaults.get(feature) == DefaultState.ENABLED;
        }
        throw new IllegalArgumentException("Cached feature not registered");
    }

    /**
     * Writes new finch values to prefs. This method should be called from a background thread.
     *
     * @param prefs the SharedPreferences to write new feature values to.
     */
    public void onStartupCompleted(SharedPreferences prefs) {
        Set<String> newEnabledSet = new HashSet<>();
        Set<String> newDisabledSet = new HashSet<>();
        mDefaults.forEach(
                (String feature, @DefaultState Integer value) -> {
                    if (AwFeatureMap.isEnabled(feature)) {
                        newEnabledSet.add(feature);
                    } else {
                        newDisabledSet.add(feature);
                    }
                });
        prefs.edit()
                .putStringSet(CACHED_ENABLED_FLAGS_PREF, newEnabledSet)
                .putStringSet(CACHED_DISABLED_FLAGS_PREF, newDisabledSet)
                .apply();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public WebViewCachedFlags(
            SharedPreferences prefs, Map<String, @DefaultState Integer> defaults) {
        // TODO(crbug.com/414342590): Remove the call to HashSet constructor once the migration code
        // is removed.
        mOverrideEnabled =
                new HashSet<>(
                        prefs.getStringSet(CACHED_ENABLED_FLAGS_PREF, Collections.emptySet()));
        mOverrideDisabled =
                new HashSet<>(
                        prefs.getStringSet(CACHED_DISABLED_FLAGS_PREF, Collections.emptySet()));
        SharedPreferences.Editor editor = prefs.edit();
        cleanUpOldManualExperiments(prefs, editor);
        editor.remove(CACHED_ENABLED_FLAGS_PREF).remove(CACHED_DISABLED_FLAGS_PREF).apply();
        mDefaults = defaults;
    }

    /**
     * Before this generic mechanism was written, a number of early startup experiments used
     * individual prefs to read experiment state. By migrating to the generic mechanism, we may
     * leave many clients with old preferences on their devices. This method cleans up any old
     * preferences from the manual experiments. It also uses the state of the old preference to
     * carry forward the client's experiment state so that we don't revert them to the default
     * behavior for a single startup.
     *
     * @param prefs the SharedPreferences object used to initialize this class.
     * @param editor SharedPreferences.Editor used to make modifications to prefs.
     */
    // TODO(crbug.com/414342590): Remove this method once migrations are near 0.
    private void cleanUpOldManualExperiments(
            SharedPreferences prefs, SharedPreferences.Editor editor) {
        boolean didMigration = false;
        if (prefs.contains("useWebViewResourceContext")) {
            // If this pref is present, we should enable the WEBVIEW_SEPARATE_RESOURCE_CONTEXT flag
            // for this run of WebView.
            editor.remove("useWebViewResourceContext");
            mOverrideDisabled.remove(AwFeatures.WEBVIEW_SEPARATE_RESOURCE_CONTEXT);
            mOverrideEnabled.add(AwFeatures.WEBVIEW_SEPARATE_RESOURCE_CONTEXT);
            didMigration = true;
        }
        if (prefs.contains("defaultWebViewPartitionedCookiesState")) {
            // If this pref is present, we want to default to not using CHIPS so enable the
            // WEBVIEW_DISABLE_CHIPS flag.
            editor.remove("defaultWebViewPartitionedCookiesState");
            mOverrideDisabled.remove(AwFeatures.WEBVIEW_DISABLE_CHIPS);
            mOverrideEnabled.add(AwFeatures.WEBVIEW_DISABLE_CHIPS);
            didMigration = true;
        }
        if (prefs.contains("webViewUseStartupTasksLogic")) {
            // If this pref is present, we should enable the WEBVIEW_USE_STARTUP_TASKS_LOGIC flag
            // for this run of WebView.
            editor.remove("webViewUseStartupTasksLogic");
            mOverrideDisabled.remove(AwFeatures.WEBVIEW_USE_STARTUP_TASKS_LOGIC);
            mOverrideEnabled.add(AwFeatures.WEBVIEW_USE_STARTUP_TASKS_LOGIC);
            didMigration = true;
        }
        RecordHistogram.recordBooleanHistogram(MIGRATION_HISTOGRAM_NAME, didMigration);
    }
}
