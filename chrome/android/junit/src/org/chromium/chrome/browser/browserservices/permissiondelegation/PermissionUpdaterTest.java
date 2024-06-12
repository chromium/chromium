// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.embedder_support.util.Origin;

/** Tests for {@link PermissionUpdater}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PermissionUpdaterTest {
    private static final Origin ORIGIN = Origin.create("https://www.website.com");
    private static final String URL = "https://www.website.com";
    private static final String PACKAGE_NAME = "com.package.name";

    @Mock public InstalledWebappPermissionManager mPermissionManager;

    @Mock public NotificationPermissionUpdater mNotificationsPermissionUpdater;
    @Mock public LocationPermissionUpdater mLocationPermissionUpdater;

    private PermissionUpdater mPermissionUpdater;
    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        PackageManager pm = RuntimeEnvironment.application.getPackageManager();
        mShadowPackageManager = shadowOf(pm);
        mPermissionUpdater =
                new PermissionUpdater(
                        mPermissionManager,
                        mNotificationsPermissionUpdater,
                        mLocationPermissionUpdater);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesntRegister_whenClientDoesntHandleIntents() {
        mPermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);

        verifyPermissionNotUpdated();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesntRegister_whenOtherClientHandlesIntent() {
        installBrowsableIntentHandler(ORIGIN, "com.package.other");

        mPermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);

        verifyPermissionNotUpdated();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesRegister_whenClientHandleIntentCorrectly() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);

        mPermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);

        verifyPermissionWillUpdate();
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

    private void verifyPermissionNotUpdated() {
        verify(mPermissionManager, never()).addDelegateApp(any(), anyString());
        verify(mNotificationsPermissionUpdater, never())
                .onOriginVerified(any(), any(), anyString());
    }

    private void verifyPermissionWillUpdate() {
        verify(mPermissionManager).addDelegateApp(eq(ORIGIN), eq(PACKAGE_NAME));
        verify(mNotificationsPermissionUpdater)
                .onOriginVerified(eq(ORIGIN), eq(URL), eq(PACKAGE_NAME));
    }
}
