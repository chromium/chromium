// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Provider class for {@link TabManagementDelegate}. */
@NullMarked
public class TabManagementDelegateProvider {
    private static @Nullable TabManagementDelegate sTabManagementDelegate;

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
