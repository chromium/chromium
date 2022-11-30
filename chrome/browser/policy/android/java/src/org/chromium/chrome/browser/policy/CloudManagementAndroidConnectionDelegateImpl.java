// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

/**
 * Instantiable version of {@link CloudManagementAndroidConnectionDelegate}.
 * Downstream targets may provide a different implementation. In GN, we specify that
 * {@link CloudManagementAndroidConnectionDelegate} is compiled separately from its implementation;
 * other projects may specify a different CloudManagementAndroidConnectionDelegate via GN. Please
 * check http://go/apphooks-migration for more details.
 */
public class CloudManagementAndroidConnectionDelegateImpl
        implements CloudManagementAndroidConnectionDelegate {
    @Override
    public String getGservicesAndroidId() {
        return "";
    }
}
