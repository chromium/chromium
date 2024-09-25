// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import android.text.TextUtils;

import org.jni_zero.CalledByNative;

import org.chromium.base.ServiceLoaderUtil;

import java.util.UUID;

/** Allows access to cloud management functionalities implemented downstream. */
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

    private CloudManagementAndroidConnection() {}

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
        String newClientId = null;
        CloudManagementAndroidConnectionDelegate delegate =
                ServiceLoaderUtil.maybeCreate(CloudManagementAndroidConnectionDelegate.class);
        if (delegate != null) {
            newClientId = delegate.getGservicesAndroidId();
        }
        if (TextUtils.isEmpty(newClientId)) {
            newClientId = UUID.randomUUID().toString();
        }
        CloudManagementSharedPreferences.saveClientId(newClientId);

        return newClientId;
    }

    public static void setDelegateForTesting(CloudManagementAndroidConnectionDelegate delegate) {
        ServiceLoaderUtil.setInstanceForTesting(
                CloudManagementAndroidConnectionDelegate.class, delegate);
    }
}
