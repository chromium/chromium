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
    private static final String SAFE_BROWSING_OPT_IN_METADATA_NAME =
            "android.webkit.WebView.EnableSafeBrowsing";

    // Do not change value, it is used by external AndroidManifest.xml files
    private static final String METADATA_HOLDER_SERVICE_NAME =
            "android.webkit.MetaDataHolderService";
    // Do not change value, it is used by external AndroidManifest.xml files
    private static final String XRW_ALLOWLIST_METADATA_NAME =
            "REQUESTED_WITH_HEADER_ORIGIN_ALLOW_LIST";
    private static final String XRW_PARSING_ERROR_MESSAGE = "Value of meta-data "
            + XRW_ALLOWLIST_METADATA_NAME + " in service " + METADATA_HOLDER_SERVICE_NAME
            + " must be a resource ID referencing a string-array resource.";

    private static Set<String> sXRequestedWithAllowListOverride;

    /**
     * Find out if the App opted out from metrics collection using the meta-data tag.
     */
    public static boolean isAppOptedOutFromMetricsCollection() {
        Context ctx = ContextUtils.getApplicationContext();
        try {
            ApplicationInfo info = ctx.getPackageManager().getApplicationInfo(
                    ctx.getPackageName(), PackageManager.GET_META_DATA);
            if (info.metaData == null) {
                // null means no such tag was found.
                return false;
            }
            // getBoolean returns false if the key is not found, which is what we want.
            return info.metaData.getBoolean(METRICS_OPT_OUT_METADATA_NAME);
        } catch (PackageManager.NameNotFoundException e) {
            // This should never happen.
            Log.e(TAG, "App could not find itself by package name!");
            // The conservative thing is to assume the app HAS opted out.
            return true;
        }
    }

    /**
     * Checks the application manifest for Safe Browsing opt-in preference.
     *
     * @return true if app has opted in, false if opted out, and null if no preference specified.
     */
    @Nullable
    public static Boolean getSafeBrowsingAppOptInPreference() {
        Context ctx = ContextUtils.getApplicationContext();
        try {
            ApplicationInfo info = ctx.getPackageManager().getApplicationInfo(
                    ctx.getPackageName(), PackageManager.GET_META_DATA);
            if (info.metaData == null
                    || !info.metaData.containsKey(SAFE_BROWSING_OPT_IN_METADATA_NAME)) {
                // No <meta-data> tag was found.
                return null;
            }
            return info.metaData.getBoolean(SAFE_BROWSING_OPT_IN_METADATA_NAME);
        } catch (PackageManager.NameNotFoundException e) {
            // This should never happen.
            Log.e(TAG, "App could not find itself by package name!");
            return false;
        }
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
        if (sXRequestedWithAllowListOverride != null) {
            return sXRequestedWithAllowListOverride;
        }
        Context context = ContextUtils.getApplicationContext();
        Bundle bundle = getMetadataHolderServiceBundle(context);
        if (bundle != null) {
            return getXRequestedWithAllowListFromMetadata(context, bundle);
        }
        return Collections.emptySet();
    }

    /**
     * Pulls out X-Requested-With header from the metadata bundle, if present.
     * @param context Application context
     * @param bundle Bundle to extract the resource from
     * @return Set of strings, possibly empty if unable to find the key.
     */
    private static Set<String> getXRequestedWithAllowListFromMetadata(
            final Context context, Bundle bundle) {
        if (!bundle.containsKey(XRW_ALLOWLIST_METADATA_NAME)) {
            return Collections.emptySet();
        }
        int metadataResourceId = bundle.getInt(XRW_ALLOWLIST_METADATA_NAME);
        try {
            String[] stringArray = context.getResources().getStringArray(metadataResourceId);
            return new HashSet<>(Arrays.asList(stringArray));
        } catch (NotFoundException e) {
            throw new IllegalArgumentException(XRW_PARSING_ERROR_MESSAGE, e);
        }
    }

    /**
     * Get metadata bundle from ApplicationManifest.xml registered to
     * the {@link ManifestMetadataUtil#METADATA_HOLDER_SERVICE_NAME} service.
     *
     * @return Metadata bundle or {@code null} if no metadata was found.
     * @param context Application context
     */
    private static Bundle getMetadataHolderServiceBundle(final Context context) {
        int flags = PackageManager.GET_META_DATA;
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.N) {
            flags |= PackageManager.MATCH_DISABLED_COMPONENTS;
        }
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
     * @return AutoCloseable that will reset the value when closed.
     */
    public static AutoCloseable setXRequestedWithAllowListScopedForTesting(Set<String> allowList) {
        sXRequestedWithAllowListOverride = allowList;
        return () -> sXRequestedWithAllowListOverride = null;
    }
}
