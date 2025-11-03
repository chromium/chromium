// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.CloseWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
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
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/** Integration tests for {@link MultiInstanceManagerApi31}. */
@DoNotBatch(reason = "This class tests creating, destroying and managing multiple windows.")
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@MinAndroidSdkLevel(VERSION_CODES.S)
public class MultiInstanceManagerApi31Test {
    private static final int TAB1_ID = 456;
    private static final Token TAB_GROUP_ID1 = new Token(2L, 2L);
    private static final String TAB_GROUP_TITLE = "Regrouped tabs";
    private static final ArrayList<Map.Entry<Integer, String>> TAB_IDS_TO_URLS =
            new ArrayList<>(List.of(Map.entry(TAB1_ID, "https://www.youtube.com/")));

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private WebPageStation mPage;
    private ModalDialogManager mModalDialogManager;
    private MultiInstanceManagerApi31 mMultiInstanceManager;

    @Before
    public void setup() throws InterruptedException {
        mPage = mActivityTestRule.startOnBlankPage();
        mModalDialogManager = mActivityTestRule.getActivity().getModalDialogManager();
        mMultiInstanceManager =
                (MultiInstanceManagerApi31)
                        mActivityTestRule.getActivity().getMultiInstanceMangerForTesting();
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
                            newActivity.getWindowIdForTesting(), CloseWindowAppSource.OTHER);
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
        otherActivities[1].finishAndRemoveTask();

        // Check state of instances after one instance is made inactive.
        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 3);

        // Decrease instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(2);

        // Simulate relaunch of an active instance after the instance limit downgrade.
        otherActivities[0].finishAndRemoveTask();
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
                            otherActivities[0].getWindowIdForTesting(), CloseWindowAppSource.OTHER);
                    multiInstanceManager.closeInstance(
                            newActivity.getWindowIdForTesting(), CloseWindowAppSource.OTHER);
                });
    }

    @Test
    @SmallTest
    public void moveTabsToOtherWindow_multipleWindowsOpen() {
        ChromeTabbedActivity newWindow =
                createNewWindow(mActivityTestRule.getActivity(), /* instanceId= */ 2);
        List<Tab> tabs = new ArrayList<>();
        var activity = mActivityTestRule.getActivity();
        var activeTab = ThreadUtils.runOnUiThreadBlocking(activity::getActivityTab);
        tabs.add(activeTab);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.moveTabsToOtherWindow(
                            tabs, MultiInstanceManager.NewWindowAppSource.MENU);
                });
        assertTrue("Target selector dialog should be visible", mModalDialogManager.isShowing());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.closeInstance(
                            newWindow.getWindowIdForTesting(), newWindow.getTaskId());
                });
    }

    @Test
    @SmallTest
    public void moveTabsToOtherWindow_singleWindowOpen() {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        List<Tab> tabs = new ArrayList<>();
        var activity = mActivityTestRule.getActivity();
        var activeTab = ThreadUtils.runOnUiThreadBlocking(activity::getActivityTab);
        tabs.add(activeTab);

        verifyInstanceState(/* expectedActiveInstances= */ 1, /* expectedTotalInstances= */ 1);
        ChromeTabbedActivity newWindow =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () ->
                                mMultiInstanceManager.moveTabsToOtherWindow(
                                        tabs,
                                        MultiInstanceManager.NewWindowAppSource.WINDOW_MANAGER));
        assertFalse(
                "Target selector dialog should not be visible with only one window open",
                mModalDialogManager.isShowing());
        // A new instance should be created because of the new window opened.
        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 2);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.closeInstance(
                            newWindow.getWindowIdForTesting(), newWindow.getTaskId());
                });
    }

    @Test
    @SmallTest
    public void moveTabGroupToOtherWindow_singleWindowOpen() {
        verifyInstanceState(/* expectedActiveInstances= */ 1, /* expectedTotalInstances= */ 1);
        TabGroupMetadata tabGroupMetadata = getTabGroupMetaData();

        ChromeTabbedActivity newWindow =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () ->
                                mMultiInstanceManager.moveTabGroupToOtherWindow(
                                        tabGroupMetadata,
                                        MultiInstanceManager.NewWindowAppSource.WINDOW_MANAGER));
        assertFalse(
                "Target selector dialog should not be visible with only one window open",
                mModalDialogManager.isShowing());

        // A new instance should be created because of the new window opened.
        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 2);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.closeInstance(
                            newWindow.getWindowIdForTesting(), newWindow.getTaskId());
                });
    }

    @Test
    @SmallTest
    public void moveTabGroupToOtherWindow_multipleWindowsOpen() {
        ChromeTabbedActivity newWindow =
                createNewWindow(mActivityTestRule.getActivity(), /* instanceId= */ 2);
        TabGroupMetadata tabGroupMetadata = getTabGroupMetaData();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.moveTabGroupToOtherWindow(
                            tabGroupMetadata,
                            MultiInstanceManager.NewWindowAppSource.WINDOW_MANAGER);
                });
        assertTrue("Target selector dialog should be visible", mModalDialogManager.isShowing());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.closeInstance(
                            newWindow.getWindowIdForTesting(), newWindow.getTaskId());
                });
    }

    @Test
    @SmallTest
    public void openUrlInSelectedWindow_multipleWindowsOpen() {
        ChromeTabbedActivity newWindow =
                createNewWindow(mActivityTestRule.getActivity(), /* instanceId= */ 2);
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.openUrlInSelectedWindow(urlParams, /* parentTabId= */ 1);
                });
        assertTrue("Target selector dialog should be visible", mModalDialogManager.isShowing());
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.closeInstance(
                            newWindow.getWindowIdForTesting(), newWindow.getTaskId());
                });
    }

    @Test
    @SmallTest
    public void openUrlInSelectedWindow_singleWindowOpen() {
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));
        verifyInstanceState(/* expectedActiveInstances= */ 1, /* expectedTotalInstances= */ 1);

        ChromeTabbedActivity newWindow =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () ->
                                mMultiInstanceManager.openUrlInSelectedWindow(
                                        urlParams, /* parentTabId= */ 1));
        assertFalse(
                "Target selector dialog should not be visible", mModalDialogManager.isShowing());

        // A new instance should be created because of the new window opened.
        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 2);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.closeInstance(
                            newWindow.getWindowIdForTesting(), newWindow.getTaskId());
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
        Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> activity.getActivityTab());
        ChromeTabUtils.loadUrlOnUiThread(tab, UrlConstants.GOOGLE_URL);
        return activity;
    }

    private void verifyInstanceState(int expectedActiveInstances, int expectedTotalInstances) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Active instance count is incorrect.",
                            MultiInstanceManagerApi31.getPersistedInstanceIds(
                                            PersistedInstanceType.ACTIVE)
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

    private TabGroupMetadata getTabGroupMetaData() {
        return new TabGroupMetadata(
                /* selectedTabId= */ TAB1_ID,
                /* sourceWindowId= */ 1,
                TAB_GROUP_ID1,
                TAB_IDS_TO_URLS,
                /* tabGroupColor= */ 0,
                TAB_GROUP_TITLE,
                /* mhtmlTabTitle= */ null,
                /* tabGroupCollapsed= */ true,
                /* isGroupShared= */ false,
                /* isIncognito= */ false);
    }
}
