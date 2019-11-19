// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;

/**
 * Tests for {@link NotificationPermissionUpdater}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_NOTIFICATION_DELEGATION_ENROLMENT)
public class NotificationPermissionUpdaterTest {
    private static final Origin ORIGIN = Origin.create("https://www.website.com");
    private static final String PACKAGE_NAME = "com.package.name";
    private static final String OTHER_PACKAGE_NAME = "com.other.package.name";

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock public TrustedWebActivityPermissionManager mPermissionManager;
    @Mock public TrustedWebActivityClient mTrustedWebActivityClient;

    private NotificationPermissionUpdater mNotificationPermissionUpdater;
    private ShadowPackageManager mShadowPackageManager;

    private boolean mNotificationsEnabled;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        PackageManager pm = RuntimeEnvironment.application.getPackageManager();
        mShadowPackageManager = shadowOf(pm);
        mNotificationPermissionUpdater = new NotificationPermissionUpdater(
                RuntimeEnvironment.application, mPermissionManager, mTrustedWebActivityClient);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesntRegister_whenClientDoesntHandleIntents() {
        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);

        verifyPermissionNotUpdated();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesntRegister_whenOtherClientHandlesIntent() {
        installBrowsableIntentHandler(ORIGIN, "com.package.other");

        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);

        verifyPermissionNotUpdated();
    }


    @Test
    @Feature("TrustedWebActivities")
    public void doesntRegister_whenClientDoesntHaveService() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);

        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);

        verifyPermissionNotUpdated();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void disablesNotifications_whenClientNotificationsAreDisabled() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);
        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setNotificationsEnabledForClient(false);

        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);

        verifyPermissionUpdated(false);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void enablesNotifications_whenClientNotificationsAreEnabled() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);
        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setNotificationsEnabledForClient(true);

        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);

        verifyPermissionUpdated(true);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void updatesPermission_onSubsequentCalls() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);

        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setNotificationsEnabledForClient(true);
        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);
        verifyPermissionUpdated(true);

        setNotificationsEnabledForClient(false);
        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);
        verifyPermissionUpdated(false);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void updatesPermission_onNewClient() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);
        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setNotificationsEnabledForClient(true);
        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);
        verifyPermissionUpdated(true);

        installBrowsableIntentHandler(ORIGIN, OTHER_PACKAGE_NAME);
        installTrustedWebActivityService(ORIGIN, OTHER_PACKAGE_NAME);
        setNotificationsEnabledForClient(false);
        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, OTHER_PACKAGE_NAME);
        verifyPermissionUpdated(OTHER_PACKAGE_NAME, false);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void unregisters_onClientUninstall() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);
        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setNotificationsEnabledForClient(true);

        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);

        uninstallTrustedWebActivityService(ORIGIN);
        mNotificationPermissionUpdater.onClientAppUninstalled(ORIGIN);

        verifyPermissionUnregistered();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesntUnregister_whenOtherClientsRemain() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);

        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setNotificationsEnabledForClient(true);

        mNotificationPermissionUpdater.onOriginVerified(ORIGIN, PACKAGE_NAME);
        verifyPermissionUpdated(true);

        // Since we haven't called uninstallTrustedWebActivityService, the Updater sees that
        // notifications can still be handled by other apps. We don't unregister, but we do update
        // to the permission to that of the other app.
        setNotificationsEnabledForClient(false);
        mNotificationPermissionUpdater.onClientAppUninstalled(ORIGIN);
        verifyPermissionNotUnregistered();
        verifyPermissionUpdated(false);

        uninstallTrustedWebActivityService(ORIGIN);
        mNotificationPermissionUpdater.onClientAppUninstalled(ORIGIN);
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
        when(mTrustedWebActivityClient.checkNotificationPermission(eq(origin), any())).thenAnswer(
                invocation -> {
                    TrustedWebActivityClient.NotificationPermissionCheckCallback callback =
                            ((TrustedWebActivityClient.NotificationPermissionCheckCallback)
                                    invocation.getArgument(1));
                    callback.onPermissionCheck(
                            new ComponentName(packageName, "FakeClass"),
                            mNotificationsEnabled);
                    return true;
                }
        );
    }

    private void setNotificationsEnabledForClient(boolean enabled) {
        mNotificationsEnabled = enabled;
    }

    private void uninstallTrustedWebActivityService(Origin origin) {
        when(mTrustedWebActivityClient.checkNotificationPermission(eq(origin), any()))
                .thenReturn(false);
    }

    private void verifyPermissionNotUpdated() {
        verify(mPermissionManager, never()).register(any(), anyString(), anyBoolean());
    }

    private void verifyPermissionUpdated(boolean enabled) {
        verifyPermissionUpdated(PACKAGE_NAME, enabled);
    }

    private void verifyPermissionUpdated(String packageName, boolean enabled) {
        verify(mPermissionManager).register(eq(ORIGIN), eq(packageName), eq(enabled));
    }

    private void verifyPermissionUnregistered() {
        verify(mPermissionManager).unregister(eq(ORIGIN));
    }

    private void verifyPermissionNotUnregistered() {
        verify(mPermissionManager, never()).unregister(eq(ORIGIN));
    }
}
