// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.graphics.Color;
import android.os.Bundle;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivityManager;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.shell_apk.HostBrowserUtils.PackageNameAndComponentName;
import org.chromium.webapk.shell_apk.LaunchHostBrowserSelector;
import org.chromium.webapk.test.WebApkTestHelper;

import java.util.Arrays;

/** Tests for {@link SplashActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class SplashActivityTest {
    public static final String BROWSER_PACKAGE_NAME = "com.google.android.apps.chrome";

    private static final int MODERN_BROWSER_VERSION = 10000;

    private LaunchHostBrowserSelector.Callback mLaunchHostBrowserSelectorCallback;
    private boolean mLaunchHostBrowserSelectorDialogNeeded;

    private ShadowActivityManager mShadowActivityManager;
    private ShadowApplication mShadowApplication;
    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() {
        Context appContext = ApplicationProvider.getApplicationContext();
        ActivityManager activityManager =
                (ActivityManager) appContext.getSystemService(Context.ACTIVITY_SERVICE);
        mShadowActivityManager = Shadows.shadowOf(activityManager);
        mShadowApplication = shadowOf((Application) ApplicationProvider.getApplicationContext());
        mShadowPackageManager = Shadows.shadowOf(appContext.getPackageManager());

        mLaunchHostBrowserSelectorDialogNeeded = false;
        LaunchHostBrowserSelector.setMockForTesting(
                (callback) -> {
                    if (!mLaunchHostBrowserSelectorDialogNeeded) {
                        callback.onBrowserSelected(
                                new PackageNameAndComponentName(BROWSER_PACKAGE_NAME),
                                /* dialogShown= */ false);
                        return;
                    }
                    mLaunchHostBrowserSelectorCallback = callback;
                });

        Bundle metadata = new Bundle();
        metadata.putString(WebApkMetaDataKeys.START_URL, "https://pwa.rocks/");
        metadata.putBoolean(WebApkMetaDataKeys.IS_ARC_CHROMEOS, false);
        WebApkTestHelper.registerWebApkWithMetaData(appContext.getPackageName(), metadata, null);

        // Install browser.
        mShadowPackageManager.addPackage(
                newPackageInfo(BROWSER_PACKAGE_NAME, MODERN_BROWSER_VERSION));
    }

    @After
    public void tearDown() {
        LaunchHostBrowserSelector.setMockForTesting(null);
    }

    // Test common cases that SplashActivity:
    // - Does not finish itself when the WebAPK is launched from the app list.
    // - Finishes itself when the user backs out of the activity stacked on top.
    @Test
    public void testNormalLaunch() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(
                splashActivityController.get().getTaskId(), splashActivityController.get());

        // ActivityController#visible() attaches the activity to the window.
        splashActivityController.create(null).visible().resume();
        RobolectricUtil.runAllBackgroundAndUi();
        assertNotNull(mShadowApplication.getNextStartedActivity());
        assertFalse(splashActivityController.get().isFinishing());

        splashActivityController.get().onActivityResult(0, 0, null);
        splashActivityController.pause().resume();
        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(splashActivityController.get().isFinishing());
    }

    // Test that SplashActivity finishes itself when:
    // - the user backs out of the activity stacked on top
    // AND
    // - the activity is recreated because it was previously killed by the Android OS due to memory
    //   pressure.
    @Test
    public void testWebApkKilledByOomFinishOnBack() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(
                splashActivityController.get().getTaskId(), splashActivityController.get());

        splashActivityController.create(new Bundle()).visible();
        splashActivityController.get().onActivityResult(0, 0, null);
        splashActivityController.resume();
        RobolectricUtil.runAllBackgroundAndUi();
        assertNull(mShadowApplication.getNextStartedActivity());
        assertTrue(splashActivityController.get().isFinishing());
    }

    // Test that SplashActivity does not finish itself when:
    // - the choose-host-browser dialog is up
    // AND
    // - the activity is recreated because it was previously killed by the Android OS due to memory
    //   pressure.
    @Test
    public void testWebApkKilledByOomHostBrowserNotSelected() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(
                splashActivityController.get().getTaskId(), splashActivityController.get());
        mLaunchHostBrowserSelectorDialogNeeded = true;

        splashActivityController.create(new Bundle()).visible();
        // Dialog shown, LaunchHostBrowserSelector callback called after Activity#onResume()
        splashActivityController.resume();
        RobolectricUtil.runAllBackgroundAndUi();
        assertNotNull(mLaunchHostBrowserSelectorCallback);
        mLaunchHostBrowserSelectorCallback.onBrowserSelected(
                new PackageNameAndComponentName(BROWSER_PACKAGE_NAME), /* dialogShown= */ true);
        mLaunchHostBrowserSelectorCallback = null;
        RobolectricUtil.runAllBackgroundAndUi();
        assertNotNull(mShadowApplication.getNextStartedActivity());
        assertFalse(splashActivityController.get().isFinishing());
    }

    // Test that SplashActivity does not finish itself when:
    // - the WebAPK is launched from Android Recents on Android O+
    // AND
    // - the activity is recreated because it was previously killed by the Android OS due to memory
    //   pressure.
    // On pre-O, the activity stacked on top of SplashActivity is recreated but SplashActivity isn't
    // when the user taps the WebAPK in Android recents.
    @Test
    public void testWebApkKilledByOomRecreatedViaRecentsAndroidOPlus() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(splashActivityController.get().getTaskId(), new Activity());

        splashActivityController.create(new Bundle()).visible().resume();
        RobolectricUtil.runAllBackgroundAndUi();
        assertNull(mShadowApplication.getNextStartedActivity());
        assertFalse(splashActivityController.get().isFinishing());
    }

    // Test that SplashActivity does not finish itself when:
    // - the WebAPK is launched from a deep link.
    // AND
    // - the WebAPK is already running, but SplashActivity is not running because it was killed by
    //   the Android OS due to memory pressure.
    @Test
    public void testDeepLink() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(splashActivityController.get().getTaskId(), new Activity());

        splashActivityController.create(new Bundle()).visible();
        splashActivityController.newIntent(new Intent());
        splashActivityController.get().onActivityResult(0, 0, null);
        splashActivityController.resume();
        RobolectricUtil.runAllBackgroundAndUi();

        assertNotNull(mShadowApplication.getNextStartedActivity());
        assertFalse(splashActivityController.get().isFinishing());
    }

    /**
     * Test that SplashActivity does not finish itself when it receives onActivityResult() prior to
     * onNewIntent().
     */
    @Test
    public void testActivityResultBeforeNewIntent() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(splashActivityController.get().getTaskId(), new Activity());

        splashActivityController.create(new Bundle()).visible();
        splashActivityController.get().onActivityResult(0, 0, null);
        splashActivityController.newIntent(new Intent());
        splashActivityController.resume();
        RobolectricUtil.runAllBackgroundAndUi();

        assertNotNull(mShadowApplication.getNextStartedActivity());
        assertFalse(splashActivityController.get().isFinishing());
    }

    /**
     * Test that SplashActivity sets the correct dark theme color when the system is in night mode
     * and the dark theme color is valid.
     */
    @Test
    @Config(qualifiers = "night")
    public void testSplashScreenStatusBarWhenNightModeAndValidDarkThemeColor() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(splashActivityController.get().getTaskId(), new Activity());

        Bundle metadata = new Bundle();
        metadata.putString(WebApkMetaDataKeys.DARK_THEME_COLOR, "4280295456L");
        splashActivityController.get().updateStatusBar(metadata);
        assertEquals(
                Color.parseColor("#202020"),
                splashActivityController.get().getWindow().getStatusBarColor());
    }

    /**
     * Test that SplashActivity sets the light theme color when the system is in night mode and the
     * dark theme color is invalid.
     */
    @Test
    @Config(qualifiers = "night")
    public void testSplashScreenStatusBarWhenNightModeAndiInvalidDarkThemeColor() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(splashActivityController.get().getTaskId(), new Activity());

        Bundle metadata = new Bundle();
        metadata.putString(WebApkMetaDataKeys.THEME_COLOR, "4286611584L");
        metadata.putString(WebApkMetaDataKeys.DARK_THEME_COLOR, "");
        splashActivityController.get().updateStatusBar(metadata);
        assertEquals(
                Color.parseColor("#808080"),
                splashActivityController.get().getWindow().getStatusBarColor());
    }

    /**
     * Test that SplashActivity sets the light theme color when the system is in night mode and the
     * dark theme color is missing.
     */
    @Test
    @Config(qualifiers = "night")
    public void testSplashScreenStatusBarWhenNightModeAndMissingDarkThemeColor() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(splashActivityController.get().getTaskId(), new Activity());

        Bundle metadata = new Bundle();
        metadata.putString(WebApkMetaDataKeys.THEME_COLOR, "4286611584L");
        splashActivityController.get().updateStatusBar(metadata);
        assertEquals(
                Color.parseColor("#808080"),
                splashActivityController.get().getWindow().getStatusBarColor());
    }

    /**
     * Test that SplashActivity sets the default black theme color when the system is in night mode
     * and both theme colors are invalid.
     */
    @Test
    @Config(qualifiers = "night")
    public void testSplashScreenStatusBarWhenNightModeAndAllThemeColorsInvalid() {
        ActivityController<SplashActivity> splashActivityController =
                Robolectric.buildActivity(SplashActivity.class, new Intent());
        setAppTaskTopActivity(splashActivityController.get().getTaskId(), new Activity());

        Bundle metadata = new Bundle();
        metadata.putString(WebApkMetaDataKeys.THEME_COLOR, "");
        metadata.putString(WebApkMetaDataKeys.DARK_THEME_COLOR, "");
        splashActivityController.get().updateStatusBar(metadata);
        assertEquals(
                Color.parseColor("#000000"),
                splashActivityController.get().getWindow().getStatusBarColor());
    }

    /** Sets {@link ActivityManager#getAppTasks()} to have the passed-in top activity. */
    private void setAppTaskTopActivity(int taskId, Activity topActivity) {
        ActivityManager.RecentTaskInfo recentTaskInfo = new ActivityManager.RecentTaskInfo();
        recentTaskInfo.id = taskId;
        recentTaskInfo.topActivity = topActivity.getComponentName();
        ActivityManager.AppTask appTask = mock(ActivityManager.AppTask.class);
        when(appTask.getTaskInfo()).thenReturn(recentTaskInfo);

        mShadowActivityManager.setAppTasks(Arrays.asList(appTask));
    }

    private static PackageInfo newPackageInfo(String packageName, int version) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.versionName = version + ".";
        packageInfo.applicationInfo = new ApplicationInfo();
        packageInfo.applicationInfo.packageName = packageName;
        packageInfo.applicationInfo.metaData = new Bundle();
        return packageInfo;
    }
}
