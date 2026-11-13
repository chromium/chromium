// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.app.AppOpsManager;
import android.content.Context;
import android.os.Process;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.page_info.PageInfoPermissionsController.PermissionObject;
import org.chromium.components.page_info.PermissionParamsListBuilder;
import org.chromium.components.permissions.PermissionUtil;
import org.chromium.ui.base.UiAndroidFeatures;
import org.chromium.ui.permissions.ActivityAndroidPermissionDelegate;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;

/** Unit tests for PermissionParamsListBuilder. */
@RunWith(BaseRobolectricTestRunner.class)
public class PermissionParamsListBuilderUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    private PermissionParamsListBuilder mPermissionParamsListBuilder;

    @Mock Profile mProfileMock;

    @Before
    public void setUp() {
        FakePermissionDelegate.clearBlockedPermissions();
        AndroidPermissionDelegate permissionDelegate = new FakePermissionDelegate();
        RuntimeEnvironment.getApplication().setTheme(R.style.Theme_BrowserUI_DayNight);
        mPermissionParamsListBuilder =
                new PermissionParamsListBuilder(
                        RuntimeEnvironment.getApplication(), permissionDelegate);
    }

    @Test
    public void emptyList() {
        List<PermissionObject> permissions = mPermissionParamsListBuilder.build();
        assertEquals(0, permissions.size());
    }

    @Test
    public void addSingleEntryAndBuild() {
        mPermissionParamsListBuilder.addPermissionEntry(
                "Foo", "foo", ContentSettingsType.COOKIES, true, false);

        List<PermissionObject> permissions = mPermissionParamsListBuilder.build();
        assertEquals(1, permissions.size());
        PermissionObject perm = permissions.get(0);
        assertTrue(perm.allowed);
    }

    @Test
    public void addLocationEntryAndBuildWhenSystemLocationDisabled() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(false);
        mPermissionParamsListBuilder.addPermissionEntry(
                "Test", "test", ContentSettingsType.GEOLOCATION_WITH_OPTIONS, true, false);

        List<PermissionObject> permissions = mPermissionParamsListBuilder.build();
        assertEquals(1, permissions.size());

        PermissionObject perm = permissions.get(0);
        assertEquals(R.string.page_info_android_location_blocked, perm.warningTextResource);
    }

    @Test
    public void arNotificationWhenCameraBlocked() {
        FakePermissionDelegate.blockPermission(android.Manifest.permission.CAMERA);
        mPermissionParamsListBuilder.addPermissionEntry(
                "Test", "test", ContentSettingsType.AR, true, false);

        List<PermissionObject> permissions = mPermissionParamsListBuilder.build();
        assertEquals(1, permissions.size());

        PermissionObject perm = permissions.get(0);
        assertEquals(R.string.page_info_android_ar_camera_blocked, perm.warningTextResource);
    }

    @Test
    @Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
    @EnableFeatures(UiAndroidFeatures.CHECK_LOCAL_NETWORK_PERMISSION_REVOKED_COMPAT)
    public void localNetworkWarningWhenAndroidPermissionRevokedCompat_targetSdkBelow37() {
        Context context = ContextUtils.getApplicationContext();
        int prevTargetSdk = context.getApplicationInfo().targetSdkVersion;
        try {
            context.getApplicationInfo().targetSdkVersion = 36;
            Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
            Shadows.shadowOf(activity)
                    .grantPermissions(PermissionUtil.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK);
            AppOpsManager appOpsManager =
                    (AppOpsManager) context.getSystemService(Context.APP_OPS_SERVICE);
            String op =
                    AppOpsManager.permissionToOp(
                            PermissionUtil.ANDROID_PERMISSION_ACCESS_LOCAL_NETWORK);
            assertNotNull(op);
            Shadows.shadowOf(appOpsManager)
                    .setMode(
                            op,
                            Process.myUid(),
                            context.getPackageName(),
                            AppOpsManager.MODE_IGNORED);

            PermissionParamsListBuilder builder =
                    new PermissionParamsListBuilder(
                            RuntimeEnvironment.getApplication(),
                            new ActivityAndroidPermissionDelegate(new WeakReference<>(activity)));
            builder.addPermissionEntry(
                    "Local network",
                    "local_network",
                    ContentSettingsType.LOCAL_NETWORK,
                    true,
                    false);

            List<PermissionObject> permissions = builder.build();
            assertEquals(1, permissions.size());
            assertEquals(
                    R.string.page_info_android_permission_blocked,
                    permissions.get(0).warningTextResource);

            // Re-enabling the permission in Android Settings clears the warning.
            Shadows.shadowOf(appOpsManager)
                    .setMode(
                            op,
                            Process.myUid(),
                            context.getPackageName(),
                            AppOpsManager.MODE_ALLOWED);
            permissions = builder.build();
            assertEquals(1, permissions.size());
            assertEquals(0, permissions.get(0).warningTextResource);
        } finally {
            context.getApplicationInfo().targetSdkVersion = prevTargetSdk;
        }
    }

    private static class FakePermissionDelegate implements AndroidPermissionDelegate {
        private static final List<String> sBlockedPermissions = new ArrayList<>();

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
