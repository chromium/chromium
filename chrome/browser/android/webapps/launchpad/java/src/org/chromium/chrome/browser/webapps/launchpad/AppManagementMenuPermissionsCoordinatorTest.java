// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps.launchpad;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
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
import org.chromium.components.embedder_support.browser_context.BrowserContextHandle;
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
    private static final String APP_URL = "https://example.com/";

    private static final LaunchpadItem MOCK_ITEM =
            new LaunchpadItem(APP_PACKAGE_NAME, APP_SHORT_NAME, APP_NAME, APP_URL, null, null);

    @Mock
    Activity mActivity;

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;

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

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mView = (AppManagementMenuPermissionsView) LayoutInflater.from(mActivity).inflate(
                R.layout.launchpad_app_menu_permissions, null);
        mCoordinator = new AppManagementMenuPermissionsCoordinator(mView, MOCK_ITEM);
    }

    private void setPermission(@ContentSettingsType int type, @ContentSettingValues int value) {
        when(mWebsitePreferenceBridgeJniMock.getSettingForOrigin(
                     any(BrowserContextHandle.class), eq(type), anyString(), anyString()))
                .thenReturn(value);
    }

    @Test
    public void testMediatorInitialization() {
        setPermission(ContentSettingsType.NOTIFICATIONS, ContentSettingValues.BLOCK);
        setPermission(ContentSettingsType.MEDIASTREAM_MIC, ContentSettingValues.ALLOW);
        setPermission(ContentSettingsType.MEDIASTREAM_CAMERA, ContentSettingValues.ASK);
        setPermission(ContentSettingsType.GEOLOCATION, ContentSettingValues.ASK);

        AppManagementMenuPermissionsMediator mediator =
                new AppManagementMenuPermissionsMediator(APP_URL);
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
}
