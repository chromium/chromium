// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.memory.MemoryPressureMonitor;
import org.chromium.content_public.browser.ContentViewStatics;

/**
 * Java side of the Browser Context: contains all the java side objects needed to host one
 * browsing session (i.e. profile).
 *
 * Note that historically WebView was running in single process mode, and limitations on renderer
 * process only being able to use a single browser context, currently there can only be one
 * AwBrowserContext instance, so at this point the class mostly exists for conceptual clarity.
 */
@JNINamespace("android_webview")
public class AwBrowserContext {
    private static final String CHROMIUM_PREFS_NAME = "WebViewProfilePrefsDefault";

    private static final String TAG = "AwBrowserContext";
    private final SharedPreferences mSharedPreferences;

    private AwGeolocationPermissions mGeolocationPermissions;
    private AwFormDatabase mFormDatabase;
    private AwServiceWorkerController mServiceWorkerController;
    private AwQuotaManagerBridge mQuotaManagerBridge;

    /** Pointer to the Native-side AwBrowserContext. */
    private long mNativeAwBrowserContext;
    private final boolean mIsDefault;

    public AwBrowserContext(
            SharedPreferences sharedPreferences, long nativeAwBrowserContext, boolean isDefault) {
        mNativeAwBrowserContext = nativeAwBrowserContext;
        mSharedPreferences = sharedPreferences;

        mIsDefault = isDefault;
        if (isDefaultAwBrowserContext()) {
            migrateGeolocationPreferences();
        }

        PlatformServiceBridge.getInstance().setSafeBrowsingHandler();

        // Register MemoryPressureMonitor callbacks and make sure it polls only if there is at
        // least one WebView around.
        MemoryPressureMonitor.INSTANCE.registerComponentCallbacks();
        AwContentsLifecycleNotifier.addObserver(new AwContentsLifecycleNotifier.Observer() {
            @Override
            public void onFirstWebViewCreated() {
                MemoryPressureMonitor.INSTANCE.enablePolling();
            }
            @Override
            public void onLastWebViewDestroyed() {
                MemoryPressureMonitor.INSTANCE.disablePolling();
            }
        });
    }

    @VisibleForTesting
    public void setNativePointer(long nativeAwBrowserContext) {
        mNativeAwBrowserContext = nativeAwBrowserContext;
    }

    public AwGeolocationPermissions getGeolocationPermissions() {
        if (mGeolocationPermissions == null) {
            mGeolocationPermissions = new AwGeolocationPermissions(mSharedPreferences);
        }
        return mGeolocationPermissions;
    }

    public AwFormDatabase getFormDatabase() {
        if (mFormDatabase == null) {
            mFormDatabase = new AwFormDatabase();
        }
        return mFormDatabase;
    }

    public AwServiceWorkerController getServiceWorkerController() {
        if (mServiceWorkerController == null) {
            mServiceWorkerController =
                    new AwServiceWorkerController(ContextUtils.getApplicationContext(), this);
        }
        return mServiceWorkerController;
    }

    public AwQuotaManagerBridge getQuotaManagerBridge() {
        if (mQuotaManagerBridge == null) {
            mQuotaManagerBridge = new AwQuotaManagerBridge(
                    AwBrowserContextJni.get().getQuotaManagerBridge(mNativeAwBrowserContext));
        }
        return mQuotaManagerBridge;
    }

    private void migrateGeolocationPreferences() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            // Prefs dir will be created if it doesn't exist, so must allow writes
            // for this and so that the actual prefs can be written to the new
            // location if needed.
            final String oldGlobalPrefsName = "WebViewChromiumPrefs";
            SharedPreferences oldGlobalPrefs =
                    ContextUtils.getApplicationContext().getSharedPreferences(
                            oldGlobalPrefsName, Context.MODE_PRIVATE);
            AwGeolocationPermissions.migrateGeolocationPreferences(
                    oldGlobalPrefs, mSharedPreferences);
        }
    }

    /**
     * @see android.webkit.WebView#pauseTimers()
     */
    public void pauseTimers() {
        ContentViewStatics.setWebKitSharedTimersSuspended(true);
    }

    /**
     * @see android.webkit.WebView#resumeTimers()
     */
    public void resumeTimers() {
        ContentViewStatics.setWebKitSharedTimersSuspended(false);
    }

    public long getNativePointer() {
        return mNativeAwBrowserContext;
    }

    public boolean isDefaultAwBrowserContext() {
        return mIsDefault;
    }

    private static AwBrowserContext sInstance;
    public static AwBrowserContext getDefault() {
        if (sInstance == null) {
            sInstance = AwBrowserContextJni.get().getDefaultJava();
        }
        return sInstance;
    }

    // See comments in WebViewChromiumFactoryProvider for details.
    public void setWebLayerRunningInSameProcess() {
        AwBrowserContextJni.get().setWebLayerRunningInSameProcess(mNativeAwBrowserContext);
    }

    @CalledByNative
    public static AwBrowserContext create(long nativeAwBrowserContext, boolean isDefault) {
        SharedPreferences sharedPreferences;
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            // Prefs dir will be created if it doesn't exist, so must allow writes.
            sharedPreferences = ContextUtils.getApplicationContext().getSharedPreferences(
                    CHROMIUM_PREFS_NAME, Context.MODE_PRIVATE);
        }

        return new AwBrowserContext(sharedPreferences, nativeAwBrowserContext, isDefault);
    }

    @NativeMethods
    interface Natives {
        AwBrowserContext getDefaultJava();
        long getQuotaManagerBridge(long nativeAwBrowserContext);
        void setWebLayerRunningInSameProcess(long nativeAwBrowserContext);
    }
}
