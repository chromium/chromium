// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.CloseWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.SessionStartupPolicy;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Integration tests for {@link TabbedStartupWindowPolicyDelegate}. */
@DoNotBatch(reason = "This class tests creating and managing multiple windows on startup.")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(VERSION_CODES.S)
@EnableFeatures(ChromeFeatureList.SYNC_RESTORE_ON_STARTUP_PREF)
public class TabbedStartupWindowPolicyDelegateTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final List<String> STARTUP_URLS =
            List.of(UrlConstants.GOOGLE_URL, UrlConstants.CHROME_WEBSTORE_URL);

    private final Set<ChromeTabbedActivity> mExtraActivities = new HashSet<>();

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        DeviceInfo.setIsDesktopForTesting(true);
    }

    @After
    public void tearDown() {
        for (ChromeTabbedActivity activity : mExtraActivities) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        MultiInstanceManager manager = activity.getMultiInstanceMangerForTesting();
                        if (manager != null) {
                            manager.closeWindows(
                                    Collections.singletonList(activity.getWindowId()),
                                    CloseWindowAppSource.OTHER);
                        }
                    });
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ((MultiInstanceOrchestratorImpl) MultiInstanceOrchestratorImpl.getInstance())
                            .clearAssignmentsForTesting();
                    TabbedStartupWindowPolicyDelegate.getInstance().resetPolicy();
                    ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(
                            TabbedStartupWindowPolicyDelegate.PREF_UNSET);
                    ChromeMultiInstancePersistentStore.writeRestoreOnStartupUrls(
                            Collections.emptyList());
                    ChromeMultiInstancePersistentStore.clearSessionStartupPolicy();
                    ChromeMultiInstancePersistentStore.deleteInstanceState(2);
                });
    }

    @Test
    @SmallTest
    public void testNewWindow_RestoreOnStartup_NewTabPref() throws Exception {
        // Setup.
        setRestoreOnStartupPref(SessionStartupPref.NEW_TAB);

        // Act.
        ChromeTabbedActivity newActivity = createNewWindow(/* preferNew= */ true);

        // Verify.
        assertSingleNtpTab(newActivity);
    }

    @Test
    @SmallTest
    public void testNewWindow_RestoreOnStartup_UrlsPref_SuppressesUrls() throws Exception {
        // Setup.
        setRestoreOnStartupUrlsPref(STARTUP_URLS);

        // Act.
        ChromeTabbedActivity newActivity = createNewWindow(/* preferNew= */ true);

        // Verify: A fresh new window (preferNew = true, e.g. from launcher shortcut) suppresses
        // startup URLs and opens a single NTP, even when the URLS startup preference is configured
        // (b/555572612).
        assertSingleNtpTab(newActivity);
    }

    @Test
    @SmallTest
    public void testStandardStartup_RestoreOnStartup_UrlsPref_OpensUrls() throws Exception {
        // Setup.
        setRestoreOnStartupUrlsPref(STARTUP_URLS);

        // Act: Standard startup launch (preferNew = false) allocating an unmapped instance in
        // Tier 4.
        ChromeTabbedActivity newActivity = createNewWindow(/* preferNew= */ false);

        // Verify: Standard startup claims URLS and opens the configured URLs.
        assertTabUrls(newActivity, STARTUP_URLS);
    }

    @Test
    @SmallTest
    public void testStandardStartup_RestoreOnStartup_UrlsPref_WithUrlIntent() throws Exception {
        // Setup.
        setRestoreOnStartupUrlsPref(STARTUP_URLS);

        // Act: Launch via external URL intent (preferNew = false).
        ChromeTabbedActivity newActivity =
                launchExternalUrlIntent(Uri.parse(UrlConstants.GOOGLE_ACCOUNT_HOME_URL));

        // Verify.
        assertTabUrls(
                newActivity,
                List.of(
                        UrlConstants.GOOGLE_URL,
                        UrlConstants.CHROME_WEBSTORE_URL,
                        UrlConstants.GOOGLE_ACCOUNT_HOME_URL));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            UrlConstants.GOOGLE_ACCOUNT_HOME_URL,
                            newActivity.getActivityTab().getOriginalUrl().getSpec());
                });
    }

    @Test
    @SmallTest
    public void testNewWindow_RestoreAllPolicy_SuppressesWindowRestoration() throws Exception {
        // Setup: Simulate a prior session with an instance 2 marked recoverable and RESTORE_ALL
        // policy.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ((MultiInstanceOrchestratorImpl) MultiInstanceOrchestratorImpl.getInstance())
                            .clearAssignmentsForTesting();
                    TabbedStartupWindowPolicyDelegate.getInstance().resetPolicy();
                    ChromeMultiInstancePersistentStore.writeSessionStartupPolicy(
                            SessionStartupPolicy.RESTORE_ALL);
                    ChromeMultiInstancePersistentStore.writeLastAccessedTime(/* instanceId= */ 2);
                    ChromeMultiInstancePersistentStore.writeIsRecoverable(
                            /* instanceId= */ 2, true);
                });

        // Act: Launch a fresh new window (preferNew = true, e.g. from launcher shortcut).
        ChromeTabbedActivity newActivity = createNewWindow(/* preferNew= */ true);

        // Verify: Window restoration is suppressed (b/555556913). The startup policy is cleared,
        // but maybeRestoreWindowsAfterLaunch() is skipped so instance 2 is NOT restored (no
        // activity is launched for it).
        assertSingleNtpTab(newActivity);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            ChromeMultiInstancePersistentStore.readSessionStartupPolicy(),
                            is(SessionStartupPolicy.DEFAULT));
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (Activity activity : ApplicationStatus.getRunningActivities()) {
                        if (activity instanceof ChromeTabbedActivity tabbedActivity) {
                            assertNotEquals(2, tabbedActivity.getWindowId());
                        }
                    }
                });
    }

    private void setRestoreOnStartupPref(int pref) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabbedStartupWindowPolicyDelegate.getInstance().resetPolicy();
                    ChromeMultiInstancePersistentStore.writeRestoreOnStartupPrefValue(pref);
                });
    }

    private void setRestoreOnStartupUrlsPref(List<String> startupUrls) {
        setRestoreOnStartupPref(SessionStartupPref.URLS);
        ThreadUtils.runOnUiThreadBlocking(
                () -> ChromeMultiInstancePersistentStore.writeRestoreOnStartupUrls(startupUrls));
    }

    private ChromeTabbedActivity createNewWindow(boolean preferNew) {
        return createNewWindow(
                InstrumentationRegistry.getInstrumentation().getTargetContext(),
                /* instanceId= */ -1,
                preferNew,
                /* addIncognitoExtras= */ false);
    }

    private ChromeTabbedActivity createNewWindow(
            Context context, int instanceId, boolean preferNew, boolean addIncognitoExtras) {
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        context,
                        instanceId,
                        preferNew,
                        /* openAdjacently= */ false,
                        NewWindowAppSource.UNKNOWN);
        if (addIncognitoExtras) {
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_WINDOW, true);
        }
        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () -> ContextUtils.getApplicationContext().startActivity(intent));
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                "Activity tab should be non-null.",
                                activity.getActivityTab(),
                                notNullValue()));
        mExtraActivities.add(activity);
        return activity;
    }

    private ChromeTabbedActivity launchExternalUrlIntent(Uri uri) {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        context,
                        /* windowId= */ -1,
                        /* preferNew= */ false,
                        /* openAdjacently= */ false,
                        NewWindowAppSource.UNKNOWN);
        intent.setData(uri);
        intent.setAction(Intent.ACTION_VIEW);
        // Explicitly set the launch type to FROM_EXTERNAL_APP to simulate an intent from an
        // external application.
        IntentHandler.setTabLaunchType(intent, TabLaunchType.FROM_EXTERNAL_APP);
        ChromeTabbedActivity activity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () -> ContextUtils.getApplicationContext().startActivity(intent));
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                "Activity tab should be non-null.",
                                activity.getActivityTab(),
                                notNullValue()));
        mExtraActivities.add(activity);
        return activity;
    }

    private void assertSingleNtpTab(ChromeTabbedActivity activity) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(activity.getTabModelSelector().getTotalTabCount(), is(1));
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel model = activity.getTabModelSelector().getModel(false);
                    assertTrue(UrlUtilities.isNtpUrl(model.getTabAt(0).getOriginalUrl()));
                });
    }

    private void assertTabUrls(ChromeTabbedActivity activity, List<String> expectedUrls) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getTabModelSelector().getTotalTabCount(),
                            is(expectedUrls.size()));
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel model = activity.getTabModelSelector().getModel(false);
                    for (int i = 0; i < expectedUrls.size(); i++) {
                        assertEquals(
                                expectedUrls.get(i), model.getTabAt(i).getOriginalUrl().getSpec());
                    }
                });
    }
}
