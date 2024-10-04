// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources.NotFoundException;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * Utility class to fetch metadata declared in the ApplicationManifest.xml file of the embedding
 * app.
 */
public class ManifestMetadataUtil {
    private static final String TAG = "ManifestMetadata";

    // Individual apps can use this meta-data tag in their manifest to opt out of metrics
    // reporting. See https://developer.android.com/reference/android/webkit/WebView.html
    private static final String METRICS_OPT_OUT_METADATA_NAME =
            "android.webkit.WebView.MetricsOptOut";
    private static final String CONTEXT_EXPERIMENT_OPT_OUT_METADATA_NAME =
            "android.webkit.WebView.WebViewContextOptOut";
    private static final String SAFE_BROWSING_OPT_IN_METADATA_NAME =
            "android.webkit.WebView.EnableSafeBrowsing";

    // Do not change value, it is used by external AndroidManifest.xml files
    private static final String MULTI_PROFILE_NAME_TAG_KEY_METADATA_NAME =
            "android.webkit.WebView.MultiProfileNameTagKey";

    // Do not change value, it is used by external AndroidManifest.xml files
    private static final String METADATA_HOLDER_SERVICE_NAME =
            "android.webkit.MetaDataHolderService";
    // Do not change value, it is used by external AndroidManifest.xml files
    private static final String XRW_ALLOWLIST_METADATA_NAME =
            "REQUESTED_WITH_HEADER_ORIGIN_ALLOW_LIST";
    private static final String XRW_PARSING_ERROR_MESSAGE =
            "Value of meta-data "
                    + XRW_ALLOWLIST_METADATA_NAME
                    + " in service "
                    + METADATA_HOLDER_SERVICE_NAME
                    + " must be a resource ID referencing a string-array resource.";

    /** Used in tests. */
    @Nullable private static Set<String> sXrwAllowlistForTesting;

    @Nullable private static volatile MetadataCache sMetadataCache;

    /**
     * Cache for all AndroidManifest.xml meta-data. All meta-data should be fetched at the time this
     * class is constructed.
     */
    @VisibleForTesting
    public static class MetadataCache {
        private final boolean mIsAppOptedOutFromMetricsCollection;
        private final boolean mIsAppOptedOutFromContextExperiment;
        private final @Nullable Boolean mSafeBrowsingOptInPreference;
        private final @Nullable Integer mAppMultiProfileProfileNameTagKey;
        private final @NonNull Set<String> mXRequestedAllowList;

        public MetadataCache(@NonNull Context context) {
            // Cache app level metadata.
            @Nullable Bundle appMetadata = getAppMetadata(context);
            mIsAppOptedOutFromMetricsCollection = isAppOptedOutFromMetricsCollection(appMetadata);
            mSafeBrowsingOptInPreference = getSafeBrowsingAppOptInPreference(appMetadata);

            // Holder service metadata.
            @Nullable
            Bundle metadataHolderServiceMetadata = getMetadataHolderServiceMetadata(context);
            mAppMultiProfileProfileNameTagKey =
                    getAppMultiProfileProfileNameTagKey(metadataHolderServiceMetadata);
            mIsAppOptedOutFromContextExperiment =
                    isAppOptedOutFromContextExperiment(metadataHolderServiceMetadata);
            mXRequestedAllowList =
                    getXRequestedWithAllowList(context, metadataHolderServiceMetadata);
        }
    }

    /**
     * Used to ensure the Metadata cache is initialized. No-op if the cache already exists. Will
     * initialize the cache with the given context otherwise.
     */
    public static void ensureMetadataCacheInitialized(Context context) {
        if (sMetadataCache == null) {
            sMetadataCache = new MetadataCache(context);
        }
    }

    @VisibleForTesting
    public static MetadataCache getMetadataCache() {
        if (sMetadataCache == null) {
            sMetadataCache = new MetadataCache(ContextUtils.getApplicationContext());
        }
        return sMetadataCache;
    }

    /**
     * Caches all of the manifest metadata values. Called lazily if any of the public metadata
     * accessor method values are not inside of the cache at the time that they are invoked.
     */

    /** Find out if the App opted out from metrics collection using the meta-data tag. */
    public static boolean isAppOptedOutFromMetricsCollection() {
        return getMetadataCache().mIsAppOptedOutFromMetricsCollection;
    }

    @VisibleForTesting
    public static boolean isAppOptedOutFromMetricsCollection(@Nullable Bundle appMetadata) {
        boolean value;
        if (appMetadata == null) {
            // The conservative thing is to assume the app HAS opted out.
            value = true;
        } else {
            // getBoolean returns false if the key is not found, which is what we want.
            value = appMetadata.getBoolean(METRICS_OPT_OUT_METADATA_NAME);
        }
        return value;
    }

    /**
     * Checks the application manifest for WebView's context experiment opt-out preference.
     *
     * @return true if the app has opted out, false otherwise.
     */
    public static boolean isAppOptedOutFromContextExperiment() {
        return getMetadataCache().mIsAppOptedOutFromContextExperiment;
    }

    @VisibleForTesting
    public static boolean isAppOptedOutFromContextExperiment(
            @Nullable Bundle metadataHolderServiceMetadata) {
        boolean value = false;
        if (metadataHolderServiceMetadata != null
                && metadataHolderServiceMetadata.containsKey(
                        CONTEXT_EXPERIMENT_OPT_OUT_METADATA_NAME)) {
            value =
                    metadataHolderServiceMetadata.getBoolean(
                            CONTEXT_EXPERIMENT_OPT_OUT_METADATA_NAME);
        }
        return value;
    }

