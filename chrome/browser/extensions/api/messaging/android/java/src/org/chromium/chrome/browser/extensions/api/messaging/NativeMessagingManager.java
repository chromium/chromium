// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.extensions.api.messaging;

import android.content.pm.PackageManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/**
 * Profile-scoped manager for Android Native Messaging. Maintains a map of package names to
 * ServiceConnections.
 */
@JNINamespace("extensions")
@NullMarked
public class NativeMessagingManager implements Destroyable, NativeMessagingConnection.Observer {
    private static @Nullable ProfileKeyedMap<NativeMessagingManager> sProfileMap;

    private long mNativePtr;
    private final Map<String, NativeMessagingConnection> mConnections = new HashMap<>();

    /** Return the {@link NativeMessagingManager} associated with the passed in {@link Profile}. */
    public static NativeMessagingManager getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sProfileMap == null) {
            sProfileMap =
                    ProfileKeyedMap.createMapOfDestroyables(
                            ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
        }
        return sProfileMap.getForProfile(profile, NativeMessagingManager::new);
    }

    private NativeMessagingManager(Profile profile) {
        mNativePtr = NativeMessagingManagerJni.get().initialize(this, profile);
    }

    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();
        if (mNativePtr != 0) {
            NativeMessagingManagerJni.get().destroy(mNativePtr);
            mNativePtr = 0;
        }
        // Snapshot to a list so mConnections.remove() inside onUnbound doesn't break iteration:
        for (NativeMessagingConnection connection : new ArrayList<>(mConnections.values())) {
            connection.unbind();
        }
        mConnections.clear();
    }

    @CalledByNative
    void onExtensionUnloaded(@JniType("std::string") String extensionId) {
        ThreadUtils.assertOnUiThread();
        for (NativeMessagingConnection connection : new ArrayList<>(mConnections.values())) {
            connection.onExtensionUnloaded(extensionId);
        }
    }

    // NativeMessagingConnection.Observer
    @Override
    public void onUnbound(String packageName) {
        mConnections.remove(packageName);
    }

    // Adds the provided `port` from the extension with `extensionId` to an
    // external Android app.
    // Should only be called by the `port` which adds itself here.
    @Nullable String addPort(
            String packageName,
            String extensionId,
            boolean isVerifiedExtension,
            byte[][] certificates,
            NativeMessageAndroidPort port) {
        ThreadUtils.assertOnUiThread();

        // If `certificates` is empty, then signing certificate checks for the external app will be
        // skipped. If this method is called with an empty `certificates` array, assume callers of
        // this method have authorized it.
        if (certificates.length > 0
                && !hasAnyMatchingSigningCertificate(packageName, certificates)) {
            return NativeMessagingConnection.getUnableToConnectError(packageName);
        }

        NativeMessagingConnection connection = mConnections.get(packageName);
        // Initiate a connection to the app if the app is not connected.
        if (connection == null) {
            connection = new NativeMessagingConnection(packageName, this);
            if (!connection.isBound()) {
                return NativeMessagingConnection.getUnableToConnectError(packageName);
            }

            mConnections.put(packageName, connection);
        }

        return connection.addPort(extensionId, isVerifiedExtension, port);
    }

    private static boolean hasAnyMatchingSigningCertificate(
            String packageName, byte[][] certificates) {
        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        for (byte[] certBytes : certificates) {
            if (pm.hasSigningCertificate(
                    packageName, certBytes, PackageManager.CERT_INPUT_SHA256)) {
                return true;
            }
        }
        return false;
    }

    @Nullable NativeMessagingConnection getConnectionForTesting(String packageName) {
        return mConnections.get(packageName);
    }

    @NativeMethods
    interface Natives {
        long initialize(NativeMessagingManager javaObject, @JniType("Profile*") Profile profile);

        void destroy(long nativeNativeMessagingManager);
    }
}
