// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import static java.lang.annotation.ElementType.TYPE_USE;

import android.content.SharedPreferences;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureList;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.services.tracing.TracingServiceFeatures;

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
@JNINamespace("android_webview")
@NullMarked
public class WebViewCachedFlags {
    private static final String CACHED_ENABLED_FLAGS_PREF = "CachedFlagsEnabled";
    private static final String CACHED_DISABLED_FLAGS_PREF = "CachedFlagsDisabled";
    private static final String MIGRATION_HISTOGRAM_NAME = "Android.WebView.CachedFlagMigration";
    private static final String CACHED_FLAGS_EXIST_HISTOGRAM_NAME =
            "Android.WebView.CachedFlagsExist";

    @IntDef({DefaultState.DISABLED, DefaultState.ENABLED})
    @Retention(RetentionPolicy.SOURCE)
    @Target(TYPE_USE)
    @VisibleForTesting
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
                            // Add new CachedFlags here along with their default state.
                            Map.ofEntries(
                                    Map.entry(
                                            AwFeatures.WEBVIEW_MOVE_WORK_TO_PROVIDER_INIT,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_EARLY_PERFETTO_INIT,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_EARLY_STARTUP_TRACING,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_USE_STARTUP_TASKS_LOGIC,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_USE_STARTUP_TASKS_LOGIC_P2,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_STARTUP_TASKS_YIELD_TO_NATIVE,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_REDUCED_SEED_EXPIRATION,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_REDUCED_SEED_REQUEST_PERIOD,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            TracingServiceFeatures.ENABLE_PERFETTO_SYSTEM_TRACING,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_BYPASS_PROVISIONAL_COOKIE_MANAGER,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures
                                                    .WEBVIEW_OPT_IN_TO_GMS_BIND_SERVICE_OPTIMIZATION,
                                            DefaultState.DISABLED)));
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
     * @param feature the name of the feature to query.
     * @return true if feature is overridden in the cache i.e the client was actually in one of the
     *     experiment arms, false if it is not overridden.
     */
    @VisibleForTesting
    public boolean isCachedFeatureOverridden(String feature) {
        return mOverrideEnabled.contains(feature) || mOverrideDisabled.contains(feature);
    }

    /** Helper method to be called by native to check feature values without the instance. */
    @CalledByNative
    private static boolean isFeatureOverridden(@JniType("std::string") String feature) {
        return get().isCachedFeatureOverridden(feature);
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
                    Boolean overrideState = getStateIfOverridden(feature);
                    if (overrideState != null) {
                        if (overrideState) {
                            newEnabledSet.add(feature);
                        } else {
                            newDisabledSet.add(feature);
                        }
                    }
                });
        prefs.edit()
                .putStringSet(CACHED_ENABLED_FLAGS_PREF, newEnabledSet)
                .putStringSet(CACHED_DISABLED_FLAGS_PREF, newDisabledSet)
                .apply();
    }

    @VisibleForTesting
    public WebViewCachedFlags(
            SharedPreferences prefs, Map<String, @DefaultState Integer> defaults) {
        boolean flagsExist =
                prefs.contains(CACHED_ENABLED_FLAGS_PREF)
                        && prefs.contains(CACHED_DISABLED_FLAGS_PREF);
        RecordHistogram.recordBooleanHistogram(CACHED_FLAGS_EXIST_HISTOGRAM_NAME, flagsExist);
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

    /** Helper method to be called by native to check feature values without the instance. */
    @CalledByNative
    private static boolean isFeatureEnabled(@JniType("std::string") String feature) {
        return get().isCachedFeatureEnabled(feature);
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
            // This flag has been cleaned up now so we don't need to add it to enabled set. Just
            // remove the pref.
            editor.remove("useWebViewResourceContext");
            didMigration = true;
        }
        if (prefs.contains("defaultWebViewPartitionedCookiesState")) {
            // This flag has been cleaned up now so we don't need to add it to enabled set. Just
            // remove the pref.
            editor.remove("defaultWebViewPartitionedCookiesState");
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

    /**
     * @param feature the name of the feature to query.
     * @return null if the feature is not overridden, which means that the client is not a part of
     *     the study. Otherwise returns true if the feature is enabled or false if it is disabled.
     */
    private @Nullable Boolean getStateIfOverridden(String feature) {
        if (!FeatureList.isNativeInitialized()) {
            return FeatureOverrides.getTestValueForFeature(feature);
        }

        return WebViewCachedFlagsJni.get().getStateIfOverridden(feature);
    }

    @NativeMethods
    interface Natives {
        @JniType("std::optional<bool>")
        @Nullable Boolean getStateIfOverridden(@JniType("std::string") String feature);
    }
}
