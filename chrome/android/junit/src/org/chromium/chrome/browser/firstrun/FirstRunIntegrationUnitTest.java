// Copyright 2018 The Chromium Authors
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

import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.searchwidget.SearchActivity;
import org.chromium.chrome.browser.webapps.WebApkIntentDataProviderFactory;
import org.chromium.chrome.browser.webapps.WebappActivity;
import org.chromium.chrome.browser.webapps.WebappLauncherActivity;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.test.WebApkTestHelper;

import java.util.ArrayList;
import java.util.List;

/** JUnit tests for first run triggering code. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class FirstRunIntegrationUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ChromeBrowserInitializer mChromeBrowserInitializer;

    private final List<ActivityController> mActivityControllerList = new ArrayList<>();

    private Context mContext;
    private ShadowApplication mShadowApplication;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mShadowApplication = ShadowApplication.getInstance();

        UserManager userManager = Mockito.mock(UserManager.class);
        Mockito.when(userManager.isDemoUser()).thenReturn(false);
        mShadowApplication.setSystemService(Context.USER_SERVICE, userManager);
        ChromeBrowserInitializer.setForTesting(mChromeBrowserInitializer);

        FirstRunStatus.setFirstRunFlowComplete(false);
        WebApkValidator.setDisableValidationForTesting(true);
    }

    @After
    public void tearDown() {
        for (ActivityController activityController : mActivityControllerList) {
            activityController.destroy();
        }
    }

    /** Checks that the intent component targets the passed-in class. */
    private boolean checkIntentComponentClass(Intent intent, Class componentClass) {
        if (intent == null || intent.getComponent() == null) return false;

        String intentClassName = intent.getComponent().getClassName();
        return componentClass.getName().equals(intentClassName);
    }

    /** Builds activity using the component class name from the provided intent. */
    @SuppressWarnings("unchecked")
    private void buildActivityWithClassNameFromIntent(Intent intent) {
        Class<? extends Activity> activityClass = null;
        try {
            activityClass =
                    (Class<? extends Activity>) Class.forName(intent.getComponent().getClassName());
        } catch (ClassNotFoundException e) {
            throw new RuntimeException(e);
        }
        createActivity(activityClass, intent);
    }

    /**
     * Launches {@link WebappLauncherActivity}. If WebappLauncherActivity is relaunched, waits for
     * the relaunch to occur.
     */
    private void launchWebappLauncherActivityProcessRelaunch(Intent intent) {
        createActivity(WebappLauncherActivity.class, intent);
        Intent launchedIntent = mShadowApplication.peekNextStartedActivity();
        if (checkIntentComponentClass(launchedIntent, WebappLauncherActivity.class)) {
            // Pop the WebappLauncherActivity from the 'started activities' list.
            mShadowApplication.getNextStartedActivity();
            buildActivityWithClassNameFromIntent(launchedIntent);
        }
    }

    /** Checks that {@link FirstRunActivity} was launched. */
    private void assertFirstRunActivityLaunched() {
        Intent launchedIntent = mShadowApplication.getNextStartedActivity();
        Assert.assertNotNull(launchedIntent);

        Assert.assertTrue(checkIntentComponentClass(launchedIntent, FirstRunActivity.class));
    }

    private <T extends Activity> Activity createActivity(Class<T> clazz, Intent intent) {
        ActivityController<T> activityController =
                Robolectric.buildActivity(clazz, intent).create();
        T activity = activityController.get();
        mActivityControllerList.add(activityController);
        return activity;
    }

    @Test
    public void testGenericViewIntentGoesToFirstRun() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("http://test.com"));
        intent.setPackage(mContext.getPackageName());
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Activity launcherActivity = createActivity(ChromeLauncherActivity.class, intent);
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

        Activity launcherActivity = createActivity(ChromeLauncherActivity.class, launchedIntent);
        assertFirstRunActivityLaunched();
        Assert.assertTrue(launcherActivity.isFinishing());
    }

    @Test
    public void testRedirectChromeTabbedActivityToFirstRun() {
        Intent intent = new Intent();
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Activity tabbedActivity = createActivity(ChromeTabbedActivity.class, intent);
        assertFirstRunActivityLaunched();
        Assert.assertTrue(tabbedActivity.isFinishing());
    }

    @Test
    public void testRedirectSearchActivityToFirstRun() {
        Intent intent = new Intent();
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        Activity searchActivity = createActivity(SearchActivity.class, intent);
        assertFirstRunActivityLaunched();
        Assert.assertTrue(searchActivity.isFinishing());
    }

    /**
     * Tests that when the first run experience is shown by a WebAPK that the WebAPK is launched
     * when the user finishes the first run experience. In the case where the WebAPK (as opposed
     * to WebappActivity) displays the splash screen this is necessary for correct behaviour when
     * the user taps the app icon and the WebAPK is still running.
     */
    @Test
    public void testFreRelaunchesWebApkNotWebApkActivity() {
        String webApkPackageName = "org.chromium.webapk.name";
        String startUrl = "https://pwa.rocks/";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, startUrl);
        WebApkTestHelper.registerWebApkWithMetaData(
                webApkPackageName, bundle, /* shareTargetMetaData= */ null);
        WebApkTestHelper.addIntentFilterForUrl(webApkPackageName, startUrl);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(webApkPackageName, startUrl);
        intent.putExtra(WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK, true);

        launchWebappLauncherActivityProcessRelaunch(intent);

        Intent launchedIntent = mShadowApplication.getNextStartedActivity();
        Assert.assertTrue(checkIntentComponentClass(launchedIntent, FirstRunActivity.class));
        PendingIntent freCompleteLaunchIntent =
                launchedIntent.getParcelableExtra(
                        FirstRunActivityBase.EXTRA_FRE_COMPLETE_LAUNCH_INTENT);
        Assert.assertNotNull(freCompleteLaunchIntent);
        Assert.assertEquals(
                webApkPackageName,
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
                webApkPackageName, bundle, /* shareTargetMetaData= */ null);
        WebApkTestHelper.addIntentFilterForUrl(webApkPackageName, startUrl);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(webApkPackageName, startUrl);

        launchWebappLauncherActivityProcessRelaunch(intent);

        Intent launchedIntent = mShadowApplication.getNextStartedActivity();
        Assert.assertTrue(checkIntentComponentClass(launchedIntent, WebappActivity.class));
        buildActivityWithClassNameFromIntent(launchedIntent);

        // No FRE should have been launched.
        Assert.assertNull(mShadowApplication.getNextStartedActivity());
    }

    /** Test that the lightweight first run experience is used for unbound WebAPKs. */
    @Test
    public void testLightweightFre() {
        String webApkPackageName = "unbound.webapk";
        String startUrl = "https://pwa.rocks/";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, startUrl);
        WebApkTestHelper.registerWebApkWithMetaData(
                webApkPackageName, bundle, /* shareTargetMetaData= */ null);
        WebApkTestHelper.addIntentFilterForUrl(webApkPackageName, startUrl);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(webApkPackageName, startUrl);

        launchWebappLauncherActivityProcessRelaunch(intent);

        Intent launchedIntent = mShadowApplication.getNextStartedActivity();
        Assert.assertTrue(
                checkIntentComponentClass(launchedIntent, LightweightFirstRunActivity.class));
    }

    /**
     * Test that {@link WebappLauncherActivity} shows the regular full first run experience when it
     * is launched with an intent which both:
     * - Has a WebAPK package extra which meets the lightweight first run activity requirements
     * - Refers to an invalid WebAPK
     */
    @Test
    public void testFullFreIfWebApkInvalid() {
        String webApkPackageName = "unbound.webapk";
        String startUrl = "https://pwa.rocks/";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, startUrl);
        WebApkTestHelper.registerWebApkWithMetaData(
                webApkPackageName, bundle, /* shareTargetMetaData= */ null);
        // Cause WebApkValidator#canWebApkHandleUrl() to fail (but not
        // WebApkIntentDataProviderFactory#create()) by not registering the intent handlers for the
        // WebAPK.

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(webApkPackageName, startUrl);
        Assert.assertNotNull(WebApkIntentDataProviderFactory.create(intent));

        launchWebappLauncherActivityProcessRelaunch(intent);

        Intent launchedIntent = mShadowApplication.getNextStartedActivity();
        Assert.assertTrue(checkIntentComponentClass(launchedIntent, FirstRunActivity.class));

        // WebappLauncherActivity (not the WebAPK) should be launched when the WebAPK completes.
        PendingIntent freCompleteLaunchIntent =
                launchedIntent.getParcelableExtra(
                        FirstRunActivityBase.EXTRA_FRE_COMPLETE_LAUNCH_INTENT);
        Assert.assertNotNull(freCompleteLaunchIntent);
        Assert.assertEquals(
                mContext.getPackageName(),
                Shadows.shadowOf(freCompleteLaunchIntent).getSavedIntent().getPackage());
    }
}
