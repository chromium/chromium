// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.Manifest;
import android.os.Build;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.safe_browsing.AwAdvancedProtectionStatusManagerBridge;
import org.chromium.android_webview.safe_browsing.AwAdvancedProtectionStatusManagerBridgeJni;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.safe_browsing.OsAdditionalSecurityProvider;
import org.chromium.components.safe_browsing.OsAdditionalSecurityUtil;

/** Tests for {@link AwAdvancedProtectionStatusManagerBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AwAdvancedProtectionStatusManagerBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock AwAdvancedProtectionStatusManagerBridge.Natives mNativeMock;

    private static class TestPermissionProvider extends OsAdditionalSecurityProvider {
        private final boolean mIsAdvancedProtectionRequestedByOs;
        private Observer mObserver;

        public TestPermissionProvider(boolean isAdvancedProtectionRequestedByOs) {
            mIsAdvancedProtectionRequestedByOs = isAdvancedProtectionRequestedByOs;
        }

        @Override
        public boolean isAdvancedProtectionRequestedByOs() {
            return mIsAdvancedProtectionRequestedByOs;
        }

        @Override
        public void addObserver(Observer observer) {
            mObserver = observer;
        }

        @Override
        public void removeObserver(Observer observer) {
            if (mObserver == observer) mObserver = null;
        }

        public void notifyObserver() {
            if (mObserver != null) mObserver.onAdvancedProtectionOsSettingChanged();
        }

        public Observer getObserver() {
            return mObserver;
        }
    }

    private TestPermissionProvider mProvider;

    private void setPermissionProvider(TestPermissionProvider provider) {
        mProvider = provider;
        OsAdditionalSecurityUtil.setInstanceForTesting(provider);
    }

    @Before
    public void setUp() {
        ContextUtils.initApplicationContextForTests(RuntimeEnvironment.getApplication());
        AwAdvancedProtectionStatusManagerBridgeJni.setInstanceForTesting(mNativeMock);
    }

    @After
    public void tearDown() {
        AwAdvancedProtectionStatusManagerBridge.stopObserving();
        OsAdditionalSecurityUtil.setInstanceForTesting(null);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @Config(sdk = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    // Permission is not granted, SDK is < 36, which doesn't include AAPM.
    public void testIsUnderAdvancedProtection_noPermission() {
        setPermissionProvider(
                new TestPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ true));
        assertFalse(AwAdvancedProtectionStatusManagerBridge.isUnderAdvancedProtection());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void testIsUnderAdvancedProtection_noServiceProvider() {
        Shadows.shadowOf(RuntimeEnvironment.getApplication())
                .grantPermissions(Manifest.permission.QUERY_ADVANCED_PROTECTION_MODE);
        setPermissionProvider(null);
        assertFalse(AwAdvancedProtectionStatusManagerBridge.isUnderAdvancedProtection());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void testIsUnderAdvancedProtection_providerReturnsFalse() {
        Shadows.shadowOf(RuntimeEnvironment.getApplication())
                .grantPermissions(Manifest.permission.QUERY_ADVANCED_PROTECTION_MODE);
        setPermissionProvider(
                new TestPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ false));
        assertFalse(AwAdvancedProtectionStatusManagerBridge.isUnderAdvancedProtection());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void testIsUnderAdvancedProtection_providerReturnsTrue() {
        Shadows.shadowOf(RuntimeEnvironment.getApplication())
                .grantPermissions(Manifest.permission.QUERY_ADVANCED_PROTECTION_MODE);
        setPermissionProvider(
                new TestPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ true));
        assertTrue(AwAdvancedProtectionStatusManagerBridge.isUnderAdvancedProtection());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @Config(sdk = Build.VERSION_CODES.BAKLAVA)
    public void testObserver_notifiesNative() {
        Shadows.shadowOf(RuntimeEnvironment.getApplication())
                .grantPermissions(Manifest.permission.QUERY_ADVANCED_PROTECTION_MODE);
        setPermissionProvider(
                new TestPermissionProvider(/* isAdvancedProtectionRequestedByOs= */ true));

        AwAdvancedProtectionStatusManagerBridge.startObserving();
        mProvider.notifyObserver();

        verify(mNativeMock, times(1)).onAdvancedProtectionOsSettingChanged();

        AwAdvancedProtectionStatusManagerBridge.stopObserving();
        assertNull(mProvider.getObserver());
    }
}
