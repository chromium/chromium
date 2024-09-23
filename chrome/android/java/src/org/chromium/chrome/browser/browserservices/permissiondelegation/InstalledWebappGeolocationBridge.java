// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.browser.trusted.TrustedWebActivityCallback;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.url.GURL;

/**
 * Provides Trusted Web Activity Client App location for native. The C++ counterpart is the
 * {@code installed_webapp_geolocation_bridge.h}. The InstalledWebappGeolocationBridge is
 * responsible for converting start and stop location command from the native to the corresponding
 * TWA client app via {@link TrustedWebActivityClient}, and notify the native part when there is
 * a location result.
 * Lifecycle: The native part is responsible for controlling its lifecycle. A new instance will be
 * created for each new geolocation request. This class should not be used after "stopAndDestroy" is
 * called.
 */
public class InstalledWebappGeolocationBridge {
    static final String EXTRA_NEW_LOCATION_AVAILABLE_CALLBACK = "onNewLocationAvailable";
    public static final String EXTRA_NEW_LOCATION_ERROR_CALLBACK = "onNewLocationError";

    private long mNativePointer;
    private final GURL mUrl;

    private final TrustedWebActivityClient mTwaClient;

    private final TrustedWebActivityCallback mLocationUpdateCallback =
            new TrustedWebActivityCallback() {
                @Override
                public void onExtraCallback(String callbackName, @Nullable Bundle bundle) {
                    // Hop back over to the UI thread to deal with the result.
                    PostTask.postTask(
                            TaskTraits.UI_USER_VISIBLE,
                            () -> {
                                if (TextUtils.equals(
                                        callbackName, EXTRA_NEW_LOCATION_AVAILABLE_CALLBACK)) {
                                    notifyNewGeoposition(bundle);
                                } else if (TextUtils.equals(
                                        callbackName, EXTRA_NEW_LOCATION_ERROR_CALLBACK)) {
                                    String message =
                                            bundle != null ? bundle.getString("message", "") : "";
                                    notifyNewGeopositionError(message);
                                }
                            });
                }
            };

    InstalledWebappGeolocationBridge(long nativePtr, GURL url, TrustedWebActivityClient client) {
        mNativePointer = nativePtr;
        mUrl = url;
        mTwaClient = client;
    }

    @CalledByNative
    public static @Nullable InstalledWebappGeolocationBridge create(long nativePtr, GURL url) {
        if (url == null) return null;

        return new InstalledWebappGeolocationBridge(
                nativePtr,
                url,
                ChromeApplicationImpl.getComponent().resolveTrustedWebActivityClient());
    }

    @CalledByNative
    public void start(boolean highAccuracy) {
        mTwaClient.startListeningLocationUpdates(
                mUrl.getSpec(), highAccuracy, mLocationUpdateCallback);
    }

    @CalledByNative
    public void stopAndDestroy() {
        mNativePointer = 0;
        mTwaClient.stopLocationUpdates(mUrl.getSpec());
    }

    private void notifyNewGeoposition(@Nullable Bundle bundle) {
        if (bundle == null || mNativePointer == 0) return;

        double latitude = bundle.getDouble("latitude");
        double longitude = bundle.getDouble("longitude");
        // Android Location.getTime() is in milliseconds, convert to seconds before passing the
        // value to native.
        double timeStamp = bundle.getLong("timeStamp") / 1000.0;
        boolean hasAltitude = bundle.containsKey("altitude");
        double altitude = bundle.getDouble("altitude");
        boolean hasAccuracy = bundle.containsKey("accuracy");
        double accuracy = bundle.getDouble("accuracy");
        boolean hasBearing = bundle.containsKey("bearing");
        double bearing = bundle.getDouble("bearing");
        boolean hasSpeed = bundle.containsKey("speed");
        double speed = bundle.getDouble("speed");
        InstalledWebappGeolocationBridgeJni.get()
                .onNewLocationAvailable(
                        mNativePointer,
                        latitude,
                        longitude,
                        timeStamp,
                        hasAltitude,
                        altitude,
                        hasAccuracy,
                        accuracy,
                        hasBearing,
                        bearing,
                        hasSpeed,
                        speed);
    }

    private void notifyNewGeopositionError(String message) {
        if (mNativePointer == 0) return;
        InstalledWebappGeolocationBridgeJni.get().onNewErrorAvailable(mNativePointer, message);
    }

    @NativeMethods
    interface Natives {
        void onNewLocationAvailable(
                long nativeInstalledWebappGeolocationBridge,
                double latitude,
                double longitude,
                double timeStamp,
                boolean hasAltitude,
                double altitude,
                boolean hasAccuracy,
                double accuracy,
                boolean hasHeading,
                double heading,
                boolean hasSpeed,
                double speed);

        void onNewErrorAvailable(
                long nativeInstalledWebappGeolocationBridge,
                @JniType("std::string") String message);
    }
}
