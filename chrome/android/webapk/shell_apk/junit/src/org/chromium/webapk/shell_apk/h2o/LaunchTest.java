// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivityManager;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.shell_apk.CustomAndroidOsShadowAsyncTask;
import org.chromium.webapk.shell_apk.HostBrowserLauncher;
import org.chromium.webapk.shell_apk.HostBrowserUtils;
import org.chromium.webapk.shell_apk.TestBrowserInstaller;
import org.chromium.webapk.shell_apk.WebApkSharedPreferences;
import org.chromium.webapk.test.WebApkTestHelper;

import java.util.ArrayList;

/** Tests launching WebAPK. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {CustomAndroidOsShadowAsyncTask.class})
public final class LaunchTest {
    /** Values based on manifest specified in GN file. */
    private static final String BROWSER_PACKAGE_NAME = "com.google.android.apps.chrome";
    private static final String DEFAULT_START_URL = "https://pwa.rocks/";

    /** Chromium version which does not support showing the splash screen within WebAPK. */
    private static final int BROWSER_H2O_INCOMPATIBLE_VERSION = 57;

    private static String sWebApkPackageName;

    private Context mAppContext;
    private ShadowApplication mShadowApplication;
    private PackageManager mPackageManager;
    private ShadowPackageManager mShadowPackageManager;

    private TestBrowserInstaller mTestBrowserInstaller = new TestBrowserInstaller();

    @Before
    public void setUp() {
        sWebApkPackageName = RuntimeEnvironment.application.getPackageName();

        mShadowApplication = ShadowApplication.getInstance();
        mAppContext = RuntimeEnvironment.application;
        mPackageManager = mAppContext.getPackageManager();
        mShadowPackageManager = Shadows.shadowOf(mPackageManager);

        Bundle metadata = new Bundle();
        metadata.putString(WebApkMetaDataKeys.START_URL, "https://pwa.rocks/");
        WebApkTestHelper.registerWebApkWithMetaData(sWebApkPackageName, metadata, null);
    }

    /**
     * Test launching via a deep link.
     * Check:
     * 1) That the host browser was launched.
     * 2) Which activities were launnched between the activity which handled
     * the intent and the host browser getting launched.
     */
    @Test
    public void testDeepLink() {
        final String deepLinkUrl = "https://pwa.rocks/deep.html";

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(deepLinkUrl));
        launchIntent.setPackage(sWebApkPackageName);

        ArrayList<Intent> launchedIntents;
        launchedIntents =
                launchAndCheckBrowserLaunched(false /* opaqueMainActivityInitiallyEnabled */,
                        false /* browserCompatibleWithSplashActivity */, launchIntent,
                        H2OTransparentLauncherActivity.class);
        Assert.assertEquals(1, launchedIntents.size());
        assertIntentIsForBrowserLaunch(launchedIntents.get(0), deepLinkUrl);

        launchedIntents =
                launchAndCheckBrowserLaunched(false /* opaqueMainActivityInitiallyEnabled */,
                        true /* browserCompatibleWithSplashActivity */, launchIntent,
                        H2OTransparentLauncherActivity.class);
        Assert.assertEquals(5, launchedIntents.size());
        assertIntentComponentClassNameEquals(H2OMainActivity.class, launchedIntents.get(0));
        Assert.assertEquals(BROWSER_PACKAGE_NAME, launchedIntents.get(1).getPackage());
        assertIntentComponentClassNameEquals(
                H2OTransparentLauncherActivity.class, launchedIntents.get(2));
        assertIntentComponentClassNameEquals(SplashActivity.class, launchedIntents.get(3));
        assertIntentIsForBrowserLaunch(launchedIntents.get(4), deepLinkUrl);

        launchedIntents =
                launchAndCheckBrowserLaunched(true /* opaqueMainActivityInitiallyEnabled */,
                        false /* browserCompatibleWithSplashActivity */, launchIntent,
                        H2OTransparentLauncherActivity.class);
        Assert.assertEquals(2, launchedIntents.size());
        assertIntentComponentClassNameEquals(SplashActivity.class, launchedIntents.get(0));
        assertIntentIsForBrowserLaunch(launchedIntents.get(1), deepLinkUrl);

        launchedIntents =
                launchAndCheckBrowserLaunched(true /* opaqueMainActivityInitiallyEnabled */,
                        true /* browserCompatibleWithSplashActivity */, launchIntent,
                        H2OTransparentLauncherActivity.class);
        Assert.assertEquals(2, launchedIntents.size());
        assertIntentComponentClassNameEquals(SplashActivity.class, launchedIntents.get(0));
        assertIntentIsForBrowserLaunch(launchedIntents.get(1), deepLinkUrl);
    }

    /** Test that the host browser is launched as a result of a main launch intent. */
    @Test
    public void testMainIntent() {
        Intent launchIntent = new Intent(Intent.ACTION_MAIN);
        launchIntent.setPackage(sWebApkPackageName);

        ArrayList<Intent> launchedIntents;
        launchedIntents =
                launchAndCheckBrowserLaunched(false /* opaqueMainActivityInitiallyEnabled */,
                        false /* browserCompatibleWithSplashActivity */, launchIntent,
                        H2OMainActivity.class);
        Assert.assertEquals(1, launchedIntents.size());
        assertIntentIsForBrowserLaunch(launchedIntents.get(0), DEFAULT_START_URL);

        launchedIntents =
                launchAndCheckBrowserLaunched(false /* opaqueMainActivityInitiallyEnabled */,
                        true /* browserCompatibleWithSplashActivity */, launchIntent,
                        H2OMainActivity.class);
        Assert.assertEquals(4, launchedIntents.size());
        Assert.assertEquals(BROWSER_PACKAGE_NAME, launchedIntents.get(0).getPackage());
        assertIntentComponentClassNameEquals(
                H2OTransparentLauncherActivity.class, launchedIntents.get(1));
        assertIntentComponentClassNameEquals(SplashActivity.class, launchedIntents.get(2));
        assertIntentIsForBrowserLaunch(launchedIntents.get(3), DEFAULT_START_URL);

        launchedIntents =
                launchAndCheckBrowserLaunched(true /* opaqueMainActivityInitiallyEnabled */,
                        false /* browserCompatibleWithSplashActivity */, launchIntent,
                        H2OOpaqueMainActivity.class);
        Assert.assertEquals(2, launchedIntents.size());
        assertIntentComponentClassNameEquals(SplashActivity.class, launchedIntents.get(0));
        assertIntentIsForBrowserLaunch(launchedIntents.get(1), DEFAULT_START_URL);

        launchedIntents =
                launchAndCheckBrowserLaunched(true /* opaqueMainActivityInitiallyEnabled */,
                        true /* browserCompatibleWithSplashActivity */, launchIntent,
                        H2OOpaqueMainActivity.class);
        Assert.assertEquals(2, launchedIntents.size());
        assertIntentComponentClassNameEquals(SplashActivity.class, launchedIntents.get(0));
        assertIntentIsForBrowserLaunch(launchedIntents.get(1), DEFAULT_START_URL);
    }

    /**
     * Tests that the target share activity is propagated to the host browser launch intent in
     * the scenario where there are several hops between the share intent getting handled and the
     * browser getting launched.
     */
    @Test
    public void testTargetShareActivityPreserved() {
        Bundle metadata = new Bundle();
        metadata.putString(WebApkMetaDataKeys.START_URL, "https://pwa.rocks/");
        Bundle[] shareMetadata = new Bundle[2];
        for (int i = 0; i < shareMetadata.length; ++i) {
            shareMetadata[i] = new Bundle();
            shareMetadata[i].putString(WebApkMetaDataKeys.SHARE_ACTION, "https://pwa.rocks/share");
        }
        WebApkTestHelper.registerWebApkWithMetaData(sWebApkPackageName, metadata, shareMetadata);

        final String shareActivityClassName =
                WebApkTestHelper.getGeneratedShareTargetActivityClassName(1);
        Intent launchIntent = new Intent(Intent.ACTION_SEND);
        launchIntent.setComponent(new ComponentName(sWebApkPackageName, shareActivityClassName));
        launchIntent.putExtra(Intent.EXTRA_TEXT, "subject_value");

        ArrayList<Intent> launchedIntents =
                launchAndCheckBrowserLaunched(true /* opaqueMainActivityInitiallyEnabled */,
                        false /* browserCompatibleWithSplashActivity */, launchIntent,
                        H2OTransparentLauncherActivity.class);
        Assert.assertTrue(launchedIntents.size() > 1);

        Intent browserLaunchIntent = launchedIntents.get(launchedIntents.size() - 1);
        Assert.assertEquals(shareActivityClassName,
                browserLaunchIntent.getStringExtra(
                        WebApkConstants.EXTRA_WEBAPK_SELECTED_SHARE_TARGET_ACTIVITY_CLASS_NAME));
    }

    /**
     * Tests that the EXTRA_SOURCE intent extra in the launch intent is propagated to the host
     * browser launch intent in the scenario where there are several activity hops between
     * the deep link getting handled and the host browser getting launched.
     */
    @Test
    public void testSourcePropagated() {
        final String deepLinkUrl = "https://pwa.rocks/deep_link.html";
        final int source = 2;

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(deepLinkUrl));
        launchIntent.setPackage(sWebApkPackageName);
        launchIntent.putExtra(WebApkConstants.EXTRA_SOURCE, source);

        ArrayList<Intent> launchedIntents =
                launchAndCheckBrowserLaunched(true /* opaqueMainActivityInitiallyEnabled */,
                        true /* browserCompatibleWithSplashActivity */, launchIntent,
                        H2OTransparentLauncherActivity.class);
        Assert.assertTrue(launchedIntents.size() > 1);

        Intent browserLaunchIntent = launchedIntents.get(launchedIntents.size() - 1);
        Assert.assertEquals(
                source, browserLaunchIntent.getIntExtra(WebApkConstants.EXTRA_SOURCE, -1));
    }

    /**
     * Check that the WebAPK does not propagate the {@link EXTRA_RELAUNCH} extra. When
     * the host browser relaunches the WebAPK, the host browser might copy over all of
     * the extras and not remove the relaunch intent. Check that this scenario does not
     * yield an infinite loop.
     */
    @Test
    public void testDoesNotPropagateRelaunchDirective() {
        final String deepLinkUrl = "https://pwa.rocks/deep_link.html";

        Intent launchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(deepLinkUrl));
        launchIntent.setPackage(sWebApkPackageName);
        launchIntent.putExtra(WebApkConstants.EXTRA_RELAUNCH, true);

        ArrayList<Intent> launchedIntents =
                launchAndCheckBrowserLaunched(true /* opaqueMainActivityInitiallyEnabled */,
                        true /* browserCompatibleWithSplashActivity */, launchIntent,
                        H2OTransparentLauncherActivity.class);
        Assert.assertTrue(launchedIntents.size() > 1);

        Intent browserLaunchIntent = launchedIntents.get(launchedIntents.size() - 1);
        Assert.assertFalse(browserLaunchIntent.hasExtra(WebApkConstants.EXTRA_RELAUNCH));
    }

    /**
     * Test that WebAPK does not keep asking the host browser to relaunch the WebAPK if changing the
     * enabled component is slow.
     */
    @Test
    public void testDoesNotLoopIfEnablingInitialSplashActivityIsSlow() {
        // InitialSplashActivity is disabled. Host browser is compatible with SplashActivity.
        changeWebApkActivityEnabledSetting(mPackageManager, H2OOpaqueMainActivity.class,
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED);
        changeWebApkActivityEnabledSetting(mPackageManager, H2OMainActivity.class,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED);
        installBrowser(BROWSER_PACKAGE_NAME,
                HostBrowserUtils.MINIMUM_REQUIRED_CHROMIUM_VERSION_NEW_SPLASH);

        Intent launchIntent = new Intent(Intent.ACTION_MAIN);
        launchIntent.setPackage(sWebApkPackageName);

        // WebAPK requested host browser to relaunch WebAPK recently. The WebAPK should not ask
        // the host browser to relaunch it again.
        {
            SharedPreferences.Editor editor = WebApkSharedPreferences.getPrefs(mAppContext).edit();
            editor.putLong(WebApkSharedPreferences.PREF_REQUEST_HOST_BROWSER_RELAUNCH_TIMESTAMP,
                    System.currentTimeMillis() - 1);
            editor.apply();

            buildActivityFully(H2OMainActivity.class, launchIntent);
            Intent startedActivityIntent = mShadowApplication.getNextStartedActivity();
            Assert.assertEquals(BROWSER_PACKAGE_NAME, startedActivityIntent.getPackage());
            Assert.assertFalse(startedActivityIntent.hasExtra(WebApkConstants.EXTRA_RELAUNCH));
        }

        // WebAPK requested host browser to relaunch WebAPK a long time ago. The WebAPK should ask
        // the host browser to relaunch it.
        {
            SharedPreferences.Editor editor = WebApkSharedPreferences.getPrefs(mAppContext).edit();
            editor.putLong(WebApkSharedPreferences.PREF_REQUEST_HOST_BROWSER_RELAUNCH_TIMESTAMP, 1);
            editor.apply();

            buildActivityFully(H2OMainActivity.class, launchIntent);
            Intent startedActivityIntent = mShadowApplication.getNextStartedActivity();
            Assert.assertEquals(BROWSER_PACKAGE_NAME, startedActivityIntent.getPackage());
            Assert.assertTrue(startedActivityIntent.hasExtra(WebApkConstants.EXTRA_RELAUNCH));
        }
    }

    /**
     * Test that H2OMainActivity is always used as the entry point when the host browser is
     * org.chromium.arc.intent_helper.
     */
    @Test
    public void testLaunchWithArcIntentHelperHostBrowser() {
        Intent launchIntent = new Intent(Intent.ACTION_MAIN);
        launchIntent.setPackage(sWebApkPackageName);

        changeWebApkActivityEnabledSetting(mPackageManager, H2OOpaqueMainActivity.class,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED);
        changeWebApkActivityEnabledSetting(mPackageManager, H2OMainActivity.class,
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED);
        installBrowser(HostBrowserUtils.ARC_INTENT_HELPER_BROWSER,
                HostBrowserUtils.MINIMUM_REQUIRED_CHROMIUM_VERSION_NEW_SPLASH);
        ArrayList<Intent> launchedIntents = runActivityChain(launchIntent,
                H2OOpaqueMainActivity.class, HostBrowserUtils.ARC_INTENT_HELPER_BROWSER);

        // The entry point should have been switched to H2OMainActivity.
        Assert.assertFalse(isWebApkActivityEnabled(mPackageManager, H2OOpaqueMainActivity.class));
        Assert.assertTrue(isWebApkActivityEnabled(mPackageManager, H2OMainActivity.class));

        Assert.assertTrue(!launchedIntents.isEmpty());
        assertIntentIsForCustomBrowserLaunch(launchedIntents.get(launchedIntents.size() - 1),
                HostBrowserUtils.ARC_INTENT_HELPER_BROWSER, DEFAULT_START_URL);
    }

    /** Checks the name of the intent's component class name. */
    private static void assertIntentComponentClassNameEquals(Class expectedClass, Intent intent) {
        Assert.assertEquals(expectedClass.getName(), intent.getComponent().getClassName());
    }

    /** Checks that the passed in intent launches the host browser with the given URL. */
    private static void assertIntentIsForBrowserLaunch(Intent intent, String expectedStartUrl) {
        assertIntentIsForCustomBrowserLaunch(intent, BROWSER_PACKAGE_NAME, expectedStartUrl);
    }

    /** Checks that the passed in intent launches the given host browser with the given URL. */
    private static void assertIntentIsForCustomBrowserLaunch(
            Intent intent, String browserPackage, String expectedStartUrl) {
        Assert.assertEquals(browserPackage, intent.getPackage());
        Assert.assertEquals(HostBrowserLauncher.ACTION_START_WEBAPK, intent.getAction());
        Assert.assertEquals(expectedStartUrl, intent.getStringExtra(WebApkConstants.EXTRA_URL));
    }

    /**
     * Launches WebAPK with the given intent and configuration. Tests that the host browser is
     * launched and which activities are enabled after the browser launch.
     * @param initialSplashActivityInitiallyEnabled Whether SplashActivity is enabled at the
     *         beginning of the test case.
     * @param browserCompatibleWithSplashActivity Whether the host browser supports the ShellAPK
     *         showing the splash screen.
     * @param launchIntent Intent to launch.
     * @param launchActivity Activity which should receive the launch intent.
     * @return List of launched activity intents (including the host browser launch intent).
     */
    private ArrayList<Intent> launchAndCheckBrowserLaunched(
            boolean opaqueMainActivityInitiallyEnabled, boolean browserCompatibleWithSplashActivity,
            Intent launchIntent, Class<? extends Activity> launchActivity) {
        changeWebApkActivityEnabledSetting(mPackageManager,
                opaqueMainActivityInitiallyEnabled ? H2OOpaqueMainActivity.class
                                                   : H2OMainActivity.class,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED);
        changeWebApkActivityEnabledSetting(mPackageManager,
                opaqueMainActivityInitiallyEnabled ? H2OMainActivity.class
                                                   : H2OOpaqueMainActivity.class,
                PackageManager.COMPONENT_ENABLED_STATE_DISABLED);
        installBrowser(BROWSER_PACKAGE_NAME,
                browserCompatibleWithSplashActivity
                        ? HostBrowserUtils.MINIMUM_REQUIRED_CHROMIUM_VERSION_NEW_SPLASH
                        : BROWSER_H2O_INCOMPATIBLE_VERSION);

        // Android modifies the intent when the intent is used to launch an activity. Clone the
        // intent so as not to affect test cases which use the same intent.
        Intent launchIntentCopy = (Intent) launchIntent.clone();

        ArrayList<Intent> launchedIntents =
                runActivityChain(launchIntentCopy, launchActivity, BROWSER_PACKAGE_NAME);

        Assert.assertEquals(browserCompatibleWithSplashActivity,
                isWebApkActivityEnabled(mPackageManager, H2OOpaqueMainActivity.class));
        Assert.assertEquals(!browserCompatibleWithSplashActivity,
                isWebApkActivityEnabled(mPackageManager, H2OMainActivity.class));

        return launchedIntents;
    }

    /** Changes whether the passed in WebAPK activity is enabled. */
    private static void changeWebApkActivityEnabledSetting(
            PackageManager packageManager, Class<? extends Activity> activity, int enabledSetting) {
        ComponentName component = new ComponentName(sWebApkPackageName, activity.getName());
        packageManager.setComponentEnabledSetting(
                component, enabledSetting, PackageManager.DONT_KILL_APP);
    }

    /** Returns whether the passed in WebAPK activity is enabled. */
    private static boolean isWebApkActivityEnabled(
            PackageManager packageManager, Class<? extends Activity> activity) {
        ComponentName component = new ComponentName(sWebApkPackageName, activity.getName());
        int enabledSetting = packageManager.getComponentEnabledSetting(component);
        return (enabledSetting == PackageManager.COMPONENT_ENABLED_STATE_ENABLED);
    }

    /**
     * Launches activity with the given intent. Runs till the browser package is launched. Returns
     * the chain of launched activities (including the browser launch).
     */
    @SuppressWarnings("unchecked")
    private ArrayList<Intent> runActivityChain(
            Intent launchIntent, Class<? extends Activity> launchActivity, String browserPackage) {
        ArrayList<Intent> activityIntentChain = new ArrayList<Intent>();

        buildActivityFully(launchActivity, launchIntent);
        for (;;) {
            Intent startedActivityIntent = mShadowApplication.getNextStartedActivity();
            if (startedActivityIntent == null) break;

            activityIntentChain.add(startedActivityIntent);

            if (browserPackage.equals(startedActivityIntent.getPackage())) {
                if (!startedActivityIntent.hasExtra(WebApkConstants.EXTRA_RELAUNCH)) break;

                // Emulate host browser relaunch behaviour.
                String startUrl = startedActivityIntent.getStringExtra(WebApkConstants.EXTRA_URL);
                Intent relaunchIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(startUrl));
                relaunchIntent.setComponent(new ComponentName(
                        sWebApkPackageName, H2OTransparentLauncherActivity.class.getName()));
                Bundle startedActivityExtras = startedActivityIntent.getExtras();
                if (startedActivityExtras != null) {
                    relaunchIntent.putExtras(startedActivityExtras);
                }
                relaunchIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                mAppContext.startActivity(relaunchIntent);
                continue;
            }

            Class<? extends Activity> startedActivityClass = null;
            try {
                startedActivityClass = (Class<? extends Activity>) Class.forName(
                        startedActivityIntent.getComponent().getClassName());
            } catch (ClassNotFoundException e) {
                Assert.fail();
            }
            buildActivityFully(startedActivityClass, startedActivityIntent);
        }
        return activityIntentChain;
    }

    private static void buildActivityFully(Class<? extends Activity> activityClass, Intent intent) {
        ActivityController<? extends Activity> controller =
                Robolectric.buildActivity(activityClass, intent);
        setAppTaskTopActivity(controller.get().getTaskId(), controller.get());
        controller.create().start().resume().visible();
    }

    /** Installs browser with the given package name and version. */
    private void installBrowser(String browserPackageName, int version) {
        mTestBrowserInstaller.uninstallBrowser(browserPackageName);
        mTestBrowserInstaller.installBrowserWithVersion(browserPackageName, version + ".");
    }

    private static void setAppTaskTopActivity(int taskId, Activity topActivity) {
        ActivityManager.RecentTaskInfo recentTaskInfo = new ActivityManager.RecentTaskInfo();
        recentTaskInfo.id = taskId;
        recentTaskInfo.topActivity = topActivity.getComponentName();
        ActivityManager.AppTask appTask = Mockito.mock(ActivityManager.AppTask.class);
        Mockito.when(appTask.getTaskInfo()).thenReturn(recentTaskInfo);

        ArrayList<ActivityManager.AppTask> appTasks = new ArrayList<ActivityManager.AppTask>();
        appTasks.add(appTask);

        ActivityManager activityManager =
                (ActivityManager) RuntimeEnvironment.application.getSystemService(
                        Context.ACTIVITY_SERVICE);
        ShadowActivityManager shadowActivityManager = Shadows.shadowOf(activityManager);
        shadowActivityManager.setAppTasks(appTasks);
    }
}
