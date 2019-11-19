// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static junit.framework.Assert.assertNotNull;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.anyBoolean;
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
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ContentSettingsType;
import org.chromium.chrome.browser.preferences.website.ContentSettingValues;
import org.chromium.chrome.browser.preferences.website.WebsitePreferenceBridge;
import org.chromium.chrome.browser.preferences.website.WebsitePreferenceBridgeJni;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.LocationSettingsTestUtil;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.base.PermissionCallback;

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

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeMock);
        when(mWebsitePreferenceBridgeMock.isPermissionControlledByDSE(
                     anyInt(), anyString(), anyBoolean()))
                .thenReturn(false);
        AndroidPermissionDelegate permissionDelegate = new FakePermissionDelegate();
        mSettingsActivityRequiredListener = new FakeSystemSettingsActivityRequiredListener();
        mPermissionParamsListBuilder =
                new PermissionParamsListBuilder(RuntimeEnvironment.application, permissionDelegate,
                        "https://example.com", mSettingsActivityRequiredListener, result -> {});
    }

    @Test
    public void addSingleEntryAndBuild() {
        Context context = RuntimeEnvironment.application;
        mPermissionParamsListBuilder.addPermissionEntry(
                "Foo", ContentSettingsType.COOKIES, ContentSettingValues.ALLOW);

        List<PageInfoView.PermissionParams> params = mPermissionParamsListBuilder.build();

        assertEquals(1, params.size());
        PageInfoView.PermissionParams permissionParams = params.get(0);

        String expectedStatus = "Foo – " + context.getString(R.string.page_info_permission_allowed);
        assertEquals(expectedStatus, permissionParams.status.toString());

        assertNull(permissionParams.clickCallback);
    }

    @Test
    public void addLocationEntryAndBuildWhenSystemLocationDisabled() {
        LocationSettingsTestUtil.setSystemLocationSettingEnabled(false);
        mPermissionParamsListBuilder.addPermissionEntry(
                "Test", ContentSettingsType.GEOLOCATION, ContentSettingValues.ALLOW);

        List<PageInfoView.PermissionParams> params = mPermissionParamsListBuilder.build();

        assertEquals(1, params.size());
        PageInfoView.PermissionParams permissionParams = params.get(0);
        assertEquals(
                R.string.page_info_android_location_blocked, permissionParams.warningTextResource);

        assertNotNull(permissionParams.clickCallback);
        permissionParams.clickCallback.run();
        assertEquals(1, mSettingsActivityRequiredListener.getCallCount());
        assertEquals(Settings.ACTION_LOCATION_SOURCE_SETTINGS,
                mSettingsActivityRequiredListener.getIntentOverride().getAction());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.APP_NOTIFICATION_STATUS_MESSAGING)
    public void appNotificationStatusMessagingWhenNotificationsDisabled() {
        getMutableNotificationManager().setNotificationsEnabled(false);

        mPermissionParamsListBuilder.addPermissionEntry(
                "", ContentSettingsType.NOTIFICATIONS, ContentSettingValues.ALLOW);

        List<PageInfoView.PermissionParams> params = mPermissionParamsListBuilder.build();

        assertEquals(1, params.size());
        assertEquals(
                R.string.page_info_android_permission_blocked, params.get(0).warningTextResource);
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.APP_NOTIFICATION_STATUS_MESSAGING)
    public void appNotificationStatusMessagingWhenNotificationsEnabled() {
        getMutableNotificationManager().setNotificationsEnabled(true);

        mPermissionParamsListBuilder.addPermissionEntry(
                "", ContentSettingsType.NOTIFICATIONS, ContentSettingValues.ALLOW);

        List<PageInfoView.PermissionParams> params = mPermissionParamsListBuilder.build();

        assertEquals(1, params.size());
        assertEquals(0, params.get(0).warningTextResource);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.APP_NOTIFICATION_STATUS_MESSAGING)
    public void appNotificationStatusMessagingFlagDisabled() {
        getMutableNotificationManager().setNotificationsEnabled(false);

        mPermissionParamsListBuilder.addPermissionEntry(
                "", ContentSettingsType.NOTIFICATIONS, ContentSettingValues.ALLOW);

        List<PageInfoView.PermissionParams> params = mPermissionParamsListBuilder.build();

        assertEquals(1, params.size());
        assertEquals(0, params.get(0).warningTextResource);
    }

    private static ShadowNotificationManager getMutableNotificationManager() {
        NotificationManager notificationManager =
                (NotificationManager) RuntimeEnvironment.application.getSystemService(
                        Context.NOTIFICATION_SERVICE);
        return shadowOf(notificationManager);
    }

    private static class FakePermissionDelegate implements AndroidPermissionDelegate {
        @Override
        public boolean hasPermission(String permission) {
            return true;
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
