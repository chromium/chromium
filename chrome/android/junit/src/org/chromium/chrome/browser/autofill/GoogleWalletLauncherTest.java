// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Build;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

import java.util.ArrayList;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = Build.VERSION_CODES.TIRAMISU)
public class GoogleWalletLauncherTest {
    /** Unit tests for {@link AutofillFallbackSurfaceLauncher}. */
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    @Mock private PackageManager mPackageManager;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(TestActivity.class);
    }

    @Test
    public void testOpenGoogleWallet_walletAppInstalled_opensApp() {
        ComponentName walletActivityComponent =
                new ComponentName(
                        GoogleWalletLauncher.GOOGLE_WALLET_PACKAGE_NAME,
                        GoogleWalletLauncher.GOOGLE_WALLET_ACTIVITY_NAME);
        List<ResolveInfo> resolveInfos = createResolvedInfo(walletActivityComponent);
        when(mPackageManager.queryIntentActivities(any(Intent.class), anyInt()))
                .thenReturn(resolveInfos);

        GoogleWalletLauncher.openGoogleWallet(mActivity, mPackageManager);

        ShadowActivity shadowActivity = Shadows.shadowOf(mActivity);
        Intent walletAppIntent = shadowActivity.getNextStartedActivity();
        assertNotNull(walletAppIntent);
        assertEquals(walletActivityComponent, walletAppIntent.getComponent());
    }

    @Test
    public void testOpenGoogleWallet_walletAppNotInstalled_opensCct() {
        GoogleWalletLauncher.openGoogleWallet(mActivity, mPackageManager);

        ShadowActivity shadowActivity = Shadows.shadowOf(mActivity);
        Intent cctIntent = shadowActivity.getNextStartedActivity();
        assertNotNull(cctIntent);
        assertEquals(GoogleWalletLauncher.GOOGLE_WALLET_PASSES_URL, cctIntent.getDataString());
    }

    @Test
    public void testOpenGoogleWalletPassesSettings_walletAppInstalled_opensApp() {
        ComponentName component =
                new ComponentName(
                        GoogleWalletLauncher.GMS_CORE_PACKAGE_NAME,
                        GoogleWalletLauncher.GOOGLE_PAY_ACTIVITY_NAME);
        List<ResolveInfo> resolveInfos = createResolvedInfo(component);
        when(mPackageManager.queryIntentActivities(any(Intent.class), anyInt()))
                .thenReturn(resolveInfos);

        GoogleWalletLauncher.openGoogleWalletPassesSettings(mActivity, mPackageManager);

        ShadowActivity shadowActivity = Shadows.shadowOf(mActivity);
        Intent walletAppIntent = shadowActivity.getNextStartedActivity();
        assertNotNull(walletAppIntent);
        assertEquals(component, walletAppIntent.getComponent());
    }

    @Test
    public void testOpenGoogleWalletPassesSettings_walletAppNotInstalled_opensCct() {
        GoogleWalletLauncher.openGoogleWalletPassesSettings(mActivity, mPackageManager);

        ShadowActivity shadowActivity = Shadows.shadowOf(mActivity);
        Intent cctIntent = shadowActivity.getNextStartedActivity();
        assertNotNull(cctIntent);
        assertEquals(
                GoogleWalletLauncher.GOOGLE_WALLET_MANAGE_PASSES_DATA_URL,
                cctIntent.getDataString());
    }

    private static List<ResolveInfo> createResolvedInfo(ComponentName componentName) {
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = componentName.getPackageName();
        activityInfo.name = componentName.getClassName();
        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.activityInfo = activityInfo;
        List<ResolveInfo> resolveInfos = new ArrayList();
        resolveInfos.add(resolveInfo);

        return resolveInfos;
    }
}
