// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.LruCache;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.MediaIntegrityApiStatus;
import org.chromium.android_webview.common.MediaIntegrityProvider;
import org.chromium.base.BaseFeatures;
import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.memory.MemoryPressureMonitor;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.blink.mojom.PermissionStatus;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentViewStatics;
import org.chromium.url.Origin;

import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.Set;
import java.util.regex.Pattern;

/**
 * Java side of the Browser Context: contains all the java side objects needed to host one browsing
 * session (i.e. profile).
 *
 * <p>Note that historically WebView was running in single process mode, and limitations on renderer
 * process only being able to use a single browser context, currently there can only be one
 * AwBrowserContext instance, so at this point the class mostly exists for conceptual clarity.
 */
@JNINamespace("android_webview")
@Lifetime.Profile
public class AwBrowserContext implements BrowserContextHandle {
    private static final String BASE_PREFERENCES = "WebViewProfilePrefs";

    /* package */ static final Pattern BAD_HEADER_CHAR = Pattern.compile("[\u0000\r\n]");
    /* package */ static final String BAD_HEADER_MSG =
            "HTTP headers must not contain null, CR, or NL characters. ";

    /**
     * Cache storing already-initialized Play providers for the Media Integrity Blink renderer
     * extension. This cache speeds up calls for new providers from a similar context.
     *
     * <p>Cached entries will remain cached across page loads. The cache is Profile-specific to
     * avoid sharing values between profiles.
     *
     * <p>The cache size is estimated from the {@code Android.WebView.OriginsVisited} histogram,
     * looking at the 1-day aggregation of a representative app where users browse multiple domains.
     * The P95 for the metric over 1 day is 15.
     *
     * @see MediaIntegrityProviderKey
     */
    private final LruCache<MediaIntegrityProviderKey, MediaIntegrityProvider>
            mMediaIntegrityProviderCache =
                    new LruCache<>(10) {

                        private int mEvictionCounter;

                        @Override
                        protected void entryRemoved(
                                boolean evicted,
                                MediaIntegrityProviderKey key,
                                MediaIntegrityProvider oldValue,
                                MediaIntegrityProvider newValue) {
                            // Log evictions due to lack of space.
                            if (evicted) {
                                RecordHistogram.recordCount100Histogram(
                                        "Android.WebView.MediaIntegrity"
                                                + ".TokenProviderCacheEvictionsCumulativeV2",
                                        ++mEvictionCounter);
                            }
                        }
                    };

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

    private final AwPrefetchManager mPrefetchManager;

    /**
     * Cache key for MediaIntegrityProviders. Ensures that values are keyed by
     *
     * <ul>
     *   <li>top frame origin
     *   <li>source frame origin
     *   <li>Api status
     *   <li>cloud project number
     * </ul>
     */
    public static final class MediaIntegrityProviderKey {

        private final Origin mTopFrameOrigin;
        private final Origin mSourceOrigin;
        @MediaIntegrityApiStatus private final int mRequestMode;
        private final long mCloudProjectNumber;

        public MediaIntegrityProviderKey(
                Origin topFrameOrigin,
                Origin sourceOrigin,
                @MediaIntegrityApiStatus int requestMode,
                long cloudProjectNumber) {
            mTopFrameOrigin = topFrameOrigin;
            mSourceOrigin = sourceOrigin;
            mRequestMode = requestMode;
            mCloudProjectNumber = cloudProjectNumber;
        }

        @Override
        public int hashCode() {
            return Objects.hash(mTopFrameOrigin, mSourceOrigin, mRequestMode, mCloudProjectNumber);
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (!(obj instanceof MediaIntegrityProviderKey other)) {
                return false;
            }
            return Objects.equals(this.mTopFrameOrigin, other.mTopFrameOrigin)
                    && Objects.equals(this.mSourceOrigin, other.mSourceOrigin)
                    && this.mRequestMode == other.mRequestMode
                    && this.mCloudProjectNumber == other.mCloudProjectNumber;
        }
    }

    public AwBrowserContext(long nativeAwBrowserContext) {
        this(
                nativeAwBrowserContext,
                AwBrowserContextJni.get().getDefaultContextName(),
                AwBrowserContextJni.get().getDefaultContextRelativePath(),
                AwCookieManager.getDefaultCookieManager(),
                new AwPrefetchManager(0),
                true);
    }

