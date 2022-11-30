// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;

import java.util.UUID;

/**
 * Allows access to cloud management functionalities implemented downstream.
 */
public class CloudManagementAndroidConnection {
    private static class LazyHolder {
        private static final CloudManagementAndroidConnection INSTANCE =
                new CloudManagementAndroidConnection();
    }

    /** Returns an instance of this class. */
    @CalledByNative
    public static CloudManagementAndroidConnection getInstance() {
        return LazyHolder.INSTANCE;
    }

    /** Provides access to downstream implementation. */
    private final CloudManagementAndroidConnectionDelegate mDelegate;

    private CloudManagementAndroidConnection() {
        mDelegate = new CloudManagementAndroidConnectionDelegateImpl();
    }

    /* Returns the client ID to be used in the DM token generation. Once generated, the ID is saved
     * to Shared Preferences so it can be reused. */
    @CalledByNative
    public String getClientId() {
        // Return the ID saved in Shared Preferences, if available.
        String clientId = CloudManagementSharedPreferences.readClientId();
        if (!clientId.isEmpty()) {
            return clientId;
        }

        // Use the Gservices Android ID as the client ID. If the Android ID can't be obtained, then
        // a randomly generated ID is used instead (e.g. for non-Chrome official builds).
        // Save the ID in Shared Preferences, so it can be reused.
        String newClientId = getDelegate().getGservicesAndroidId();
        if (newClientId == null || newClientId.isEmpty()) {
            newClientId = UUID.randomUUID().toString();
        }
        CloudManagementSharedPreferences.saveClientId(newClientId);

        return newClientId;
    }

    /** Overrides {@link mDelegate} if not null. */
    private static CloudManagementAndroidConnectionDelegate sDelegateForTesting;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public static void setDelegateForTesting(CloudManagementAndroidConnectionDelegate delegate) {
        sDelegateForTesting = delegate;
    }

    private CloudManagementAndroidConnectionDelegate getDelegate() {
        return sDelegateForTesting != null ? sDelegateForTesting : mDelegate;
    }
}
