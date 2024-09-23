// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.os.Build;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.support_lib_boundary.ProcessGlobalConfigConstants;

import java.lang.reflect.Field;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Class that contains the process global configuration information if it was set by the embedding
 * app using androidx.webkit.ProcessGlobalConfig.
 *
 * It is extracted once when WebView first loads and is used for global process configurations (
 * hence the name).
 */
@Lifetime.Singleton
public final class AndroidXProcessGlobalConfig {
    private String mDataDirectorySuffix;
    private String mDataDirectoryBasePath;
    private String mCacheDirectoryBasePath;
    private boolean mPartitionedCookiesEnabled;
    private static AndroidXProcessGlobalConfig sGlobalConfig;

    private AndroidXProcessGlobalConfig(@NonNull Map<String, Object> configMap) {
        for (Entry<String, Object> entry : configMap.entrySet()) {
            Object configValue = entry.getValue();
            switch (entry.getKey()) {
                case ProcessGlobalConfigConstants.DATA_DIRECTORY_SUFFIX:
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                        throw new RuntimeException(
                                "AndroidXProcessGlobalConfig map should not have value set for "
                                        + "key: "
                                        + entry.getKey()
                                        + " in SDK version >= "
                                        + Build.VERSION_CODES.P);
                    }
                    if (!(configValue instanceof String)) {
                        throw new RuntimeException(
                                "AndroidXProcessGlobalConfig map does not have "
                                        + "right type of value for key: "
                                        + entry.getKey());
                    }
                    mDataDirectorySuffix = (String) configValue;
                    break;
                case ProcessGlobalConfigConstants.DATA_DIRECTORY_BASE_PATH:
                    if (!(configValue instanceof String)) {
                        throw new RuntimeException(
                                "AndroidXProcessGlobalConfig map does not have "
                                        + "right type of value for key: "
                                        + entry.getKey());
                    }
                    mDataDirectoryBasePath = (String) configValue;
                    break;
                case ProcessGlobalConfigConstants.CACHE_DIRECTORY_BASE_PATH:
                    if (!(configValue instanceof String)) {
                        throw new RuntimeException(
                                "AndroidXProcessGlobalConfig map does not have "
                                        + "right type of value for key: "
                                        + entry.getKey());
                    }
                    mCacheDirectoryBasePath = (String) configValue;
                    break;
                case ProcessGlobalConfigConstants.CONFIGURE_PARTITIONED_COOKIES:
                    if (!(configValue instanceof Boolean)) {
                        throw new RuntimeException(
                                "AndroidXProcessGlobalConfig map does not have "
                                        + "right type of value for key: "
                                        + entry.getKey());
                    }
                    mPartitionedCookiesEnabled = (Boolean) configValue;
                    break;
                default:
                    throw new RuntimeException(
                            "AndroidXProcessGlobalConfig map contains unknown key: "
                                    + entry.getKey());
            }
        }
    }

    /**
     * Extracts the process global config that was set by the app in
     * androidx.webkit.ProcessGlobalConfig.
     * <p>
     * This method should only be called once.
     */
    public static void extractConfigFromApp(ClassLoader cl) {
        assert sGlobalConfig == null;
        HashMap<String, Object> configMap = null;
        try {
            Class<?> holder = Class.forName("androidx.webkit.ProcessGlobalConfig", true, cl);
            Field sProcessGlobalConfig = holder.getDeclaredField("sProcessGlobalConfig");
            sProcessGlobalConfig.setAccessible(true);
            AtomicReference<HashMap<String, Object>> configRef =
                    (AtomicReference<HashMap<String, Object>>) sProcessGlobalConfig.get(null);
            configMap = configRef.get();
        } catch (Exception e) {
            // The class probably doesn't exist - the app may not be using the AndroidX library,
            // or not a recent enough version.
        }
        if (configMap == null) {
            sGlobalConfig = new AndroidXProcessGlobalConfig(Collections.emptyMap());
        } else {
            sGlobalConfig = new AndroidXProcessGlobalConfig(configMap);
        }
    }

    /**
     * Gets the process global config that was set by the app in
     * androidx.webkit.ProcessGlobalConfig.
     * <p>
     * This method should be called after calling {@link #extractConfigFromApp(ClassLoader)}
     */
    public static @NonNull AndroidXProcessGlobalConfig getConfig() {
        assert sGlobalConfig != null;
        return sGlobalConfig;
    }

    public @Nullable String getDataDirectorySuffixOrNull() {
        return mDataDirectorySuffix;
    }

    public @Nullable String getDataDirectoryBasePathOrNull() {
        return mDataDirectoryBasePath;
    }

    public @Nullable String getCacheDirectoryBasePathOrNull() {
        return mCacheDirectoryBasePath;
    }

    public boolean getPartitionedCookiesEnabled() {
        return mPartitionedCookiesEnabled;
    }
}