    /**
     * Checks the application manifest for Safe Browsing opt-in preference.
     *
     * @return true if app has opted in, false if opted out, and null if no preference specified.
     */
    @Nullable
    public static Boolean getSafeBrowsingAppOptInPreference() {
        return getMetadataCache().mSafeBrowsingOptInPreference;
    }

    @VisibleForTesting
    @Nullable
    public static Boolean getSafeBrowsingAppOptInPreference(@Nullable Bundle appMetadata) {
        Boolean value;
        if (appMetadata == null || !appMetadata.containsKey(SAFE_BROWSING_OPT_IN_METADATA_NAME)) {
            // No <meta-data> tag was found.
            value = null;
        } else {
            value = appMetadata.getBoolean(SAFE_BROWSING_OPT_IN_METADATA_NAME);
        }
        return value;
    }

    /**
     * Returns the tag key which will be used to retrieve the name of profile to associate with a
     * WebView when it is initialized. The app will have needed to override {@link
     * android.webkit.WebView#getTag(int)} in order to gain the benefits of this.
     *
     * @return the tag key for the profile name if provided by the app, otherwise null.
     */
    @Nullable
    public static Integer getAppMultiProfileProfileNameTagKey() {
        return getMetadataCache().mAppMultiProfileProfileNameTagKey;
    }

    @VisibleForTesting
    @Nullable
    public static Integer getAppMultiProfileProfileNameTagKey(
            @Nullable Bundle metadataHolderServiceMetadata) {
        Integer value;
        if (metadataHolderServiceMetadata != null
                && metadataHolderServiceMetadata.containsKey(
                        MULTI_PROFILE_NAME_TAG_KEY_METADATA_NAME)) {
            value = metadataHolderServiceMetadata.getInt(MULTI_PROFILE_NAME_TAG_KEY_METADATA_NAME);
        } else {
            value = null;
        }
        return value;
    }

    /**
     * Get the configured allow-list for X-Requested-With origins, if present, otherwise {@code
     * null}.
     *
     * The allowlist should be declared in the manifest with the snippet
     * <pre>
     *    &lt;service android:name="androidx.webkit.MetaDataHolderService"
     *         android:enabled="false"
     *         android:exported="false"&gt;
     *       &lt;meta-data
     *           android:name=
     *             "androidx.webkit.MetaDataHolderService.REQUESTED_WITH_HEADER_ORIGIN_ALLOW_LIST"
     *           android:resource="@array/xrw_origin_allowlist"/&gt;
     *     &lt;/service&gt;
     * </pre>
     * where {@code @array/xrw_origin_allowlist} should be a resource of the type {@code
     * string-array}.
     *
     * @return Allowlist to use by default.
     */
    @NonNull
    public static Set<String> getXRequestedWithAllowList() {
        if (sXrwAllowlistForTesting != null) {
            return sXrwAllowlistForTesting;
        }
        return getMetadataCache().mXRequestedAllowList;
    }

    /**
     * Pulls out X-Requested-With header from the metadata bundle, if present, and caches it. Will
     * cache an empty Set if unable to find the key.
     *
     * @param context Application context.
     * @param metadataHolderServiceBundle the metadata holder service bundle to extract the resource
     *     from.
     */
    @NonNull
    @VisibleForTesting
    public static Set<String> getXRequestedWithAllowList(
            final Context context, final @Nullable Bundle metadataHolderServiceBundle) {
        if (metadataHolderServiceBundle == null
                || !metadataHolderServiceBundle.containsKey(XRW_ALLOWLIST_METADATA_NAME)) {
            return Collections.emptySet();
        }
        int metadataResourceId = metadataHolderServiceBundle.getInt(XRW_ALLOWLIST_METADATA_NAME);
        try {
            String[] stringArray = context.getResources().getStringArray(metadataResourceId);
            return new HashSet<>(Arrays.asList(stringArray));
        } catch (NotFoundException e) {
            throw new IllegalArgumentException(XRW_PARSING_ERROR_MESSAGE, e);
        }
    }

    /**
     * Get the app level metadata bundle from the AndroidManifest.
     *
     * @return Metadata bundle or {@code null} if no metadata was found;
     * @param context the Application context.
     */
    @VisibleForTesting
    @Nullable
    public static Bundle getAppMetadata(final Context context) {
        try {
            ApplicationInfo info =
                    context.getPackageManager()
                            .getApplicationInfo(
                                    context.getPackageName(), PackageManager.GET_META_DATA);
            return info.metaData;
        } catch (NameNotFoundException e) {
            // This should never happen.
            Log.e(TAG, "App could not find itself by package name!");
            return null;
        }
    }

    /**
     * Get metadata bundle from ApplicationManifest.xml registered to the {@link
     * ManifestMetadataUtil#METADATA_HOLDER_SERVICE_NAME} service.
     *
     * @return Metadata bundle or {@code null} if no metadata was found.
     * @param context the Application context.
     */
    @Nullable
    @VisibleForTesting
    public static Bundle getMetadataHolderServiceMetadata(final Context context) {
        int flags = PackageManager.GET_META_DATA | PackageManager.MATCH_DISABLED_COMPONENTS;
        try {
            return context.getPackageManager()
                    .getServiceInfo(new ComponentName(context, METADATA_HOLDER_SERVICE_NAME), flags)
                    .metaData;
        } catch (NameNotFoundException e) {
            return null;
        }
    }

    /**
     * Set the value to be returned by {@link ManifestMetadataUtil#getXRequestedWithAllowList()}.
     *
     * @return AutoCloseable that will reset the value when closed.
     */
    public static AutoCloseable setXRequestedWithAllowListScopedForTesting(
            @NonNull Set<String> allowList) {
        sXrwAllowlistForTesting = allowList;
        return () -> sXrwAllowlistForTesting = null;
    }
}
