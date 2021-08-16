// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;

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

        // Generate a new ID and save it in Shared Preferences, so it can be reused.
        String newClientId = getDelegate().generateClientId();
        CloudManagementSharedPreferences.saveClientId(newClientId);

        return newClientId;
    }

    /**
     * Returns the value of Gservices Android ID that allows joining the Chrome Browser Cloud
     * Management data with Google Endpoint Management.
     *
     * Note: that ID can only be uploaded for Android versions S and older. Changes to this requires
     * explicit approval from Chrome Privacy.
     */
    @CalledByNative
    public String getGservicesAndroidId() {
        return Build.VERSION.SDK_INT <= Build.VERSION_CODES.S
                ? getDelegate().getGservicesAndroidId()
                : "";
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
