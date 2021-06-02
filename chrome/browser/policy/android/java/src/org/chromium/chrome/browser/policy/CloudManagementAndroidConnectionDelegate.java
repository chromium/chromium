// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import java.util.UUID;

/**
 * Delegate for cloud management functions implemented downstream for Google Chrome.
 */
public class CloudManagementAndroidConnectionDelegate {
    /* Returns the client ID to be used in the DM token generation. By default a random UUID is
     * generated for development and testing purposes. Any device that uses randomly generated UUID
     * as client id for CBCM might be wiped out from the server without notice. */
    public String generateClientId() {
        return UUID.randomUUID().toString();
    }
}
