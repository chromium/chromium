// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.UserManager;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.webapps.WebApkActivity;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.webapk.lib.client.WebApkValidator;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

import androidx.browser.customtabs.CustomTabsIntent;

/** JUnit tests for first run triggering code. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {FirstRunIntegrationUnitTest.MockChromeBrowserInitializer.class})
public final class FirstRunIntegrationUnitTest {
    /** Do nothing version of {@link ChromeBrowserInitializer}. */
    @Implements(ChromeBrowserInitializer.class)
    public static class MockChromeBrowserInitializer {
        @Implementation
        public void __constructor__() {}

        @Implementation
        public void handlePreNativeStartup(final BrowserParts parts) {}
    }

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private ShadowApplication mShadowApplication;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mShadowApplication = ShadowApplication.getInstance();

        UserManager userManager = Mockito.mock(UserManager.class);
        Mockito.when(userManager.isDemoUser()).thenReturn(false);
        mShadowApplication.setSystemService(Context.USER_SERVICE, userManager);

        FirstRunStatus.setFirstRunFlowComplete(false);
        WebApkValidator.disableValidationForTesting();
    }

    /** Checks that the intent component targets the passed-in class. */
    private boolean checkIntentComponentClass(Intent intent, Class componentClass) {
        return checkIntentComponentClassOneOf(intent, new Class[] {componentClass});
    }

    /** Checks that the intent component is one of the provided classes. */
    private boolean checkIntentComponentClassOneOf(Intent intent, Class[] componentClassOptions) {
        if (intent == null || intent.getComponent() == null) return false;

        String componentClassName = intent.getComponent().getClassName();
        for (Class componentClassOption : componentClassOptions) {
            if (componentClassOption.getName().equals(componentClassName)) return true;
        }
        return false;
    }

    /**
     * Checks that intent is either for {@link FirstRunActivity} or
     * {@link TabbedModeFirstRunActivity}.
     */
    private boolean checkIntentIsForFre(Intent intent) {
        return checkIntentComponentClassOneOf(
                intent, new Class[] {FirstRunActivity.class, TabbedModeFirstRunActivity.class});
    }

    /** Builds activity using the component class name from the provided intent. */
    @SuppressWarnings("unchecked")
    private static void buildActivityWithClassNameFromIntent(Intent intent) {
        Class<? extends Activity> activityClass = null;
        try {
            activityClass =
                    (Class<? extends Activity>) Class.forName(intent.getComponent().getClassName());
        } catch (ClassNotFoundException e) {
            Assert.fail();
        }
        Robolectric.buildActivity(activityClass, intent).create();
    }

    /**
     * Checks that either {@link FirstRunActivity} or {@link TabbedModeFirstRunActivity}
     * was launched.
     */
    private void assertFirstRunActivityLaunched() {
        Intent launchedIntent = mShadowApplication.getNextStartedActivity();
        Assert.assertNotNull(launchedIntent);

        Assert.assertTrue(checkIntentIsForFre(launchedIntent));
    }

    @Test
    public void testGenericViewIntentGoesToFirstRun() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://test.com"));
        intent.setPackage(mContext.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Activity launcherActivity =
                Robolectric.buildActivity(ChromeLauncherActivity.class, intent).create().get();
        assertFirstRunActivityLaunched();
        Assert.assertTrue(launcherActivity.isFinishing());
    }

    @Test
    public void testRedirectCustomTabActivityToFirstRun() {
        CustomTabsIntent customTabIntent = new CustomTabsIntent.Builder().build();
        customTabIntent.intent.setPackage(mContext.getPackageName());
        customTabIntent.intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        customTabIntent.launchUrl(mContext, Uri.parse("http://test.com"));
        Intent launchedIntent = mShadowApplication.getNextStartedActivity();
        Assert.assertNotNull(launchedIntent);

        Activity launcherActivity =
                Robolectric.buildActivity(ChromeLauncherActivity.class, launchedIntent)
                        .create()
                        .get();
        assertFirstRunActivityLaunched();
        Assert.assertTrue(launcherActivity.isFinishing());
    }

    @Test
    public void testRedirectChromeTabbedActivityToFirstRun() {
        Intent intent = new Intent();
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Activity tabbedActivity =
                Robolectric.buildActivity(ChromeTabbedActivity.class, intent).create().get();
        assertFirstRunActivityLaunched();
        Assert.assertTrue(tabbedActivity.isFinishing());
    }

    @Test
    public void testRedirectSearchActivityToFirstRun() {
        Intent intent = new Intent();
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Activity searchActivity =
                Robolectric.buildActivity(SearchActivity.class, intent).create().get();
        assertFirstRunActivityLaunched();
        Assert.assertTrue(searchActivity.isFinishing());
    }

    /**
     * Tests that when the first run experience is shown by a WebAPK that the WebAPK is launched
     * when the user finishes the first run experience. In the case where the WebAPK (as opposed
     * to WebApkActivity) displays the splash screen this is necessary for correct behaviour when
     * the user taps the app icon and the WebAPK is still running.
     */
    @Test
    public void testFreRelaunchesWebApkNotWebApkActivity() {
        String webApkPackageName = "org.chromium.webapk.name";
        String startUrl = "https://pwa.rocks/";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, startUrl);
        WebApkTestHelper.registerWebApkWithMetaData(
                webApkPackageName, bundle, null /* shareTargetMetaData */);
        WebApkTestHelper.addIntentFilterForUrl(webApkPackageName, startUrl);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, webApkPackageName);
        intent.putExtra(ShortcutHelper.EXTRA_URL, startUrl);
        intent.putExtra(WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK, true);

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchedIntent = mShadowApplication.getNextStartedActivity();
        while (checkIntentComponentClass(launchedIntent, WebappLauncherActivity.class)) {
            buildActivityWithClassNameFromIntent(launchedIntent);
            launchedIntent = mShadowApplication.getNextStartedActivity();
        }

        Assert.assertTrue(checkIntentIsForFre(launchedIntent));
        PendingIntent freCompleteLaunchIntent = launchedIntent.getParcelableExtra(
                FirstRunActivityBase.EXTRA_FRE_COMPLETE_LAUNCH_INTENT);
        Assert.assertNotNull(freCompleteLaunchIntent);
        Assert.assertEquals(webApkPackageName,
                Shadows.shadowOf(freCompleteLaunchIntent).getSavedIntent().getPackage());
    }

    /**
     * Test that if a WebAPK only requires the lightweight FRE and a user has gone through the
     * lightweight FRE that the WebAPK launches and no FRE is shown to the user.
     */
    @Test
    public void testUserAcceptedLightweightFreLaunch() {
        FirstRunStatus.setLightweightFirstRunFlowComplete(true);

        String webApkPackageName = "unbound.webapk";
        String startUrl = "https://pwa.rocks/";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, startUrl);
        WebApkTestHelper.registerWebApkWithMetaData(
                webApkPackageName, bundle, null /* shareTargetMetaData */);
        WebApkTestHelper.addIntentFilterForUrl(webApkPackageName, startUrl);

        Intent intent = new Intent();
        intent.putExtra(WebApkConstants.EXTRA_WEBAPK_PACKAGE_NAME, webApkPackageName);
        intent.putExtra(ShortcutHelper.EXTRA_URL, startUrl);

        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchedIntent = mShadowApplication.getNextStartedActivity();
        Assert.assertTrue(checkIntentComponentClass(launchedIntent, WebApkActivity.class));
        buildActivityWithClassNameFromIntent(launchedIntent);

        // No FRE should have been launched.
        Assert.assertNull(mShadowApplication.getNextStartedActivity());
    }
}
