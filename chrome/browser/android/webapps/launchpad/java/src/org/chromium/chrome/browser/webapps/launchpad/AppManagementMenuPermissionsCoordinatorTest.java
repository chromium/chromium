// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Build;
import android.view.LayoutInflater;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link AppManagementMenuPermissionsCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AppManagementMenuPermissionsCoordinatorTest {
    private static final String APP_PACKAGE_NAME = "package.name";
    private static final String APP_SHORT_NAME = "App";
    private static final String APP_NAME = "App Name";
    private static final String APP_URL = "https://example.com/123";
    private static final String ORIGIN = "https://example.com";

    private static final LaunchpadItem MOCK_ITEM =
            new LaunchpadItem(APP_PACKAGE_NAME, APP_SHORT_NAME, APP_NAME, APP_URL, null, null);


    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;

    private Activity mActivity;
    private AppManagementMenuPermissionsCoordinator mCoordinator;
    private AppManagementMenuPermissionsView mView;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mocker.mock(WebsitePreferenceBridgeJni.TEST_HOOKS, mWebsitePreferenceBridgeJniMock);

        Profile.setLastUsedProfileForTesting(mock(Profile.class));

        setPermission(ContentSettingsType.NOTIFICATIONS, ContentSettingValues.ASK);
        setPermission(ContentSettingsType.MEDIASTREAM_MIC, ContentSettingValues.ASK);
        setPermission(ContentSettingsType.MEDIASTREAM_CAMERA, ContentSettingValues.ASK);
        setPermission(ContentSettingsType.GEOLOCATION, ContentSettingValues.ASK);

        mActivity = spy(Robolectric.buildActivity(Activity.class).setup().get());
        mView = (AppManagementMenuPermissionsView) LayoutInflater.from(mActivity).inflate(
                R.layout.launchpad_app_menu_permissions, null);
        mCoordinator = new AppManagementMenuPermissionsCoordinator(mActivity, mView, MOCK_ITEM);
    }

    private void setPermission(@ContentSettingsType int type, @ContentSettingValues int value) {
        when(mWebsitePreferenceBridgeJniMock.getPermissionSettingForOrigin(
                     any(BrowserContextHandle.class), eq(type), anyString(), anyString()))
                .thenReturn(value);
    }

    @Test
    public void testMediatorInitialization() {
        setPermission(ContentSettingsType.NOTIFICATIONS, ContentSettingValues.BLOCK);
        setPermission(ContentSettingsType.MEDIASTREAM_MIC, ContentSettingValues.ALLOW);
        setPermission(ContentSettingsType.MEDIASTREAM_CAMERA, ContentSettingValues.ASK);
        setPermission(ContentSettingsType.GEOLOCATION, ContentSettingValues.ASK);

        mCoordinator = new AppManagementMenuPermissionsCoordinator(mActivity, mView, MOCK_ITEM);
        AppManagementMenuPermissionsMediator mediator = mCoordinator.getMediatorForTesting();
        PropertyModel model = mediator.getModel();
        assertEquals(ContentSettingValues.BLOCK,
                model.get(AppManagementMenuPermissionsProperties.NOTIFICATIONS));
        assertEquals(
                ContentSettingValues.ALLOW, model.get(AppManagementMenuPermissionsProperties.MIC));
        assertEquals(
                ContentSettingValues.ASK, model.get(AppManagementMenuPermissionsProperties.CAMERA));
        assertEquals(ContentSettingValues.ASK,
                model.get(AppManagementMenuPermissionsProperties.LOCATION));
        assertNotNull(model.get(AppManagementMenuPermissionsProperties.ON_CLICK));
    }

    @Test
    public void testClickListener() {
        AppManagementMenuPermissionsMediator mediator = mCoordinator.getMediatorForTesting();
        AppManagementMenuPermissionsView.OnButtonClickListener mockPermissionButtonListener =
                mock(AppManagementMenuPermissionsView.OnButtonClickListener.class);
        mediator.getModel().set(
                AppManagementMenuPermissionsProperties.ON_CLICK, mockPermissionButtonListener);

        mView.findViewById(R.id.location_button).callOnClick();
        verify(mockPermissionButtonListener, times(1))
                .onButtonClick(AppManagementMenuPermissionsProperties.LOCATION);

        mView.findViewById(R.id.camera_button).callOnClick();
        verify(mockPermissionButtonListener, times(1))
                .onButtonClick(AppManagementMenuPermissionsProperties.CAMERA);
    }

    @Test
    public void testSetPermissions() {
        setPermission(ContentSettingsType.MEDIASTREAM_MIC, ContentSettingValues.ALLOW);
        setPermission(ContentSettingsType.MEDIASTREAM_CAMERA, ContentSettingValues.BLOCK);

        mCoordinator = new AppManagementMenuPermissionsCoordinator(mActivity, mView, MOCK_ITEM);
        AppManagementMenuPermissionsMediator mediator = mCoordinator.getMediatorForTesting();
        AppManagementMenuPermissionsView.OnButtonClickListener listener =
                mediator.getModel().get(AppManagementMenuPermissionsProperties.ON_CLICK);

        listener.onButtonClick(AppManagementMenuPermissionsProperties.MIC);
        verify(mWebsitePreferenceBridgeJniMock, times(1))
                .setPermissionSettingForOrigin(any(), eq(ContentSettingsType.MEDIASTREAM_MIC),
                        eq(ORIGIN), eq(ORIGIN), eq(ContentSettingValues.BLOCK));

        listener.onButtonClick(AppManagementMenuPermissionsProperties.CAMERA);
        verify(mWebsitePreferenceBridgeJniMock, times(1))
                .setPermissionSettingForOrigin(any(), eq(ContentSettingsType.MEDIASTREAM_CAMERA),
                        eq(ORIGIN), eq(ORIGIN), eq(ContentSettingValues.ALLOW));
    }

    // Test the click listener on no-clickable icon will not set any permission, and will trigger
    // the assertion error.
    @Test(expected = AssertionError.class)
    public void testSetPermissions_noPreviousPermission() {
        setPermission(ContentSettingsType.GEOLOCATION, ContentSettingValues.ASK);

        mCoordinator = new AppManagementMenuPermissionsCoordinator(mActivity, mView, MOCK_ITEM);
        AppManagementMenuPermissionsMediator mediator = mCoordinator.getMediatorForTesting();
        AppManagementMenuPermissionsView.OnButtonClickListener listener =
                mediator.getModel().get(AppManagementMenuPermissionsProperties.ON_CLICK);

        listener.onButtonClick(AppManagementMenuPermissionsProperties.LOCATION);
        verify(mWebsitePreferenceBridgeJniMock, never())
                .setPermissionSettingForOrigin(
                        any(), eq(ContentSettingsType.GEOLOCATION), eq(ORIGIN), eq(ORIGIN), any());
    }

    // Test click icon to set notification permission on pre-O devices will set notifications
    // permission directly.
    @Test
    @Config(sdk = Build.VERSION_CODES.N_MR1)
    public void testSetPermissions_notificationsPreO() {
        setPermission(ContentSettingsType.NOTIFICATIONS, ContentSettingValues.ALLOW);

        mCoordinator = new AppManagementMenuPermissionsCoordinator(mActivity, mView, MOCK_ITEM);
        AppManagementMenuPermissionsMediator mediator = mCoordinator.getMediatorForTesting();
        AppManagementMenuPermissionsView.OnButtonClickListener listener =
                mediator.getModel().get(AppManagementMenuPermissionsProperties.ON_CLICK);

        listener.onButtonClick(AppManagementMenuPermissionsProperties.NOTIFICATIONS);
        verify(mWebsitePreferenceBridgeJniMock, times(1))
                .setPermissionSettingForOrigin(any(), eq(ContentSettingsType.NOTIFICATIONS),
                        eq(ORIGIN), eq(ORIGIN), eq(ContentSettingValues.BLOCK));
        verify(mActivity, never()).startActivity(any());
    }

    // Test click icon to set notification permission on O+ devices will open the notification
    // channel setting.
    @Test
    @Config(sdk = Build.VERSION_CODES.O)
    public void testSetPermissions_notificationsChannel() {
        setPermission(ContentSettingsType.NOTIFICATIONS, ContentSettingValues.ALLOW);

        mCoordinator = new AppManagementMenuPermissionsCoordinator(mActivity, mView, MOCK_ITEM);
        AppManagementMenuPermissionsMediator mediator = mCoordinator.getMediatorForTesting();
        AppManagementMenuPermissionsView.OnButtonClickListener listener =
                mediator.getModel().get(AppManagementMenuPermissionsProperties.ON_CLICK);

        listener.onButtonClick(AppManagementMenuPermissionsProperties.NOTIFICATIONS);
        verify(mWebsitePreferenceBridgeJniMock, never())
                .setPermissionSettingForOrigin(any(), eq(ContentSettingsType.NOTIFICATIONS),
                        eq(ORIGIN), eq(ORIGIN), eq(ContentSettingValues.BLOCK));
        verify(mActivity, times(1)).startActivity(any());
    }

    // Test open management menu for WebAPK with invalid start URL will NOT fetch permissions. The
    // permission icons are not clickable.
    @Test
    public void testInvalidUrl() {
        String invalidUrl = "notUrl";
        assertNull(Origin.create(invalidUrl));
        LaunchpadItem item = new LaunchpadItem(
                APP_PACKAGE_NAME, APP_SHORT_NAME, APP_NAME, invalidUrl, null, null);
        mCoordinator = new AppManagementMenuPermissionsCoordinator(mActivity, mView, item);
        AppManagementMenuPermissionsMediator mediator = mCoordinator.getMediatorForTesting();

        assertEquals(
                0, mediator.getModel().get(AppManagementMenuPermissionsProperties.NOTIFICATIONS));
        assertEquals(0, mediator.getModel().get(AppManagementMenuPermissionsProperties.MIC));
        assertEquals(0, mediator.getModel().get(AppManagementMenuPermissionsProperties.CAMERA));
        assertEquals(0, mediator.getModel().get(AppManagementMenuPermissionsProperties.LOCATION));
        assertNull(mediator.getModel().get(AppManagementMenuPermissionsProperties.ON_CLICK));
    }
}
