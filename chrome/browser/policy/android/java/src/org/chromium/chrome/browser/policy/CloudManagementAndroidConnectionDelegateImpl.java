// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import java.util.UUID;

/**
 * Instantiable version of {@link CloudManagementAndroidConnectionDelegate}.
 * Downstream targets may provide a different implementation. In GN, we specify that
 * {@link CloudManagementAndroidConnectionDelegate} is compiled separately from its implementation;
 * other projects may specify a different CloudManagementAndroidConnectionDelegate via GN. Please
 * check http://go/apphooks-migration for more details.
 */
public class CloudManagementAndroidConnectionDelegateImpl
        implements CloudManagementAndroidConnectionDelegate {
    /* Returns the client ID to be used in the DM token generation. By default a random UUID is
     * generated for development and testing purposes. Any device that uses randomly generated UUID
     * as client id for CBCM might be wiped out from the server without notice. */
    @Override
    public String generateClientId() {
        return UUID.randomUUID().toString();
    }

    @Override
    public String getGservicesAndroidId() {
        return "";
    }
}
