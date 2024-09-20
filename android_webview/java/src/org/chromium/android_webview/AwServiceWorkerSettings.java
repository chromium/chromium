// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Process;
import android.webkit.WebSettings;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

import java.util.Collections;
import java.util.Set;

/**
 * Stores Android WebView Service Worker specific settings.
 *
 * Methods in this class can be called from any thread, including threads created by
 * the client of WebView.
 */
@Lifetime.Profile
public class AwServiceWorkerSettings {
    // Must be maximum 20 characters, hence the abbreviation
    private static final String TAG = "AwSWSettings";
    private static final boolean TRACE = false;

    private final AwBrowserContext mBrowserContext;
    private int mCacheMode = WebSettings.LOAD_DEFAULT;
    private boolean mAllowContentUrlAccess = true;
    private boolean mAllowFileUrlAccess = true;
    private boolean mBlockNetworkLoads; // Default depends on permission of the embedding APK
    private boolean mBlockSpecialFileUrls;

    private Set<String> mRequestedWithHeaderAllowedOriginRules;

    // Lock to protect all settings.
    private final Object mAwServiceWorkerSettingsLock = new Object();

    // Computed on construction.AwServiceWorkerSettingsTest
    private final boolean mHasInternetPermission;

    public AwServiceWorkerSettings(Context context, AwBrowserContext browserContext) {
        mBrowserContext = browserContext;
        boolean hasInternetPermission =
                context.checkPermission(
                                android.Manifest.permission.INTERNET,
                                Process.myPid(),
                                Process.myUid())
                        == PackageManager.PERMISSION_GRANTED;
        synchronized (mAwServiceWorkerSettingsLock) {
            mHasInternetPermission = hasInternetPermission;
            mBlockNetworkLoads = !hasInternetPermission;

            // The application context we receive in the sdk runtime is a separate
            // context from the context that actual SDKs receive (and contains asset
            // file links). This means file urls will not work in this environment.
            // Explicitly block this to cause confusion in the case of accidentally
            // hitting assets in the application context.
            mBlockSpecialFileUrls = ContextUtils.isSdkSandboxProcess();

            mRequestedWithHeaderAllowedOriginRules =
                    ManifestMetadataUtil.getXRequestedWithAllowList();
        }
    }

    /** See {@link android.webkit.ServiceWorkerWebSettings#setCacheMode}. */
    public void setCacheMode(int mode) {
        if (TRACE) Log.d(TAG, "setCacheMode=" + mode);
        synchronized (mAwServiceWorkerSettingsLock) {
            if (mCacheMode != mode) {
                mCacheMode = mode;
            }
        }
    }

    /** See {@link android.webkit.ServiceWorkerWebSettings#getCacheMode}. */
    public int getCacheMode() {
        synchronized (mAwServiceWorkerSettingsLock) {
            return mCacheMode;
        }
    }

    /** See {@link android.webkit.ServiceWorkerWebSettings#setAllowContentAccess}. */
    public void setAllowContentAccess(boolean allow) {
        if (TRACE) Log.d(TAG, "setAllowContentAccess=" + allow);
        synchronized (mAwServiceWorkerSettingsLock) {
            if (mAllowContentUrlAccess != allow) {
                mAllowContentUrlAccess = allow;
            }
        }
    }

    /** See {@link android.webkit.ServiceWorkerWebSettings#getAllowContentAccess}. */
    public boolean getAllowContentAccess() {
        synchronized (mAwServiceWorkerSettingsLock) {
            return mAllowContentUrlAccess;
        }
    }

    /** See {@link android.webkit.ServiceWorkerWebSettings#setAllowFileAccess}. */
    public void setAllowFileAccess(boolean allow) {
        if (TRACE) Log.d(TAG, "setAllowFileAccess=" + allow);
        synchronized (mAwServiceWorkerSettingsLock) {
            if (mAllowFileUrlAccess != allow) {
                mAllowFileUrlAccess = allow;
            }
        }
    }

    /** See {@link android.webkit.ServiceWorkerWebSettings#getAllowFileAccess}. */
    public boolean getAllowFileAccess() {
        synchronized (mAwServiceWorkerSettingsLock) {
            return mAllowFileUrlAccess;
        }
    }

    public void setBlockSpecialFileUrls(boolean block) {
        if (TRACE) Log.d(TAG, "setBlockSpecialFileUrls=" + block);
        synchronized (mAwServiceWorkerSettingsLock) {
            mBlockSpecialFileUrls = block;
        }
    }

    public boolean getBlockSpecialFileUrls() {
        synchronized (mAwServiceWorkerSettingsLock) {
            return mBlockSpecialFileUrls;
        }
    }

    /** See {@link android.webkit.ServiceWorkerWebSettings#setBlockNetworkLoads}. */
    public void setBlockNetworkLoads(boolean flag) {
        if (TRACE) Log.d(TAG, "setBlockNetworkLoads=" + flag);
        synchronized (mAwServiceWorkerSettingsLock) {
            if (!flag && !mHasInternetPermission) {
                throw new SecurityException(
                        "Permission denied - " + "application missing INTERNET permission");
            }
            mBlockNetworkLoads = flag;
        }
    }

    /** See {@link android.webkit.ServiceWorkerWebSettings#getBlockNetworkLoads}. */
    public boolean getBlockNetworkLoads() {
        synchronized (mAwServiceWorkerSettingsLock) {
            return mBlockNetworkLoads;
        }
    }

    /**
     * See {@link
     * androidx.webkit.ServiceWorkerWebSettingsCompat#setRequestedWithHeaderOriginAllowList}
     */
    public void setRequestedWithHeaderOriginAllowList(Set<String> allowedOriginRules) {
        // Even though clients shouldn't pass in null, it's better to guard against it
        allowedOriginRules =
                allowedOriginRules != null ? allowedOriginRules : Collections.emptySet();
        synchronized (mAwServiceWorkerSettingsLock) {
            AwWebContentsMetricsRecorder.recordRequestedWithHeaderModeServiceWorkerAPIUsage(
                    allowedOriginRules);
            Set<String> rejectedRules =
                    mBrowserContext.updateServiceWorkerXRequestedWithAllowListOriginMatcher(
                            allowedOriginRules);
            if (!rejectedRules.isEmpty()) {
                throw new IllegalArgumentException(
                        "Malformed origin match rules: " + rejectedRules);
            }
            mRequestedWithHeaderAllowedOriginRules = allowedOriginRules;
        }
    }

    /**
     * See {@link
     * androidx.webkit.ServiceWorkerWebSettingsCompat#getRequestedWithHeaderOriginAllowList}
     */
    public Set<String> getRequestedWithHeaderOriginAllowList() {
        synchronized (mAwServiceWorkerSettingsLock) {
            return mRequestedWithHeaderAllowedOriginRules;
        }
    }
}
