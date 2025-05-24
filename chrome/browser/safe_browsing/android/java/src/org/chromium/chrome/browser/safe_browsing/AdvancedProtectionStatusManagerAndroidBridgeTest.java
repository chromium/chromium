// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionProvider;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionUtil;
import org.chromium.components.permissions.PermissionsAndroidFeatureList;

/** Tests for {@link AdvancedProtectionStatusManagerAndroidBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures(PermissionsAndroidFeatureList.OS_ADDITIONAL_SECURITY_PERMISSION_KILL_SWITCH)
@Config(manifest = Config.NONE)
public class AdvancedProtectionStatusManagerAndroidBridgeTest {
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

    @Before
    public void setUp() {
        OsAdditionalSecurityPermissionUtil.resetForTesting();
    }

    private void setPermissionProvider(OsAdditionalSecurityPermissionProvider provider) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ServiceLoaderUtil.setInstanceForTesting(
                            OsAdditionalSecurityPermissionProvider.class, provider);
                });
    }

    @Test
    @EnableFeatures({PermissionsAndroidFeatureList.OS_ADDITIONAL_SECURITY_PERMISSION_KILL_SWITCH})
    public void testKillSwitch() {
        setPermissionProvider(
                new TestPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ true));
        assertFalse(AdvancedProtectionStatusManagerAndroidBridge.isUnderAdvancedProtection());
    }

    @Test
    public void testNoServiceProvider() {
        setPermissionProvider(null);
        assertFalse(AdvancedProtectionStatusManagerAndroidBridge.isUnderAdvancedProtection());
    }

    @Test
    public void testServiceProviderDoesNotRequestAdvancedProtection() {
        setPermissionProvider(
                new TestPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ false));
        assertFalse(AdvancedProtectionStatusManagerAndroidBridge.isUnderAdvancedProtection());
    }

    @Test
    public void testServiceProviderRequestsAdvancedProtection() {
        setPermissionProvider(
                new TestPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ true));
        assertTrue(AdvancedProtectionStatusManagerAndroidBridge.isUnderAdvancedProtection());
    }
}
