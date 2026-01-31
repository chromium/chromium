// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.components.page_info.PageInfoMainController;
import org.chromium.components.page_info.PageInfoPermissionsController;
import org.chromium.components.page_info.PageInfoPermissionsController.PermissionObject;
import org.chromium.components.page_info.PageInfoRowView;
import org.chromium.components.permissions.AndroidPermissionRequester;
import org.chromium.components.permissions.PermissionUtil;
import org.chromium.components.permissions.PermissionUtilJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.Arrays;

/** Tests for PageInfoPermissionsController. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PageInfoPermissionsControllerTest {
    @Rule public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Mock private PageInfoMainController mMainController;
    @Mock private PageInfoRowView mRowView;
    @Mock private PageInfoControllerDelegate mDelegate;
    @Mock private WebContents mWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Context mContext;
    @Mock private PermissionUtil.Natives mPermissionUtilJni;

    private PageInfoPermissionsController mController;
    private boolean mRequestAndroidPermissionsResult;
    private AndroidPermissionRequester.RequestDelegate mRequestDelegateCaptured;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PermissionUtilJni.setInstanceForTesting(mPermissionUtilJni);

        when(mRowView.getContext()).thenReturn(mContext);
        when(mContext.getResources())
                .thenReturn(org.chromium.base.ContextUtils.getApplicationContext().getResources());
        when(mMainController.getURL()).thenReturn(new GURL("https://example.com"));
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);

        mController =
                new PageInfoPermissionsController(
                        mMainController,
                        mRowView,
                        mDelegate,
                        mWebContents,
                        ContentSettingsType.NOTIFICATIONS) {
                    @Override
                    protected boolean requestAndroidPermissions(
                            WindowAndroid windowAndroid,
                            int[] contentSettingsTypes,
                            AndroidPermissionRequester.RequestDelegate delegate) {
                        mRequestDelegateCaptured = delegate;
                        return mRequestAndroidPermissionsResult;
                    }

                    @Override
                    protected boolean canCreateSubpageFragment() {
                        return true;
                    }

                    @Override
                    protected @Nullable View addSubpageFragment(BaseSiteSettingsFragment fragment) {
                        return null;
                    }

                    @Override
                    protected void removeSubpageFragment() {}
                };
    }

    @After
    public void tearDown() {
        PermissionUtilJni.setInstanceForTesting(null);
    }

    @Test
    public void testOnNotificationSubscribeClicked_RequestsPermission_Granted() {
        mRequestAndroidPermissionsResult = true;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Permissions.ClapperLoud.PageInfo.OsPromptResolved", true);

        mController.onNotificationSubscribeClicked();

        mRequestDelegateCaptured.onAndroidPermissionAccepted();

        verify(mPermissionUtilJni)
                .resolveNotificationsPermissionRequest(mWebContents, ContentSetting.ALLOW);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnNotificationSubscribeClicked_RequestsPermission_OS_Level_Canceled() {
        mRequestAndroidPermissionsResult = true;
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Permissions.ClapperLoud.PageInfo.OsPromptResolved", false);

        mController.onNotificationSubscribeClicked();

        mRequestDelegateCaptured.onAndroidPermissionCanceled();

        verify(mPermissionUtilJni).dismissNotificationsPermissionRequest(mWebContents);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnNotificationSubscribeClicked_PermissionAlreadyGranted() {
        mRequestAndroidPermissionsResult = false;

        mController.onNotificationSubscribeClicked();

        verify(mPermissionUtilJni)
                .resolveNotificationsPermissionRequest(mWebContents, ContentSetting.ALLOW);
    }

    @Test
    public void testOnNotificationSubscribeClicked_NullWindow() {
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(null);

        mController.onNotificationSubscribeClicked();

        verify(mPermissionUtilJni)
                .resolveNotificationsPermissionRequest(mWebContents, ContentSetting.ALLOW);
    }

    @Test
    public void testOnSubpageRemoved_RequestsPermission_Denied() throws Exception {
        PermissionObject permission =
                new PermissionObject(
                        ContentSettingsType.NOTIFICATIONS,
                        "Notifications",
                        "notifications",
                        /* allowed= */ false,
                        /* warningTextResource= */ 0,
                        /* requested= */ true);
        mController.setPermissions(Arrays.asList(permission));

        mController.onSubpageRemoved();

        verify(mPermissionUtilJni)
                .resolveNotificationsPermissionRequest(mWebContents, ContentSetting.BLOCK);
    }

    @Test
    public void testOnPermissionsReset_RequestsPermission_Revoked() {
        PermissionObject permission =
                new PermissionObject(
                        ContentSettingsType.NOTIFICATIONS,
                        "Notifications",
                        "notifications",
                        /* allowed= */ false,
                        /* warningTextResource= */ 0,
                        /* requested= */ true);
        mController.setPermissions(Arrays.asList(permission));

        mController.onPermissionsReset();

        verify(mPermissionUtilJni)
                .resolveNotificationsPermissionRequest(mWebContents, ContentSetting.DEFAULT);
    }
}
