// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import org.jni_zero.CalledByNativeForTesting;

import org.chromium.components.safe_browsing.OsAdditionalSecurityProvider;
import org.chromium.components.safe_browsing.OsAdditionalSecurityUtil;

/** Enables setting a mock {@link OsAdditionalSecurityProvider} from native. */
public class AdvancedProtectionStatusManagerTestUtil {
    private static class TestPermissionProvider extends OsAdditionalSecurityProvider {
        private final boolean mIsAdvancedProtectionRequestedByOs;

        public TestPermissionProvider(boolean isAdvancedProtectionRequestedByOs) {
            mIsAdvancedProtectionRequestedByOs = isAdvancedProtectionRequestedByOs;
        }

        @Override
        public boolean isAdvancedProtectionRequestedByOs() {
            return mIsAdvancedProtectionRequestedByOs;
        }
    }

    @CalledByNativeForTesting
    public static void setOsAdvancedProtectionStateForTesting(
            boolean isAdvancedProtectionRequestedByOs) {
        var provider = new TestPermissionProvider(isAdvancedProtectionRequestedByOs);
        OsAdditionalSecurityUtil.setInstanceForTesting(provider);
    }
}
