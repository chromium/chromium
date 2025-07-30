// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.content.Intent;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;

import java.util.List;

/** Integration tests for {@link MultiInstanceManagerApi31}. */
@DoNotBatch(reason = "This class tests creating, destroying and managing multiple windows.")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(VERSION_CODES.S)
public class MultiInstanceManagerApi31Test {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private WebPageStation mPage;

    @Before
    public void setup() throws InterruptedException {
        mPage = mActivityTestRule.startOnBlankPage();
    }

    @After
    public void teardown() throws InterruptedException {
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.MULTI_INSTANCE_RESTORATION_MESSAGE_SHOWN);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED);
    }

    // Initial state: max limit = 4, active tasks = 4, inactive tasks = 0.
    // Final state: max limit = 2, active tasks = 2, inactive tasks = 2.
    @Test
    @MediumTest
    public void decreaseInstanceLimit_ExcessActive_ExcessTasksFinished() {
        // Set initial instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(4);

        ChromeTabbedActivity firstActivity = mActivityTestRule.getActivity();
        ChromeTabbedActivity[] otherActivities =
                createNewWindows(firstActivity, /* numWindows= */ 3);
        ThreadUtils.runOnUiThreadBlocking(() -> firstActivity.onTopResumedActivityChanged(true));

        // Check initial state of instances.
        verifyInstanceState(/* expectedActiveInstances= */ 4, /* expectedTotalInstances= */ 4);

        // Decrease instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(2);

        // Simulate restoration of an existing instance after a decrease in instance limit that
        // should trigger instance limit downgrade actions.
        otherActivities[2].finishAndRemoveTask();
        var newActivity =
                createNewWindow(firstActivity, otherActivities[2].getWindowIdForTesting());
        mActivityTestRule.getActivityTestRule().setActivity(newActivity);
        mActivityTestRule.waitForActivityCompletelyLoaded();

        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 4);
        waitForInstanceRestorationMessage();

        // Cleanup activities.
        mActivityTestRule.getActivityTestRule().setActivity(firstActivity);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var multiInstanceManager =
                            (MultiInstanceManagerApi31)
                                    mActivityTestRule
                                            .getActivity()
                                            .getMultiInstanceMangerForTesting();
                    multiInstanceManager.closeInstance(
                            otherActivities[0].getWindowIdForTesting(),
                            otherActivities[0].getTaskId());
                    multiInstanceManager.closeInstance(
                            otherActivities[1].getWindowIdForTesting(),
                            otherActivities[1].getTaskId());
                    multiInstanceManager.closeInstance(
                            newActivity.getWindowIdForTesting(), newActivity.getTaskId());
                });
    }

    // Initial state: max limit = 3, active tasks = 2, inactive tasks = 1.
    // Final state: max limit = 2, active tasks = 2, inactive tasks = 1.
    @Test
    @MediumTest
    public void decreaseInstanceLimit_MaxActive_NoTasksFinished() {
        // Set initial instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(3);

        ChromeTabbedActivity firstActivity = mActivityTestRule.getActivity();
        ChromeTabbedActivity[] otherActivities =
                createNewWindows(firstActivity, /* numWindows= */ 2);
        ThreadUtils.runOnUiThreadBlocking(() -> firstActivity.onTopResumedActivityChanged(true));

        // Check initial state of instances.
        verifyInstanceState(/* expectedActiveInstances= */ 3, /* expectedTotalInstances= */ 3);

        // Make an instance inactive by killing its task.
        List<AppTask> appTasks =
                ((ActivityManager) firstActivity.getSystemService(Context.ACTIVITY_SERVICE))
                        .getAppTasks();
        for (AppTask appTask : appTasks) {
            if (appTask.getTaskInfo().taskId == otherActivities[1].getTaskId()) {
                appTask.finishAndRemoveTask();
                break;
            }
        }

        // Check state of instances after one instance is made inactive.
        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 3);

        // Decrease instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(2);

        // Simulate relaunch of an active instance after the instance limit downgrade.
        ApplicationTestUtils.waitForActivityWithClass(
                ChromeTabbedActivity.class, Stage.DESTROYED, otherActivities[0]::finish);
        var newActivity =
                createNewWindow(otherActivities[0], otherActivities[0].getWindowIdForTesting());
        mActivityTestRule.getActivityTestRule().setActivity(newActivity);

        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 3);
        waitForInstanceRestorationMessage();

        // Cleanup activities.
        mActivityTestRule.getActivityTestRule().setActivity(firstActivity);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    var multiInstanceManager =
                            (MultiInstanceManagerApi31)
                                    mActivityTestRule
                                            .getActivity()
                                            .getMultiInstanceMangerForTesting();
                    multiInstanceManager.closeInstance(
                            otherActivities[0].getWindowIdForTesting(),
                            otherActivities[0].getTaskId());
                    multiInstanceManager.closeInstance(
                            newActivity.getWindowIdForTesting(), newActivity.getTaskId());
                });
    }

    private ChromeTabbedActivity[] createNewWindows(Context context, int numWindows) {
        ChromeTabbedActivity[] activities = new ChromeTabbedActivity[numWindows];
        for (int i = 0; i < numWindows; i++) {
            activities[i] = createNewWindow(context, /* instanceId= */ -1);
        }
        return activities;
    }

    private ChromeTabbedActivity createNewWindow(Context context, int instanceId) {
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        context,
                        instanceId,
                        /* preferNew= */ true,
                        /* openAdjacently= */ false,
                        /* addTrustedIntentExtras= */ true);
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
        ChromeTabUtils.loadUrlOnUiThread(activity.getActivityTab(), UrlConstants.GOOGLE_URL);
        return activity;
    }

    private void verifyInstanceState(int expectedActiveInstances, int expectedTotalInstances) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Active instance count is incorrect.",
                            MultiInstanceManagerApi31.getPersistedInstanceIds(
                                            MultiInstanceManager.PersistedInstanceType.ACTIVE)
                                    .size(),
                            is(expectedActiveInstances));
                    Criteria.checkThat(
                            "Persisted instance count is incorrect.",
                            MultiInstanceManagerApi31.getAllPersistedInstanceIds().size(),
                            is(expectedTotalInstances));
                });
    }

    private void waitForInstanceRestorationMessage() {
        CriteriaHelper.pollUiThread(
                () -> {
                    MessageDispatcher messageDispatcher =
                            ThreadUtils.runOnUiThreadBlocking(
                                    () ->
                                            MessageDispatcherProvider.from(
                                                    mActivityTestRule
                                                            .getActivity()
                                                            .getWindowAndroid()));
                    List<MessageStateHandler> messages =
                            MessagesTestHelper.getEnqueuedMessages(
                                    messageDispatcher,
                                    MessageIdentifier
                                            .MULTI_INSTANCE_RESTORATION_ON_DOWNGRADED_LIMIT);
                    return !messages.isEmpty();
                });
    }
}
