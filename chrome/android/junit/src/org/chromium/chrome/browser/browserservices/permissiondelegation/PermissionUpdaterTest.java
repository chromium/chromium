// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;
import android.content.pm.SigningInfo;

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
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.embedder_support.util.Origin;

/** Tests for {@link PermissionUpdater}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PermissionUpdaterTest {
    private static final Origin ORIGIN = Origin.create("https://www.website.com");
    private static final String URL = "https://www.website.com";
    private static final String PACKAGE_NAME = "com.package.name";
    private static final String APP_LABEL = "name";

    @Mock InstalledWebappPermissionStore mStore;

    @Mock public TrustedWebActivityClient mTrustedWebActivityClient;

    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        PackageManager pm = RuntimeEnvironment.application.getPackageManager();
        mShadowPackageManager = shadowOf(pm);
        mShadowPackageManager.installPackage(generateTestPackageInfo(PACKAGE_NAME));
        WebappRegistry.getInstance().setPermissionStoreForTesting(mStore);
        TrustedWebActivityClient.setInstanceForTesting(mTrustedWebActivityClient);
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
        packageInfo.signingInfo = new SigningInfo();

        Signature[] signatures = new Signature[] {new Signature("01234567")};
        shadowOf(packageInfo.signingInfo).setSignatures(signatures);
        return packageInfo;
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesntRegister_whenClientDoesntHandleIntents() {
        PermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);

        verifyPermissionNotUpdated();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesntRegister_whenOtherClientHandlesIntent() {
        installBrowsableIntentHandler(ORIGIN, "com.package.other");

        PermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);

        verifyPermissionNotUpdated();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void doesRegister_whenClientHandleIntentCorrectly() {
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);

        PermissionUpdater.onOriginVerified(ORIGIN, URL, PACKAGE_NAME);

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
        verify(mStore, never()).addDelegateApp(any(), any());
        verify(mTrustedWebActivityClient, never()).checkNotificationPermission(any(), any());
    }

    private void verifyPermissionWillUpdate() {
        verify(mStore).addDelegateApp(eq(ORIGIN), any());
        verify(mTrustedWebActivityClient).checkNotificationPermission(eq(URL), any());
    }
}
