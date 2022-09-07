// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Process;
import android.webkit.WebSettings;

import org.chromium.android_webview.settings.RequestedWithHeaderMode;
import org.chromium.base.Log;
import org.chromium.base.annotations.JNINamespace;

/**
 * Stores Android WebView Service Worker specific settings.
 *
 * Methods in this class can be called from any thread, including threads created by
 * the client of WebView.
 */
@JNINamespace("android_webview")
public class AwServiceWorkerSettings {
    // Must be maximum 20 characters, hence the abbreviation
    private static final String TAG = "AwSWSettings";
    private static final boolean TRACE = false;

    private int mCacheMode = WebSettings.LOAD_DEFAULT;
    private boolean mAllowContentUrlAccess = true;
    private boolean mAllowFileUrlAccess = true;
    private boolean mBlockNetworkLoads;  // Default depends on permission of the embedding APK
    private boolean mAcceptThirdPartyCookies;

    @RequestedWithHeaderMode
    private int mRequestedWithHeaderMode;

    // Lock to protect all settings.
    private final Object mAwServiceWorkerSettingsLock = new Object();

    // Computed on construction.
    private final boolean mHasInternetPermission;

    public AwServiceWorkerSettings(Context context) {
        boolean hasInternetPermission = context.checkPermission(
                android.Manifest.permission.INTERNET,
                Process.myPid(),
                Process.myUid()) == PackageManager.PERMISSION_GRANTED;
        synchronized (mAwServiceWorkerSettingsLock) {
            mHasInternetPermission = hasInternetPermission;
            mBlockNetworkLoads = !hasInternetPermission;
            mRequestedWithHeaderMode = AwSettings.getDefaultXRequestedWithHeaderMode();
        }
    }

    /**
     * See {@link android.webkit.ServiceWorkerWebSettings#setCacheMode}.
     */
    public void setCacheMode(int mode) {
        if (TRACE) Log.d(TAG, "setCacheMode=" + mode);
        synchronized (mAwServiceWorkerSettingsLock) {
            if (mCacheMode != mode) {
                mCacheMode = mode;
            }
        }
    }

    /**
     * See {@link android.webkit.ServiceWorkerWebSettings#getCacheMode}.
     */
    public int getCacheMode() {
        synchronized (mAwServiceWorkerSettingsLock) {
            return mCacheMode;
        }
    }

    /**
     * See {@link android.webkit.ServiceWorkerWebSettings#setAllowContentAccess}.
     */
    public void setAllowContentAccess(boolean allow) {
        if (TRACE) Log.d(TAG, "setAllowContentAccess=" + allow);
        synchronized (mAwServiceWorkerSettingsLock) {
            if (mAllowContentUrlAccess != allow) {
                mAllowContentUrlAccess = allow;
            }
        }
    }

    /**
     * See {@link android.webkit.ServiceWorkerWebSettings#getAllowContentAccess}.
     */
    public boolean getAllowContentAccess() {
        synchronized (mAwServiceWorkerSettingsLock) {
            return mAllowContentUrlAccess;
        }
    }

    /**
     * See {@link android.webkit.ServiceWorkerWebSettings#setAllowFileAccess}.
     */
    public void setAllowFileAccess(boolean allow) {
        if (TRACE) Log.d(TAG, "setAllowFileAccess=" + allow);
        synchronized (mAwServiceWorkerSettingsLock) {
            if (mAllowFileUrlAccess != allow) {
                mAllowFileUrlAccess = allow;
            }
        }
    }

    /**
     * See {@link android.webkit.ServiceWorkerWebSettings#getAllowFileAccess}.
     */
    public boolean getAllowFileAccess() {
        synchronized (mAwServiceWorkerSettingsLock) {
            return mAllowFileUrlAccess;
        }
    }

    /**
     * See {@link android.webkit.ServiceWorkerWebSettings#setBlockNetworkLoads}.
     */
    public void setBlockNetworkLoads(boolean flag) {
        if (TRACE) Log.d(TAG, "setBlockNetworkLoads=" + flag);
        synchronized (mAwServiceWorkerSettingsLock) {
            if (!flag && !mHasInternetPermission) {
                throw new SecurityException("Permission denied - "
                        + "application missing INTERNET permission");
            }
            mBlockNetworkLoads = flag;
        }
    }

    /**
     * See {@link android.webkit.ServiceWorkerWebSettings#getBlockNetworkLoads}.
     */
    public boolean getBlockNetworkLoads() {
        synchronized (mAwServiceWorkerSettingsLock) {
            return mBlockNetworkLoads;
        }
    }

    /**
     * See {@link androidx.webkit.ServiceWorkerWebSettingsCompat#setRequestedWithHeaderMode}
     */
    public void setRequestedWithHeaderMode(@RequestedWithHeaderMode int mode) {
        AwWebContentsMetricsRecorder.recordRequestedWithHeaderModeServiceWorkerAPIUsage(mode);
        synchronized (mAwServiceWorkerSettingsLock) {
            mRequestedWithHeaderMode = mode;
        }
    }

    /**
     * See {@link androidx.webkit.ServiceWorkerWebSettingsCompat#getRequestedWithHeaderMode}
     */
    @RequestedWithHeaderMode
    public int getRequestedWithHeaderMode() {
        synchronized (mAwServiceWorkerSettingsLock) {
            return mRequestedWithHeaderMode;
        }
    }
}
