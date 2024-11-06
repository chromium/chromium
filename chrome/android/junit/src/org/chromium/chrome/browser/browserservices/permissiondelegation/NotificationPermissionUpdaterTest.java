// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;

/** Tests for {@link NotificationPermissionUpdater}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class NotificationPermissionUpdaterTest {
    private static final Origin ORIGIN = Origin.create("https://www.website.com");
    private static final String URL = "https://www.website.com";
    private static final String PACKAGE_NAME = "com.package.name";
    private static final String APP_LABEL = "name";
    private static final String OTHER_PACKAGE_NAME = "com.other.package.name";

    @Mock public TrustedWebActivityClient mTrustedWebActivityClient;
    @Mock public InstalledWebappPermissionStore mStore;

    private ShadowPackageManager mShadowPackageManager;

    @ContentSettingValues private int mNotificationPermission;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        PackageManager pm = RuntimeEnvironment.application.getPackageManager();
        mShadowPackageManager = shadowOf(pm);
        mShadowPackageManager.installPackage(generateTestPackageInfo(PACKAGE_NAME));
        mShadowPackageManager.installPackage(generateTestPackageInfo(OTHER_PACKAGE_NAME));
        WebappRegistry.getInstance().setPermissionStoreForTesting(mStore);
        TrustedWebActivityClient.setInstanceForTesting(mTrustedWebActivityClient);

        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);
    }

    private PackageInfo generateTestPackageInfo(String packageName) {
        ApplicationInfo appInfo = new ApplicationInfo();
        appInfo.flags = ApplicationInfo.FLAG_INSTALLED;
        appInfo.packageName = packageName;
        appInfo.sourceDir = "/";
        appInfo.name = APP_LABEL;

        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.applicationInfo = appInfo;
        packageInfo.versionCode = 1;
        return packageInfo;
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesntRegister_whenClientDoesntHaveService() {
        NotificationPermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);

        verifyPermissionNotUpdated();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void disablesNotifications_whenClientNotificationsAreDisabled() {
        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setNotificationPermission(ContentSettingValues.BLOCK);

        NotificationPermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);

        verifyPermissionUpdated(ContentSettingValues.BLOCK);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void enablesNotifications_whenClientNotificationsAreEnabled() {
        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setNotificationPermission(ContentSettingValues.ALLOW);

        NotificationPermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);

        verifyPermissionUpdated(ContentSettingValues.ALLOW);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void updatesPermission_onSubsequentCalls() {

        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setNotificationPermission(ContentSettingValues.ALLOW);
        NotificationPermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);
        verifyPermissionUpdated(ContentSettingValues.ALLOW);

        setNotificationPermission(ContentSettingValues.BLOCK);
        NotificationPermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);
        verifyPermissionUpdated(ContentSettingValues.BLOCK);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void updatesPermission_onNewClient() {
        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setNotificationPermission(ContentSettingValues.ALLOW);
        NotificationPermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);
        verifyPermissionUpdated(ContentSettingValues.ALLOW);

        installTrustedWebActivityService(ORIGIN, OTHER_PACKAGE_NAME);
        setNotificationPermission(ContentSettingValues.BLOCK);
        NotificationPermissionUpdater.onOriginVerified(ORIGIN, URL, OTHER_PACKAGE_NAME);
        verifyPermissionUpdated(OTHER_PACKAGE_NAME, ContentSettingValues.BLOCK);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void unregisters_onClientUninstall() {
        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setNotificationPermission(ContentSettingValues.ALLOW);

        NotificationPermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);

        uninstallTrustedWebActivityService(ORIGIN);
        NotificationPermissionUpdater.onClientAppUninstalled(ORIGIN);

        verifyPermissionUnregistered();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesntUnregister_whenOtherClientsRemain() {

        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setNotificationPermission(ContentSettingValues.ALLOW);

        NotificationPermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);
        verifyPermissionUpdated(ContentSettingValues.ALLOW);

        // Since we haven't called uninstallTrustedWebActivityService, the Updater sees that
        // notifications can still be handled by other apps. We don't unregister, but we do update
        // to the permission to that of the other app.
        setNotificationPermission(ContentSettingValues.BLOCK);
        NotificationPermissionUpdater.onClientAppUninstalled(ORIGIN);
        verifyPermissionNotUnregistered();
        verifyPermissionUpdated(ContentSettingValues.BLOCK);

        uninstallTrustedWebActivityService(ORIGIN);
        NotificationPermissionUpdater.onClientAppUninstalled(ORIGIN);
        verifyPermissionUnregistered();
    }

    /** "Installs" the given package to handle intents for that origin. */
    private void installBrowsableIntentHandler(Origin origin, String packageName) {
        Intent intent = new Intent();
        intent.setPackage(packageName);
        intent.setData(origin.uri());
        intent.setAction(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);

        mShadowPackageManager.addResolveInfoForIntent(intent, new ResolveInfo());
    }

    /** "Installs" a Trusted Web Activity Service for the origin. */
    @SuppressWarnings("unchecked")
    private void installTrustedWebActivityService(Origin origin, String packageName) {
        doAnswer(
                        invocation -> {
                            TrustedWebActivityClient.PermissionCallback callback =
                                    invocation.getArgument(1);
                            callback.onPermission(
                                    new ComponentName(packageName, "FakeClass"),
                                    mNotificationPermission);
                            return true;
                        })
                .when(mTrustedWebActivityClient)
                .checkNotificationPermission(eq(origin.toString()), any());
    }

    private void setNotificationPermission(@ContentSettingValues int permission) {
        mNotificationPermission = permission;
    }

    private void uninstallTrustedWebActivityService(Origin origin) {
        doAnswer(
                        invocation -> {
                            TrustedWebActivityClient.PermissionCallback callback =
                                    invocation.getArgument(1);
                            callback.onNoTwaFound();
                            return true;
                        })
                .when(mTrustedWebActivityClient)
                .checkNotificationPermission(eq(origin.toString()), any());
    }

    private void verifyPermissionNotUpdated() {
        verify(mStore, never()).setStateForOrigin(any(), anyString(), any(), anyInt(), anyInt());
    }

    private void verifyPermissionUpdated(@ContentSettingValues int permission) {
        verifyPermissionUpdated(PACKAGE_NAME, permission);
    }

    private void verifyPermissionUpdated(String packageName, @ContentSettingValues int permission) {
        verify(mStore)
                .setStateForOrigin(
                        eq(ORIGIN),
                        eq(packageName),
                        eq(APP_LABEL),
                        eq(ContentSettingsType.NOTIFICATIONS),
                        eq(permission));
    }

    private void verifyPermissionUnregistered() {
        verify(mStore).removeOrigin(eq(ORIGIN));
    }

    private void verifyPermissionNotUnregistered() {
        verify(mStore, never()).removeOrigin(eq(ORIGIN));
    }
}
