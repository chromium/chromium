// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.memory.MemoryPressureMonitor;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentViewStatics;

import java.util.Arrays;
import java.util.List;
import java.util.Set;

/**
 * Java side of the Browser Context: contains all the java side objects needed to host one
 * browsing session (i.e. profile).
 *
 * Note that historically WebView was running in single process mode, and limitations on renderer
 * process only being able to use a single browser context, currently there can only be one
 * AwBrowserContext instance, so at this point the class mostly exists for conceptual clarity.
 */
@JNINamespace("android_webview")
@Lifetime.Profile
public class AwBrowserContext implements BrowserContextHandle {
    private static final String TAG = "AwBrowserContext";
    private static final String BASE_PREFERENCES = "WebViewProfilePrefs";

    private AwGeolocationPermissions mGeolocationPermissions;
    private AwServiceWorkerController mServiceWorkerController;
    private AwQuotaManagerBridge mQuotaManagerBridge;

    /** Pointer to the Native-side AwBrowserContext. */
    private long mNativeAwBrowserContext;

    @NonNull private final String mName;
    @NonNull private final String mRelativePath;
    @NonNull private final AwCookieManager mCookieManager;
    private final boolean mIsDefault;
    @NonNull private final SharedPreferences mSharedPreferences;

    public AwBrowserContext(long nativeAwBrowserContext) {
        this(
                nativeAwBrowserContext,
                AwBrowserContextJni.get().getDefaultContextName(),
                AwBrowserContextJni.get().getDefaultContextRelativePath(),
                AwCookieManager.getDefaultCookieManager(),
                true);
    }

    public AwBrowserContext(
            long nativeAwBrowserContext,
            @NonNull String name,
            @NonNull String relativePath,
            @NonNull AwCookieManager cookieManager,
            boolean isDefault) {
        mNativeAwBrowserContext = nativeAwBrowserContext;
        mName = name;
        mRelativePath = relativePath;
        mCookieManager = cookieManager;
        mIsDefault = isDefault;

        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            // Prefs dir will be created if it doesn't exist, so must allow writes.
            mSharedPreferences = createSharedPrefs(relativePath);

            if (isDefaultAwBrowserContext()) {
                // Migration requires disk writes.
                migrateGeolocationPreferences();
            }
        }

