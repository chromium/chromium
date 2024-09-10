// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import android.content.Context;
import android.os.Handler;
import android.os.HandlerThread;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;

/**
 * This class manages platform-specific services. (i.e. Google Services) The platform should extend
 * this class and use this base class to fetch their specialized version.
 */
public abstract class PlatformServiceBridge {
    private static PlatformServiceBridge sInstance;
    private static final Object sInstanceLock = new Object();

    private static HandlerThread sHandlerThread;
    private static Handler sHandler;
    private static final Object sHandlerLock = new Object();

    protected PlatformServiceBridge() {}

    @SuppressWarnings("unused")
    public static PlatformServiceBridge getInstance() {
        synchronized (sInstanceLock) {
            if (sInstance == null) {
                // Load an instance of PlatformServiceBridgeImpl. Because this can change
                // depending on the GN configuration, this may not be the PlatformServiceBridgeImpl
                // defined upstream.
                sInstance = new PlatformServiceBridgeImpl();
            }
            return sInstance;
        }
    }

    // Provide a mocked PlatformServiceBridge for testing.
    public static void injectInstance(PlatformServiceBridge testBridge) {
        // Although reference assignments are atomic, we still wouldn't want to assign it in the
        // middle of getInstance().
        synchronized (sInstanceLock) {
            sInstance = testBridge;
        }
    }

    // Return a handler appropriate for executing blocking Platform Service tasks.
    public static Handler getHandler() {
        synchronized (sHandlerLock) {
            if (sHandler == null) {
                sHandlerThread = new HandlerThread("PlatformServiceBridgeHandlerThread");
                sHandlerThread.start();
                sHandler = new Handler(sHandlerThread.getLooper());
            }
        }
        return sHandler;
    }

    // Can WebView use Google Play Services (a.k.a. GMS)?
    public boolean canUseGms() {
        return false;
    }

    // Returns the versionCode of GMS that the user is currently running.
    // Will always return 0 if GMS is not installed.
    public int getGmsVersionCode() {
        return 0;
    }

    // Overriding implementations may call "callback" asynchronously, on any thread.
    public void querySafeBrowsingUserConsent(@NonNull final Callback<Boolean> callback) {
        // User opt-in preference depends on a SafetyNet API. In purely upstream builds (which don't
        // communicate with GMS), assume the user has not opted in.
        callback.onResult(false);
    }

    // Overriding implementations should not call "callback" synchronously, even if the result is
    // already known. The callback should be posted to the UI thread to run at the next opportunity,
    // to avoid blocking the critical path for startup.
    public void queryMetricsSetting(Callback<Boolean> callback) {
        ThreadUtils.assertOnUiThread();
        ThreadUtils.postOnUiThread(
                () -> {
                    callback.onResult(false);
                });
    }

    public void setSafeBrowsingHandler() {
        // We don't have this specialized service.
    }

    public void warmUpSafeBrowsing(Context context, @NonNull final Callback<Boolean> callback) {
        callback.onResult(false);
    }

    // Takes an uncompressed, serialized UMA proto and logs it via a platform-specific mechanism.
    public void logMetrics(byte[] data) {}

    /**
     * Similar to {@link logMetrics}, logs a serialized UMA proto via a platform-specific mechanism
     * but blocks until the operation finishes.
     *
     * @param data uncompressed, serialized UMA proto.
     * @return Status code of the logging operation. The status codes are:
     * - Success cache (went to the devices cache): -1
     * - Success: 0
     * - Internal error: 8
     * - Interrupted: 14
     * - Timeout: 15
     * - Cancelled: 16
     * - API not connected (probably means the API is not available on device): 17
     */
    public int logMetricsBlocking(byte[] data) {
        // TODO(crbug.com/40790308): remove this once downstream implementation lands.
        logMetrics(data);
        return 0;
    }

    /**
     * Checks if app recovery mitigations are currently required and initializes SafeMode if needed.
     * This should only be called from the ":webview_service" process. All other processes should
     * query SafeModeController to receive mitigation steps.
     */
    public void checkForAppRecovery() {}

    public @Nullable AwSupervisedUserUrlClassifierDelegate getUrlClassifierDelegate() {
        return null;
    }

    /**
     * Asynchronously obtain a MediaIntegrityProvider implementation.
     *
     * @param cloudProjectNumber cloud project number passed by caller
     * @param apiStatus Enablement status of the api for given origin
     * @param callback Callback to call with the result containing either a non-null
     *     MediaIntegrityProvider implementation or an appropriate exception.
     */
    public void getMediaIntegrityProvider2(
            long cloudProjectNumber,
            @MediaIntegrityApiStatus int apiStatus,
            ValueOrErrorCallback<MediaIntegrityProvider, MediaIntegrityErrorWrapper> callback) {
        callback.onError(
                new MediaIntegrityErrorWrapper(MediaIntegrityErrorCode.NON_RECOVERABLE_ERROR));
    }
}
