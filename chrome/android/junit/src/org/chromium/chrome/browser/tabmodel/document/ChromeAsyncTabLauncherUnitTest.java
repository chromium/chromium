// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel.document;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Intent;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentRecordingActivity;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ChromeAsyncTabLauncher}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeAsyncTabLauncherUnitTest {
    private IntentRecordingActivity mActivity;

    @Before
    public void setUp() {
        AsyncTabParamsManagerSingleton.getInstance().getAsyncTabParams().clear();
        mActivity = Robolectric.buildActivity(IntentRecordingActivity.class).setup().get();
    }

    @After
    public void tearDown() {
        AsyncTabParamsManagerSingleton.getInstance().getAsyncTabParams().clear();
    }

    @Test
    public void testLaunchNewTab_withActivity_startsIntentFromActivity() {
        ChromeAsyncTabLauncher launcher =
                new ChromeAsyncTabLauncher(/* incognito= */ false, mActivity);

        launcher.launchNewTab(
                new LoadUrlParams(JUnitTestGURLs.URL_1.getSpec()),
                TabLaunchType.FROM_CHROME_UI,
                /* parent= */ null);

        Intent intent = getActivityStartedIntent();
        assertEquals(ChromeLauncherActivity.class.getName(), intent.getComponent().getClassName());
        assertEquals(JUnitTestGURLs.URL_1.getSpec(), intent.getDataString());
        assertTrue((intent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0);
    }

    @Test
    public void testLaunchUrl_withActivity_startsIntentFromActivity() {
        ChromeAsyncTabLauncher launcher =
                new ChromeAsyncTabLauncher(/* incognito= */ true, mActivity);

        launcher.launchUrl(JUnitTestGURLs.URL_1.getSpec(), TabLaunchType.FROM_CHROME_UI);

        Intent intent = getActivityStartedIntent();
        assertTrue(intent.getBooleanExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false));
    }

    @Test
    public void testLaunchNewStandaloneFrame_withActivity_startsIntentFromActivity() {
        ChromeAsyncTabLauncher launcher =
                new ChromeAsyncTabLauncher(/* incognito= */ false, mActivity);
        Intent standaloneIntent = new Intent(Intent.ACTION_VIEW);
        standaloneIntent.setPackage(RuntimeEnvironment.getApplication().getPackageName());

        launcher.launchNewStandaloneFrame(standaloneIntent);

        Intent intent = getActivityStartedIntent();
        int expectedFlags = Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
        assertEquals(expectedFlags, intent.getFlags() & expectedFlags);
    }

    @Test
    public void testLaunchUrl_finishingActivity_startsIntentFromActivity() {
        mActivity.finish();
        assertTrue(mActivity.isFinishing());
        ChromeAsyncTabLauncher launcher =
                new ChromeAsyncTabLauncher(/* incognito= */ false, mActivity);

        launcher.launchUrl(JUnitTestGURLs.URL_1.getSpec(), TabLaunchType.FROM_CHROME_UI);

        getActivityStartedIntent();
    }

    @Test
    public void testLaunchUrl_nullActivity_startsIntentFromApplicationContext() {
        ChromeAsyncTabLauncher launcher =
                new ChromeAsyncTabLauncher(/* incognito= */ false, /* activity= */ null);

        launcher.launchUrl(JUnitTestGURLs.URL_1.getSpec(), TabLaunchType.FROM_CHROME_UI);

        assertTrue(mActivity.getStartedIntents().isEmpty());
        assertApplicationContextStartedLauncherIntent();
    }

    @Test
    public void testLaunchUrl_clearedWeakReference_startsIntentFromApplicationContext() {
        ChromeAsyncTabLauncher launcher =
                new ChromeAsyncTabLauncher(/* incognito= */ false, mActivity);
        launcher.getActivityRefForTesting().clear();

        launcher.launchUrl(JUnitTestGURLs.URL_1.getSpec(), TabLaunchType.FROM_CHROME_UI);

        assertTrue(mActivity.getStartedIntents().isEmpty());
        assertApplicationContextStartedLauncherIntent();
    }

    private Intent getActivityStartedIntent() {
        assertEquals(1, mActivity.getStartedIntents().size());
        return mActivity.getStartedIntents().get(0);
    }

    private void assertApplicationContextStartedLauncherIntent() {
        Intent intent =
                Shadows.shadowOf(RuntimeEnvironment.getApplication()).getNextStartedActivity();
        assertNotNull(intent);
        assertEquals(ChromeLauncherActivity.class.getName(), intent.getComponent().getClassName());
    }
}