    public AwBrowserContext(
            long nativeAwBrowserContext,
            @NonNull String name,
            @NonNull String relativePath,
            @NonNull AwCookieManager cookieManager,
            @NonNull AwPrefetchManager prefetchManager,
            boolean isDefault) {
        mNativeAwBrowserContext = nativeAwBrowserContext;
        mName = name;
        mRelativePath = relativePath;
        mCookieManager = cookieManager;
        mPrefetchManager = prefetchManager;
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
                                MemoryPressureMonitor.INSTANCE.enablePolling(
                                        AwFeatureMap.isEnabled(
                                                BaseFeatures
                                                        .POST_GET_MY_MEMORY_STATE_TO_BACKGROUND));
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

    @Nullable
    public MediaIntegrityProvider getCachedMediaIntegrityProvider(
            @NonNull MediaIntegrityProviderKey key) {
        return mMediaIntegrityProviderCache.get(key);
    }

    public void putMediaIntegrityProviderInCache(
            @NonNull MediaIntegrityProviderKey key, @NonNull MediaIntegrityProvider provider) {
        mMediaIntegrityProviderCache.put(key, provider);
    }

    /**
     * Remove an invalid AWMI token provider from the provider cache.
     *
     * @param key The key of the cache entry to invalidate.
     * @param provider The value of the cache entry to invalidate. The cache entry will only be
     *     removed if the current provider for the given key matches this provider.
     */
    public void invalidateCachedMediaIntegrityProvider(
            @NonNull MediaIntegrityProviderKey key, @NonNull MediaIntegrityProvider provider) {
        final MediaIntegrityProvider current = mMediaIntegrityProviderCache.get(key);
        if (current == provider) {
            mMediaIntegrityProviderCache.remove(key);
        }
    }

    /**
     * Check if additional HTTP headers sent along with loadUrl, prefetchUrl, or prerenderUrl
     * contains invalid characters.
     *
     * @param headers The additional HTTP headers to be sent along with loadUrl, prefetchUrl, or
     *     prerenderUrl.
     * @return An exception if validation fails. Otherwise, an empty Optional.
     */
    public static Optional<IllegalArgumentException> validateAdditionalHeaders(
            Map<String, String> headers) {
        if (headers == null) return Optional.empty();
        for (Map.Entry<String, String> header : headers.entrySet()) {
            String headerName = header.getKey();
            String headerValue = header.getValue();
            if (headerName != null && BAD_HEADER_CHAR.matcher(headerName).find()) {
                return Optional.of(
                        new IllegalArgumentException(
                                BAD_HEADER_MSG + "Invalid header name '" + headerName + "'."));
            }
            if (headerValue != null && BAD_HEADER_CHAR.matcher(headerValue).find()) {
                return Optional.of(
                        new IllegalArgumentException(
                                BAD_HEADER_MSG
                                        + "Header '"
                                        + headerName
                                        + "' has invalid value '"
                                        + headerValue
                                        + "'"));
            }
        }
        return Optional.empty();
    }

    @NonNull
    public AwPrefetchManager getPrefetchManager() {
        return mPrefetchManager;
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

    public void clearPersistentOriginTrialStorageForTesting() {
        AwBrowserContextJni.get()
                .clearPersistentOriginTrialStorageForTesting(mNativeAwBrowserContext);
    }

    public void setServiceWorkerIoThreadClient(AwContentsIoThreadClient ioThreadClient) {
        AwBrowserContextJni.get()
                .setServiceWorkerIoThreadClient(mNativeAwBrowserContext, ioThreadClient);
    }

    @UiThread
    public void setMaxPrerenders(int maxPrerenders) {
        AwBrowserContextJni.get()
                .setAllowedPrerenderingCount(mNativeAwBrowserContext, maxPrerenders);
    }

    @UiThread
    public void warmUpSpareRenderer() {
        AwBrowserContextJni.get().warmUpSpareRenderer(mNativeAwBrowserContext);
    }

    private static SharedPreferences createSharedPrefs(String relativePath) {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(getSharedPrefsFilename(relativePath), Context.MODE_PRIVATE);
    }

    @CalledByNative
    public static AwBrowserContext create(
            long nativeAwBrowserContext,
            @JniType("std::string") String name,
            @JniType("std::string") String relativePath,
            AwCookieManager cookieManager,
            AwPrefetchManager prefetchManager,
            boolean isDefault) {
        return new AwBrowserContext(
                nativeAwBrowserContext,
                name,
                relativePath,
                cookieManager,
                prefetchManager,
                isDefault);
    }

    @CalledByNative
    public static void deleteSharedPreferences(@JniType("std::string") String relativePath) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            final String sharedPrefsFilename = getSharedPrefsFilename(relativePath);
            SharedPreferences.Editor prefsEditor = createSharedPrefs(sharedPrefsFilename).edit();
            prefsEditor.clear().apply();
        }
    }

    @CalledByNative
    private int getGeolocationPermission(@JniType("std::string") String origin) {
        AwGeolocationPermissions permissions = getGeolocationPermissions();
        if (!permissions.hasOrigin(origin)) {
            return PermissionStatus.ASK;
        }
        return permissions.isOriginAllowed(origin)
                ? PermissionStatus.GRANTED
                : PermissionStatus.DENIED;
    }

    @NativeMethods
    interface Natives {
        AwBrowserContext getDefaultJava();

        @JniType("std::string")
        String getDefaultContextName();

        @JniType("std::string")
        String getDefaultContextRelativePath();

        long getQuotaManagerBridge(long nativeAwBrowserContext);

        String[] updateServiceWorkerXRequestedWithAllowListOriginMatcher(
                long nativeAwBrowserContext, String[] rules);

        void clearPersistentOriginTrialStorageForTesting(long nativeAwBrowserContext);

        void setServiceWorkerIoThreadClient(
                long nativeAwBrowserContext, AwContentsIoThreadClient ioThreadClient);

        void setAllowedPrerenderingCount(long nativeAwBrowserContext, int maxPrerenders);

        void warmUpSpareRenderer(long nativeAwBrowserContext);
    }
}