        // Register MemoryPressureMonitor callbacks and make sure it polls only if there is at
        // least one WebView around.
        MemoryPressureMonitor.INSTANCE.registerComponentCallbacks();
        AwContentsLifecycleNotifier.getInstance()
                .addObserver(
                        new AwContentsLifecycleNotifier.Observer() {
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

    @NonNull
    public String getName() {
        return mName;
    }

    @NonNull
    public String getRelativePathForTesting() {
        return mRelativePath;
    }

    @NonNull
    public String getSharedPrefsNameForTesting() {
        return getSharedPrefsFilename(mRelativePath);
    }

    @NonNull
    private static String getSharedPrefsFilename(@NonNull final String relativePath) {
        final String dataDirSuffix = AwBrowserProcess.getProcessDataDirSuffix();
        if (dataDirSuffix == null || dataDirSuffix.isEmpty()) {
            return BASE_PREFERENCES + relativePath;
        } else {
            return BASE_PREFERENCES + relativePath + "_" + dataDirSuffix;
        }
    }

    public AwCookieManager getCookieManager() {
        return mCookieManager;
    }

    public AwGeolocationPermissions getGeolocationPermissions() {
        if (mGeolocationPermissions == null) {
            mGeolocationPermissions = new AwGeolocationPermissions(mSharedPreferences);
        }
        return mGeolocationPermissions;
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
            mQuotaManagerBridge =
                    new AwQuotaManagerBridge(
                            AwBrowserContextJni.get()
                                    .getQuotaManagerBridge(mNativeAwBrowserContext));
        }
        return mQuotaManagerBridge;
    }

    private void migrateGeolocationPreferences() {
        // Prefs dir will be created if it doesn't exist, so must allow writes
        // for this and so that the actual prefs can be written to the new
        // location if needed.
        final String oldGlobalPrefsName = "WebViewChromiumPrefs";
        SharedPreferences oldGlobalPrefs =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(oldGlobalPrefsName, Context.MODE_PRIVATE);
        AwGeolocationPermissions.migrateGeolocationPreferences(oldGlobalPrefs, mSharedPreferences);
    }

    /** Used by {@link AwServiceWorkerSettings#setRequestedWithHeaderOriginAllowList(Set)} */
    Set<String> updateServiceWorkerXRequestedWithAllowListOriginMatcher(
            Set<String> allowedOriginRules) {
        String[] badRules =
                AwBrowserContextJni.get()
                        .updateServiceWorkerXRequestedWithAllowListOriginMatcher(
                                mNativeAwBrowserContext, allowedOriginRules.toArray(new String[0]));
        return Set.of(badRules);
    }

    /** @see android.webkit.WebView#pauseTimers() */
    public void pauseTimers() {
        ContentViewStatics.setWebKitSharedTimersSuspended(true);
    }

    /** @see android.webkit.WebView#resumeTimers() */
    public void resumeTimers() {
        ContentViewStatics.setWebKitSharedTimersSuspended(false);
    }

    @Override
    public long getNativeBrowserContextPointer() {
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

    /**
     * Check whether a context with the given name exists (in memory or on disk).
     * <p>
     * Name must be non-null and valid Unicode.
     */
    public static boolean checkNamedContextExists(String name) {
        return AwBrowserContextJni.get().checkNamedContextExists(name);
    }

    /**
     * Get the context with the given name, optionally creating it if needed.
     * <p>
     * Returns null if the context does not exist and createIfNeeded is false.
     * <p>
     * Name must be non-null and valid Unicode.
     */
    public static AwBrowserContext getNamedContext(String name, boolean createIfNeeded) {
        return AwBrowserContextJni.get().getNamedContextJava(name, createIfNeeded);
    }

    /**
     * Delete the named context.
     * <p>
     * Returns true if a context was deleted. Returns false if the context did not exist beforehand.
     * <p>
     * Name must be non-null and valid Unicode.
     *
     * @throws IllegalArgumentException if trying to delete the default profile.
     * @throws IllegalStateException if trying to delete a profile which is in use.
     */
    public static boolean deleteNamedContext(String name)
            throws IllegalArgumentException, IllegalStateException {
        final String defaultContextName = AwBrowserContextJni.get().getDefaultContextName();
        if (name.equals(defaultContextName)) {
            throw new IllegalArgumentException("Cannot delete the default profile");
        }
        return AwBrowserContextJni.get().deleteNamedContext(name);
    }

    /** List all contexts. */
    public static List<String> listAllContexts() {
        return Arrays.asList(AwBrowserContextJni.get().listAllContexts());
    }

    /**
     * Get the named context's relative path, without loading it in.
     * <p>
     * Will return null if the context doesn't exist.
     */
    public static String getNamedContextPathForTesting(String name) {
        return AwBrowserContextJni.get().getNamedContextPathForTesting(name); // IN-TEST
    }

    // See comments in WebViewChromiumFactoryProvider for details.
    public void setWebLayerRunningInSameProcess() {
        AwBrowserContextJni.get().setWebLayerRunningInSameProcess(mNativeAwBrowserContext);
    }

    public void clearPersistentOriginTrialStorageForTesting() {
        AwBrowserContextJni.get()
                .clearPersistentOriginTrialStorageForTesting(mNativeAwBrowserContext);
    }

    public boolean hasFormData() {
        return AwBrowserContextJni.get().hasFormData(mNativeAwBrowserContext);
    }

    public void clearFormData() {
        AwBrowserContextJni.get().clearFormData(mNativeAwBrowserContext);
    }

    public void setServiceWorkerIoThreadClient(AwContentsIoThreadClient ioThreadClient) {
        AwBrowserContextJni.get()
                .setServiceWorkerIoThreadClient(mNativeAwBrowserContext, ioThreadClient);
    }

    private static SharedPreferences createSharedPrefs(String relativePath) {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(getSharedPrefsFilename(relativePath), Context.MODE_PRIVATE);
    }

    @CalledByNative
    public static AwBrowserContext create(
            long nativeAwBrowserContext,
            String name,
            String relativePath,
            AwCookieManager cookieManager,
            boolean isDefault) {
        return new AwBrowserContext(
                nativeAwBrowserContext, name, relativePath, cookieManager, isDefault);
    }

    @CalledByNative
    public static void deleteSharedPreferences(String relativePath) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            final String sharedPrefsFilename = getSharedPrefsFilename(relativePath);
            SharedPreferences.Editor prefsEditor = createSharedPrefs(sharedPrefsFilename).edit();
            prefsEditor.clear().apply();
        }
    }

    @NativeMethods
    interface Natives {
        AwBrowserContext getDefaultJava();

        AwBrowserContext getNamedContextJava(String name, boolean createIfNeeded);

        String getDefaultContextName();

        String getDefaultContextRelativePath();

        String getNamedContextPathForTesting(String name); // IN-TEST

        boolean deleteNamedContext(String name);

        String[] listAllContexts();

        boolean checkNamedContextExists(String name);

        long getQuotaManagerBridge(long nativeAwBrowserContext);

        void setWebLayerRunningInSameProcess(long nativeAwBrowserContext);

        String[] updateServiceWorkerXRequestedWithAllowListOriginMatcher(
                long nativeAwBrowserContext, String[] rules);

        void clearPersistentOriginTrialStorageForTesting(long nativeAwBrowserContext);

        boolean hasFormData(long nativeAwBrowserContext);

        void clearFormData(long nativeAwBrowserContext);

        void setServiceWorkerIoThreadClient(
                long nativeAwBrowserContext, AwContentsIoThreadClient ioThreadClient);
    }
}
