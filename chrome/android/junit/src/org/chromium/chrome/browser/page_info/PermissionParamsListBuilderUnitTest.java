// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.page_info.PageInfoPermissionsController.PermissionObject;
import org.chromium.components.page_info.PermissionParamsListBuilder;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for PermissionParamsListBuilder. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PermissionParamsListBuilderUnitTest {
    private PermissionParamsListBuilder mPermissionParamsListBuilder;

    @Mock Profile mProfileMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        FakePermissionDelegate.clearBlockedPermissions();
        AndroidPermissionDelegate permissionDelegate = new FakePermissionDelegate();
        mPermissionParamsListBuilder =
                new PermissionParamsListBuilder(RuntimeEnvironment.application, permissionDelegate);
    }

    @Test
    public void emptyList() {
        List<PermissionObject> permissions = mPermissionParamsListBuilder.build();
        assertEquals(0, permissions.size());
    }

    @Test
    public void addSingleEntryAndBuild() {
        mPermissionParamsListBuilder.addPermissionEntry(
                "Foo", "foo", ContentSettingsType.COOKIES, ContentSettingValues.ALLOW);

        List<PermissionObject> permissions = mPermissionParamsListBuilder.build();
        assertEquals(1, permissions.size());
        PermissionObject perm = permissions.get(0);
        assertTrue(perm.allowed);
    }

    @Test
    public void addLocationEntryAndBuildWhenSystemLocationDisabled() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(false);
        mPermissionParamsListBuilder.addPermissionEntry(
                "Test", "test", ContentSettingsType.GEOLOCATION, ContentSettingValues.ALLOW);

        List<PermissionObject> permissions = mPermissionParamsListBuilder.build();
        assertEquals(1, permissions.size());

        PermissionObject perm = permissions.get(0);
        assertEquals(R.string.page_info_android_location_blocked, perm.warningTextResource);
    }

    @Test
    public void arNotificationWhenCameraBlocked() {
        FakePermissionDelegate.blockPermission(android.Manifest.permission.CAMERA);
        mPermissionParamsListBuilder.addPermissionEntry(
                "Test", "test", ContentSettingsType.AR, ContentSettingValues.ALLOW);

        List<PermissionObject> permissions = mPermissionParamsListBuilder.build();
        assertEquals(1, permissions.size());

        PermissionObject perm = permissions.get(0);
        assertEquals(R.string.page_info_android_ar_camera_blocked, perm.warningTextResource);
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
}
