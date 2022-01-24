// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

/**
 * Delegate for cloud management functions implemented downstream for Google Chrome.
 */
public interface CloudManagementAndroidConnectionDelegate {
    /**
     * (DEPRECATED) Returns the client ID to be used in the DM token generation.
     *
     * TODO(http://crbug.com/1264463): delete this method once it's not
     *                                 overridden downstream.
     */
    default String generateClientId() {
        return "";
    }

    /** Returns the value of Gservices Android ID. */
    String getGservicesAndroidId();
}
