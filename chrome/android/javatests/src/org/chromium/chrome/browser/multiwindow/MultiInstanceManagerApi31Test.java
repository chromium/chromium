// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.os.Build.VERSION_CODES;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.RecentlyClosedEntriesManager;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.CloseWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.ntp.RecentlyClosedEntry;
import org.chromium.chrome.browser.ntp.RecentlyClosedWindow;
import org.chromium.chrome.browser.preferences.MultiInstancePreferenceKeys;
import org.chromium.chrome.browser.preferences.MultiInstanceSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

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

    private ModalDialogManager mModalDialogManager;
    private MultiInstanceManagerApi31 mMultiInstanceManager;
    private final Set<ChromeTabbedActivity> mExtraActivities = new HashSet<>();

    @Before
    public void setup() throws InterruptedException {
        mActivityTestRule.startOnBlankPage();
        mModalDialogManager = mActivityTestRule.getActivity().getModalDialogManager();
        mMultiInstanceManager =
                (MultiInstanceManagerApi31)
                        mActivityTestRule.getActivity().getMultiInstanceMangerForTesting();
    }

    @After
    public void teardown() throws InterruptedException {
        MultiInstanceSharedPreferences.getInstance()
                .removeKey(
                        MultiInstancePreferenceKeys
                                .MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED);
        for (ChromeTabbedActivity activity : mExtraActivities) {
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            mMultiInstanceManager.closeWindows(
                                    Collections.singletonList(activity.getWindowId()),
                                    CloseWindowAppSource.OTHER));
        }
    }

    @Test
    @SmallTest
    public void testTabModelSelectorObserverOnTabStateInitialized() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();

        // Get the original value of |mCreatedTabOnStartup|.
        boolean createdTabOnStartup = activity.getCreatedTabOnStartupForTesting();

        // Reset the values of |mCreatedTabOnStartup| and |MultiInstanceManager.mTabModelObserver|.
        // This tab model selector observer should be registered in MultiInstanceManager on tab
        // state initialization irrespective of the value of |mCreatedTabOnStartup|.
        activity.setCreatedTabOnStartupForTesting(false);
        activity.getMultiInstanceMangerForTesting().setTabModelObserverForTesting(null);

        var tabModelSelectorObserver = activity.getTabModelSelectorObserverForTesting();
        ThreadUtils.runOnUiThreadBlocking(tabModelSelectorObserver::onTabStateInitialized);
        Assert.assertEquals(
                "Regular tab count should be written to persistent store after tab state"
                        + " initialization.",
                1,
                ChromeMultiInstancePersistentStore.readNormalTabCount(activity.getWindowId()));
        Assert.assertEquals(
                "Incognito tab count should be written to persistent store after tab state"
                        + " initialization.",
                0,
                ChromeMultiInstancePersistentStore.readIncognitoTabCount(activity.getWindowId()));

        // Restore the original value of |mCreatedTabOnStartup|.
        activity.setCreatedTabOnStartupForTesting(createdTabOnStartup);
    }

    // Initial state: max limit = 4, active tasks = 4, inactive tasks = 0.
    // Final state: max limit = 2, active tasks = 2, inactive tasks = 2.
    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511289033
    public void decreaseInstanceLimit_ExcessActive_ExcessTasksFinished() {
        // Set initial instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(4);

        ChromeTabbedActivity firstActivity = mActivityTestRule.getActivity();
        ChromeTabbedActivity[] otherActivities =
                createNewWindows(
                        firstActivity, /* numWindows= */ 3, /* addIncognitoExtras= */ false);
        ThreadUtils.runOnUiThreadBlocking(() -> firstActivity.onTopResumedActivityChanged(true));

        // Check initial state of instances.
        verifyInstanceState(/* expectedActiveInstances= */ 4, /* expectedTotalInstances= */ 4);

        // Decrease instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(2);

        // Simulate restoration of an existing instance after a decrease in instance limit that
        // should trigger instance limit downgrade actions.
        otherActivities[2].finishAndRemoveTask();
        CriteriaHelper.pollUiThread(
                () ->
                        ApplicationStatus.getStateForActivity(otherActivities[2])
                                == ActivityState.DESTROYED,
                "Activity not destroyed");
        var newActivity =
                createNewWindow(
                        firstActivity,
                        otherActivities[2].getWindowId(),
                        /* addIncognitoExtras= */ false);
        mActivityTestRule.getActivityTestRule().setActivity(newActivity);
        mActivityTestRule.waitForActivityCompletelyLoaded();

        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 4);
    }

    // Initial state: max limit = 3, active tasks = 2, inactive tasks = 1.
    // Final state: max limit = 2, active tasks = 2, inactive tasks = 1.
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/482145010: Flaky on test-tablet & automotive.")
    public void decreaseInstanceLimit_MaxActive_NoTasksFinished() {
        // Set initial instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(3);

        ChromeTabbedActivity firstActivity = mActivityTestRule.getActivity();
        ChromeTabbedActivity[] otherActivities =
                createNewWindows(
                        firstActivity, /* numWindows= */ 2, /* addIncognitoExtras= */ false);
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
                createNewWindow(
                        otherActivities[0],
                        otherActivities[0].getWindowId(),
                        /* addIncognitoExtras= */ false);

        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 3);
    }

    @Test
    @MediumTest
    public void closeWindowFromWindowManager_softClosure() {
        // Set initial instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(5);

        ChromeTabbedActivity firstActivity = mActivityTestRule.getActivity();
        ChromeTabbedActivity[] otherActivities =
                createNewWindows(
                        firstActivity, /* numWindows= */ 2, /* addIncognitoExtras= */ false);

        // Check initial state of instances.
        verifyInstanceState(/* expectedActiveInstances= */ 3, /* expectedTotalInstances= */ 3);

        // Close one window.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMultiInstanceManager.closeWindows(
                                Collections.singletonList(
                                        otherActivities[otherActivities.length - 1].getWindowId()),
                                CloseWindowAppSource.WINDOW_MANAGER));

        // Check state of instances after one instance is closed - the closed window should become
        // an inactive one.
        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 3);
    }

    @Test
    @MediumTest
    public void closeWindowFromWindowManager_RecentlyClosedEntriesUpdated() {
        // Set initial instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(5);

        ChromeTabbedActivity firstActivity = mActivityTestRule.getActivity();
        ChromeTabbedActivity[] otherActivities =
                createNewWindows(
                        firstActivity, /* numWindows= */ 2, /* addIncognitoExtras= */ false);

        // Check initial state of instances.
        verifyInstanceState(/* expectedActiveInstances= */ 3, /* expectedTotalInstances= */ 3);

        // Verify there is 0 entry in the RecentlyClosedEntriesManager.
        RecentlyClosedEntriesManager recentlyClosedEntriesManager =
                firstActivity.getRecentlyClosedEntriesManagerForTesting();
        assertEquals(0, recentlyClosedEntriesManager.getRecentlyClosedEntries().size());

        // Close one window.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMultiInstanceManager.closeWindows(
                                Collections.singletonList(
                                        otherActivities[otherActivities.length - 1].getWindowId()),
                                CloseWindowAppSource.WINDOW_MANAGER));

        // Check state of instances after one instance is closed - the closed window should become
        // an inactive one.
        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 3);

        // Verify there is 1 window entry in the RecentlyClosedEntriesManager after the window
        // closure.
        List<RecentlyClosedEntry> entries = recentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals("There should be 1 recently closed entry", 1, entries.size());
        assertTrue(
                "The recently closed entry should be RecentlyClosedWindow type",
                entries.get(0) instanceof RecentlyClosedWindow);

        // Close another window.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMultiInstanceManager.closeWindows(
                                Collections.singletonList(
                                        otherActivities[otherActivities.length - 2].getWindowId()),
                                CloseWindowAppSource.WINDOW_MANAGER));

        // Check state of instances after the second instance is closed - the closed window should
        // become an inactive one.
        verifyInstanceState(/* expectedActiveInstances= */ 1, /* expectedTotalInstances= */ 3);

        // Verify there are 2 window entries in the RecentlyClosedEntriesManager after the second
        // window closure.
        entries = recentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals("There should be 2 recently closed entry", 2, entries.size());
        assertTrue(
                "The recently closed entry should be RecentlyClosedWindow type",
                entries.get(0) instanceof RecentlyClosedWindow);
    }

    @Test
    @MediumTest
    public void
            closeWindowFromWindowManager_RecentlyClosedEntriesNotUpdated_WindowContainsOnlyOneNtp() {
        // Set initial instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(5);

        ChromeTabbedActivity firstActivity = mActivityTestRule.getActivity();
        ChromeTabbedActivity otherActivity =
                createNewWindow(
                        firstActivity,
                        /* instanceId= */ 1,
                        /* addIncognitoExtras= */ false,
                        /* loadCustomUrl= */ false,
                        /* createMultipleTabs= */ false);

        // Check initial state of instances.
        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 2);

        // Verify there is 0 entry in the RecentlyClosedEntriesManager.
        RecentlyClosedEntriesManager recentlyClosedEntriesManager =
                firstActivity.getRecentlyClosedEntriesManagerForTesting();
        assertEquals(0, recentlyClosedEntriesManager.getRecentlyClosedEntries().size());

        // Close the window that contains only 1 NTP.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMultiInstanceManager.closeWindows(
                                Collections.singletonList(otherActivity.getWindowId()),
                                CloseWindowAppSource.WINDOW_MANAGER));

        // Check state of instances after one instance is closed - the closed window should be
        // permanently deleted.
        verifyInstanceState(/* expectedActiveInstances= */ 1, /* expectedTotalInstances= */ 1);

        // Verify there is 0 window entry in the RecentlyClosedEntriesManager after the window
        // closure.
        List<RecentlyClosedEntry> entries = recentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals("There should be 0 recently closed entry", 0, entries.size());
    }

    @Test
    @MediumTest
    public void restoreWindow_RecentlyClosedEntriesUpdated() {
        // Set initial instance limit.
        MultiWindowUtils.setMaxInstancesForTesting(5);

        ChromeTabbedActivity firstActivity = mActivityTestRule.getActivity();
        ChromeTabbedActivity[] otherActivities =
                createNewWindows(
                        firstActivity, /* numWindows= */ 2, /* addIncognitoExtras= */ false);

        // Check initial state of instances.
        verifyInstanceState(/* expectedActiveInstances= */ 3, /* expectedTotalInstances= */ 3);

        // Verify there is 0 entry in the RecentlyClosedEntriesManager.
        RecentlyClosedEntriesManager recentlyClosedEntriesManager =
                firstActivity.getRecentlyClosedEntriesManagerForTesting();
        assertEquals(0, recentlyClosedEntriesManager.getRecentlyClosedEntries().size());

        // Close one window.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMultiInstanceManager.closeWindows(
                                Collections.singletonList(
                                        otherActivities[otherActivities.length - 1].getWindowId()),
                                CloseWindowAppSource.WINDOW_MANAGER));

        // Check state of instances after one instance is closed - the closed window should become
        // an inactive one.
        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 3);

        // Verify there is 1 window entry in the RecentlyClosedEntriesManager after the window
        // closure.
        List<RecentlyClosedEntry> entries = recentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals("There should be 1 recently closed entry", 1, entries.size());
        assertTrue(
                "The recently closed entry should be RecentlyClosedWindow type",
                entries.get(0) instanceof RecentlyClosedWindow);
        RecentlyClosedWindow window = (RecentlyClosedWindow) entries.get(0);

        // Restore window.
        ThreadUtils.runOnUiThreadBlocking(
                () -> recentlyClosedEntriesManager.openRecentlyClosedEntry(window));

        // Verify window is restored and removed from recentlyClosedEntries.
        entries = recentlyClosedEntriesManager.getRecentlyClosedEntries();
        assertEquals("There should be 0 recently closed entry", 0, entries.size());
        verifyInstanceState(/* expectedActiveInstances= */ 3, /* expectedTotalInstances= */ 3);
    }

    private ChromeTabbedActivity[] createNewWindows(
            Context context, int numWindows, boolean addIncognitoExtras) {
        ChromeTabbedActivity[] activities = new ChromeTabbedActivity[numWindows];
        for (int i = 0; i < numWindows; i++) {
            activities[i] = createNewWindow(context, /* instanceId= */ -1, addIncognitoExtras);
        }
        return activities;
    }

    private ChromeTabbedActivity createNewWindow(
            Context context,
            int instanceId,
            boolean addIncognitoExtras,
            boolean loadCustomUrl,
            boolean createMultipleTabs) {
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        context,
                        instanceId,
                        /* preferNew= */ true,
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
        Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> activity.getActivityTab());
        if (loadCustomUrl) {
            ChromeTabUtils.loadUrlOnUiThread(tab, UrlConstants.GOOGLE_URL);
        }
        if (createMultipleTabs && !addIncognitoExtras) {
            ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), activity);
        }
        mExtraActivities.add(activity);
        return activity;
    }

    private ChromeTabbedActivity createNewWindow(
            Context context, int instanceId, boolean addIncognitoExtras) {
        return createNewWindow(
                context,
                instanceId,
                addIncognitoExtras,
                /* loadCustomUrl= */ true,
                /* createMultipleTabs= */ true);
    }

    private void verifyInstanceState(int expectedActiveInstances, int expectedTotalInstances) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Active instance count is incorrect.",
                            MultiWindowUtils.getPersistedInstanceIds(PersistedInstanceType.ACTIVE)
                                    .size(),
                            is(expectedActiveInstances));
                    Criteria.checkThat(
                            "Persisted instance count is incorrect.",
                            MultiWindowUtils.getPersistedInstanceIds(PersistedInstanceType.ANY)
                                    .size(),
                            is(expectedTotalInstances));
                });
    }
}
