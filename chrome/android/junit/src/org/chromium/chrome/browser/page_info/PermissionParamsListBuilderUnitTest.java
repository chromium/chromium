// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static junit.framework.Assert.assertNotNull;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.provider.Settings;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowNotificationManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
import org.chromium.components.page_info.PageInfoView;
import org.chromium.components.page_info.PermissionParamsListBuilder;
import org.chromium.components.page_info.SystemSettingsActivityRequiredListener;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for PermissionParamsListBuilder.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PermissionParamsListBuilderUnitTest {
    private PermissionParamsListBuilder mPermissionParamsListBuilder;
    private FakeSystemSettingsActivityRequiredListener mSettingsActivityRequiredListener;

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeMock;

    @Mock
    Profile mProfileMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ChromePermissionParamsListBuilderDelegate.setProfileForTesting(mProfileMock);
        mocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeMock);
        when(mWebsitePreferenceBridgeMock.isPermissionControlledByDSE(
                     any(BrowserContextHandle.class), anyInt(), anyString()))
                .thenReturn(false);
        FakePermissionDelegate.clearBlockedPermissions();
        AndroidPermissionDelegate permissionDelegate = new FakePermissionDelegate();
        mSettingsActivityRequiredListener = new FakeSystemSettingsActivityRequiredListener();
        mPermissionParamsListBuilder =
                new PermissionParamsListBuilder(RuntimeEnvironment.application, permissionDelegate,
                        "https://example.com", true, mSettingsActivityRequiredListener,
                        result -> {}, new ChromePermissionParamsListBuilderDelegate());
    }

    @Test
    public void emptyList() {
        PageInfoView.PermissionParams params = mPermissionParamsListBuilder.build();
        assertFalse(params.show_title);
        assertEquals(0, params.permissions.size());
    }

    @Test
    public void addSingleEntryAndBuild() {
        Context context = RuntimeEnvironment.application;
        mPermissionParamsListBuilder.addPermissionEntry(
                "Foo", ContentSettingsType.COOKIES, ContentSettingValues.ALLOW);

        PageInfoView.PermissionParams params = mPermissionParamsListBuilder.build();
        assertTrue(params.show_title);

        assertEquals(1, params.permissions.size());
        PageInfoView.PermissionRowParams permissionParams = params.permissions.get(0);

        String expectedStatus = "Foo – " + context.getString(R.string.page_info_permission_allowed);
        assertEquals(expectedStatus, permissionParams.status.toString());

        assertNull(permissionParams.clickCallback);
    }

    @Test
    public void addLocationEntryAndBuildWhenSystemLocationDisabled() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(false);
        mPermissionParamsListBuilder.addPermissionEntry(
                "Test", ContentSettingsType.GEOLOCATION, ContentSettingValues.ALLOW);

        List<PageInfoView.PermissionRowParams> rows =
                mPermissionParamsListBuilder.build().permissions;

        assertEquals(1, rows.size());
        PageInfoView.PermissionRowParams permissionParams = rows.get(0);
        assertEquals(
                R.string.page_info_android_location_blocked, permissionParams.warningTextResource);

        assertNotNull(permissionParams.clickCallback);
        permissionParams.clickCallback.run();
        assertEquals(1, mSettingsActivityRequiredListener.getCallCount());
        assertEquals(Settings.ACTION_LOCATION_SOURCE_SETTINGS,
                mSettingsActivityRequiredListener.getIntentOverride().getAction());
    }

    @Test
    public void arNotificationWhenCameraBlocked() {
        FakePermissionDelegate.blockPermission(android.Manifest.permission.CAMERA);
        mPermissionParamsListBuilder.addPermissionEntry(
                "Test", ContentSettingsType.AR, ContentSettingValues.ALLOW);

        List<PageInfoView.PermissionRowParams> rows =
                mPermissionParamsListBuilder.build().permissions;

        assertEquals(1, rows.size());
        PageInfoView.PermissionRowParams permissionParams = rows.get(0);
        assertEquals(
                R.string.page_info_android_ar_camera_blocked, permissionParams.warningTextResource);
    }

    private static ShadowNotificationManager getMutableNotificationManager() {
        NotificationManager notificationManager =
                (NotificationManager) RuntimeEnvironment.application.getSystemService(
                        Context.NOTIFICATION_SERVICE);
        return shadowOf(notificationManager);
    }

    private static class FakePermissionDelegate implements AndroidPermissionDelegate {
        private static List<String> sBlockedPermissions = new ArrayList<String>();

        private static void blockPermission(String permission) {
            sBlockedPermissions.add(permission);
        }

        private static void clearBlockedPermissions() {
            sBlockedPermissions.clear();
        }

        @Override
        public boolean hasPermission(String permission) {
            return !sBlockedPermissions.contains(permission);
        }

        @Override
        public boolean canRequestPermission(String permission) {
            return true;
        }

        @Override
        public boolean isPermissionRevokedByPolicy(String permission) {
            return false;
        }

        @Override
        public void requestPermissions(String[] permissions, PermissionCallback callback) {}

        @Override
        public boolean handlePermissionResult(
                int requestCode, String[] permissions, int[] grantResults) {
            return false;
        }
    }

    private static class FakeSystemSettingsActivityRequiredListener
            implements SystemSettingsActivityRequiredListener {
        int mCallCount;
        Intent mIntentOverride;

        @Override
        public void onSystemSettingsActivityRequired(Intent intentOverride) {
            mCallCount++;
            mIntentOverride = intentOverride;
        }

        public int getCallCount() {
            return mCallCount;
        }

        Intent getIntentOverride() {
            return mIntentOverride;
        }
    }
}
