// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

/**
 * Provider class for {@link TabManagementDelegate}.
 */
public class TabManagementDelegateProvider {
    private static TabManagementDelegateImpl sTabManagementDelegateImpl;

    /**
     * Returns {@link TabManagementDelegate} implementation.
     */
    public static TabManagementDelegate getDelegate() {
        if (sTabManagementDelegateImpl == null) {
            sTabManagementDelegateImpl = new TabManagementDelegateImpl();
        }
        return sTabManagementDelegateImpl;
    }
}
