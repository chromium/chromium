// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import org.jni_zero.CalledByNativeForTesting;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionProvider;

/** Enables setting a mock {@link OsAdditionalSecurityPermissionProvider} from native. */
public class AdvancedProtectionStatusManagerTestUtil {
    private static class TestPermissionProvider extends OsAdditionalSecurityPermissionProvider {
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ServiceLoaderUtil.setInstanceForTesting(
                            OsAdditionalSecurityPermissionProvider.class,
                            new TestPermissionProvider(isAdvancedProtectionRequestedByOs));
                });
    }
}
