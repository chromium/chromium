// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.ResettersForTesting;

/** Provider class for {@link TabManagementDelegate}. */
public class TabManagementDelegateProvider {
    private static TabManagementDelegate sTabManagementDelegate;

    /** Returns {@link TabManagementDelegate} implementation. */
    public static TabManagementDelegate getDelegate() {
        if (sTabManagementDelegate == null) {
            sTabManagementDelegate = new TabManagementDelegateImpl();
        }
        return sTabManagementDelegate;
    }

    static void setTabManagementDelegateForTesting(TabManagementDelegate tabManagmentDelegate) {
        sTabManagementDelegate = tabManagmentDelegate;
        ResettersForTesting.register(() -> sTabManagementDelegate = null);
    }
}
