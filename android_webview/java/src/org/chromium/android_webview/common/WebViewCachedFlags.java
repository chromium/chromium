// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import static org.chromium.build.NullUtil.assumeNonNull;

import static java.lang.annotation.ElementType.TYPE_USE;

import android.content.SharedPreferences;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.BaseFeatures;
import org.chromium.base.FeatureList;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.services.tracing.TracingServiceFeatures;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
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
    private volatile boolean mIsStartupComplete;

    /*
     * These sets keep track of which features have been logged to the UMA histograms
     * "Variations.FeatureAccess" and "Variations.FeatureAccessEarly", respectively.
     * This ensures that we only log each feature access once per process lifetime.
     * This is done for performance reasons, to avoid hashing the feature name on every access.
     */
    private final Set<String> mFeaturesLoggedGeneral = Collections.synchronizedSet(new HashSet<>());
    private final Set<String> mFeaturesLoggedEarly = Collections.synchronizedSet(new HashSet<>());

    /*
     * ThreadLocal to hold a MessageDigest instance for hashing feature names. This is used to avoid
     * the cost of creating a new MessageDigest instance on every access. It needs to be a
     * ThreadLocal because MessageDigest object is not thread-safe.
     */
    private static final ThreadLocal<MessageDigest> sMessageDigest =
            new ThreadLocal<MessageDigest>() {
                @Override
                protected MessageDigest initialValue() {
                    try {
                        return MessageDigest.getInstance("SHA-1");
                    } catch (NoSuchAlgorithmException e) {
                        throw new RuntimeException("SHA-1 not supported", e);
                    }
                }
            };

    /**
     * Initializes the singleton instance and reads the cached values from prefs. This method must
     * be called before get().
     *
     * @param prefs the SharedPreferences from which to initialize the caches.
     */
    public static void init(SharedPreferences prefs) {
        synchronized (sLock) {
            assert sInstance == null : "Cannot call WebViewCachedFlags.init more than once.";
            initInternal(prefs);
        }
    }

    /**
     * Initializes the singleton instance and reads the cached values from prefs. This method must
     * be called before get(). This method allows reinitialization for testing purposes. Must be
     * used for testing only.
     *
     * @param prefs the SharedPreferences from which to initialize the caches.
     */
    public static void initForTesting(SharedPreferences prefs) {
        initInternal(prefs);
    }

    private static void initInternal(SharedPreferences prefs) {
        synchronized (sLock) {
            sInstance =
                    new WebViewCachedFlags(
                            prefs,
                            // Add new CachedFlags here along with their default state.
                            Map.ofEntries(
                                    Map.entry(
                                            AwFeatures.WEBVIEW_BACKGROUND_CLASS_PRELOADING,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_MOVE_WORK_TO_PROVIDER_INIT,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_EARLY_TRACING_INIT,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_BACKGROUND_TRACING_INIT,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_EARLY_STARTUP_TRACING,
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
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_ENABLE_API_CALL_USER_ACTIONS,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_USE_NONEMBEDDED_LOW_ENTROPY_SOURCE,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_FASTER_GET_DEFAULT_USER_AGENT,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            BaseFeatures
                                                    .SHUTDOWN_PRE_NATIVE_THREAD_POOL_AFTER_STARTUP,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.STARTUP_NON_BLOCKING_WEBVIEW_CONSTRUCTOR,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.POST_CHROMIUM_STARTUP_IN_WEBVIEW_CONSTRUCTOR,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_STATIC_METHODS_NOT_TRIGGER_STARTUP,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_REMOVE_INSTANT_APP_SUPPORT,
                                            DefaultState.DISABLED),
                                    Map.entry(
                                            AwFeatures.WEBVIEW_PROFILE_STORE_NOT_TRIGGER_STARTUP,
                                            DefaultState.DISABLED)));
        }
    }

    /**
     * Initializes cached flags singleton instance and uses the default values for all experiments.
     *
     * @param prefs the SharedPreferences which will be cleared during initialization.
     */
    public static void initForSafeMode(SharedPreferences prefs) {
        init(prefs);
        // Once regular init has finished, reset both enabled and disabled sets so that every flag
        // uses its default value.
        assumeNonNull(sInstance).resetToDefaults();
    }

    /** Forces all experiments to use their default values. */
    @VisibleForTesting
    public void resetToDefaults() {
        mOverrideEnabled.clear();
        mOverrideDisabled.clear();
        mFeaturesLoggedGeneral.clear();
        mFeaturesLoggedEarly.clear();
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

    private void logFeatureAccess(String feature) {
        int featureHash = 0;
        boolean featureHashInitialized = false;

        // We always log "Variations.FeatureAccess" for every feature access, regardless of when it
        // happens. This matches the behavior of the C++ base::FeatureList.
        if (mFeaturesLoggedGeneral.add(feature)) {
            featureHash = hashFieldTrialName(feature);
            featureHashInitialized = true;
            RecordHistogram.recordSparseHistogram("Variations.FeatureAccess", featureHash);
        }

        // In addition to the general access log, we also log "Variations.FeatureAccessEarly" if the
        // feature is accessed before startup is complete.
        if (!mIsStartupComplete && mFeaturesLoggedEarly.add(feature)) {
            if (!featureHashInitialized) {
                featureHash = hashFieldTrialName(feature);
                featureHashInitialized = true;
            }
            RecordHistogram.recordSparseHistogram("Variations.FeatureAccessEarly", featureHash);
        }
    }

    /**
     * @param feature the name of the feature to query.
     * @return true if feature is enabled in the cache, false if it is disabled and the default
     *     value from mDefaults otherwise. Throws IllegalArgumentException if the flag is not
     *     registered in mDefaults or one of the caches.
     */
    public boolean isCachedFeatureEnabled(String feature) {
        logFeatureAccess(feature);
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
        mIsStartupComplete = true;
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
            // This flag has been cleaned up now so we don't need to add it to enabled set. Just
            // remove the pref.
            editor.remove("webViewUseStartupTasksLogic");
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

    /**
     * Computes the hash of a feature name. This must be kept in sync with base::HashFieldTrialName
     * in base/metrics/metrics_hashes.cc.
     *
     * <p>Note: RecordHistogram calls are buffered until native is initialized. Since we are
     * providing the logged value directly to the histogram here, we need to compute it in Java.
     */
    @VisibleForTesting
    public static int hashFieldTrialName(String feature) {
        byte[] featureBytes = feature.getBytes(StandardCharsets.UTF_8);
        byte[] hash = sMessageDigest.get().digest(featureBytes);
        // Read the first 4 bytes as a little-endian 32-bit integer.
        return (hash[0] & 0xFF)
                | ((hash[1] & 0xFF) << 8)
                | ((hash[2] & 0xFF) << 16)
                | ((hash[3] & 0xFF) << 24);
    }

    @NativeMethods
    interface Natives {
        @JniType("std::optional<bool>")
        @Nullable Boolean getStateIfOverridden(@JniType("std::string") String feature);
    }
}
