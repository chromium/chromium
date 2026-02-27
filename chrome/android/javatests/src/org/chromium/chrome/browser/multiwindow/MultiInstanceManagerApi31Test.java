// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.notNullValue;
import static org.junit.Assert.assertEquals;
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
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.RecentlyClosedEntriesManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.CloseWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.ntp.RecentlyClosedEntry;
import org.chromium.chrome.browser.ntp.RecentlyClosedWindow;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

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
@EnableFeatures(ChromeFeatureList.RECENTLY_CLOSED_TABS_AND_WINDOWS)
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
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.MULTI_INSTANCE_RESTORATION_MESSAGE_SHOWN);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.MULTI_INSTANCE_INSTANCE_LIMIT_DOWNGRADE_TRIGGERED);
        for (ChromeTabbedActivity activity : mExtraActivities) {
            ThreadUtils.runOnUiThreadBlocking(
                    () ->
                            mMultiInstanceManager.closeWindows(
                                    Collections.singletonList(activity.getWindowIdForTesting()),
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
                MultiInstancePersistentStore.readNormalTabCount(activity.getWindowIdForTesting()));
        Assert.assertEquals(
                "Incognito tab count should be written to persistent store after tab state"
                        + " initialization.",
                0,
                MultiInstancePersistentStore.readIncognitoTabCount(
                        activity.getWindowIdForTesting()));

        // Restore the original value of |mCreatedTabOnStartup|.
        activity.setCreatedTabOnStartupForTesting(createdTabOnStartup);
    }

    // Initial state: max limit = 4, active tasks = 4, inactive tasks = 0.
    // Final state: max limit = 2, active tasks = 2, inactive tasks = 2.
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
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
                        otherActivities[2].getWindowIdForTesting(),
                        /* addIncognitoExtras= */ false);
        mActivityTestRule.getActivityTestRule().setActivity(newActivity);
        mActivityTestRule.waitForActivityCompletelyLoaded();

        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 4);
        waitForMessage(
                newActivity, MessageIdentifier.MULTI_INSTANCE_RESTORATION_ON_DOWNGRADED_LIMIT);
    }

    // Initial state: max limit = 3, active tasks = 2, inactive tasks = 1.
    // Final state: max limit = 2, active tasks = 2, inactive tasks = 1.
    @Test
    @MediumTest
    @DisabledTest(message="crbug.com/482145010: Flaky on test-tablet & automotive.")
    @DisableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
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
                        otherActivities[0].getWindowIdForTesting(),
                        /* addIncognitoExtras= */ false);

        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 3);
        waitForMessage(
                newActivity, MessageIdentifier.MULTI_INSTANCE_RESTORATION_ON_DOWNGRADED_LIMIT);
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
                                        otherActivities[otherActivities.length - 1]
                                                .getWindowIdForTesting()),
                                CloseWindowAppSource.WINDOW_MANAGER));

        // Check state of instances after one instance is closed - the closed window should become
        // an inactive one.
        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 3);
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.RECENTLY_CLOSED_TABS_AND_WINDOWS)
    public void closeWindowFromWindowManager_hardClosure() {
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
                                        otherActivities[otherActivities.length - 1]
                                                .getWindowIdForTesting()),
                                CloseWindowAppSource.WINDOW_MANAGER));

        // Check state of instances after one instance is closed - the window should be fully
        // closed.
        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 2);
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
                                        otherActivities[otherActivities.length - 1]
                                                .getWindowIdForTesting()),
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
                                        otherActivities[otherActivities.length - 2]
                                                .getWindowIdForTesting()),
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
                                Collections.singletonList(otherActivity.getWindowIdForTesting()),
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
    @EnableFeatures(ChromeFeatureList.RECENTLY_CLOSED_TABS_AND_WINDOWS)
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
                                        otherActivities[otherActivities.length - 1]
                                                .getWindowIdForTesting()),
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

    @Test
    @SmallTest
    public void moveTabsToOtherWindow_multipleWindowsOpen() {
        var activity = mActivityTestRule.getActivity();
        createNewWindow(
                mActivityTestRule.getActivity(),
                /* instanceId= */ 2,
                /* addIncognitoExtras= */ false);
        List<Tab> tabs = new ArrayList<>();
        var activeTab = ThreadUtils.runOnUiThreadBlocking(activity::getActivityTab);
        tabs.add(activeTab);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.moveTabsToOtherWindow(
                            tabs, MultiInstanceManager.NewWindowAppSource.MENU);
                });
        assertTrue("Target selector dialog should be visible", mModalDialogManager.isShowing());
    }

    @Test
    @SmallTest
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "https://crbug.com/454379518")
    public void moveTabsToOtherWindow_singleWindowOpen() {
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        List<Tab> tabs = new ArrayList<>();
        var activity = mActivityTestRule.getActivity();
        var activeTab = ThreadUtils.runOnUiThreadBlocking(activity::getActivityTab);
        tabs.add(activeTab);

        verifyInstanceState(/* expectedActiveInstances= */ 1, /* expectedTotalInstances= */ 1);
        ApplicationTestUtils.waitForActivityWithClass(
                ChromeTabbedActivity.class,
                Stage.RESUMED,
                () ->
                        mMultiInstanceManager.moveTabsToOtherWindow(
                                tabs, MultiInstanceManager.NewWindowAppSource.WINDOW_MANAGER));
        assertFalse(
                "Target selector dialog should not be visible with only one window open",
                mModalDialogManager.isShowing());
        // A new instance should be created because of the new window opened.
        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 2);
    }

    @Test
    @SmallTest
    @Restriction({
        DeviceFormFactor.TABLET_OR_DESKTOP,
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
        DeviceRestriction.RESTRICTION_TYPE_NON_FOLDABLE
    })
    @Features.EnableFeatures({
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW,
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT
    })
    public void moveTabsToOtherWindow_multipleWindowsOpen_hideTargetSelector_newIncognitoWindow() {
        var incognitoActivity =
                createNewWindow(
                        mActivityTestRule.getActivity(),
                        /* instanceId= */ 3,
                        /* addIncognitoExtras= */ true);
        createNewWindows(
                mActivityTestRule.getActivity(),
                /* numWindows= */ 2,
                /* addIncognitoExtras= */ false);
        List<Tab> tabs = new ArrayList<>();
        var activeTab = ThreadUtils.runOnUiThreadBlocking(incognitoActivity::getActivityTab);
        tabs.add(activeTab);

        ChromeTabbedActivity newWindow =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () ->
                                mMultiInstanceManager.moveTabsToOtherWindow(
                                        tabs, MultiInstanceManager.NewWindowAppSource.MENU));

        assertFalse(
                "Target selector dialog should not be visible", mModalDialogManager.isShowing());
        assertTrue("New window should be incognito", newWindow.isIncognitoWindow());
    }

    @Test
    @SmallTest
    @Restriction({
        DeviceFormFactor.TABLET_OR_DESKTOP,
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
        DeviceRestriction.RESTRICTION_TYPE_NON_FOLDABLE
    })
    @Features.EnableFeatures({
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW,
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT
    })
    public void moveTabsToOtherWindow_multipleWindowsOpen_hideTargetSelector_newRegularWindow() {
        var activity = mActivityTestRule.getActivity();
        createNewWindows(
                mActivityTestRule.getActivity(),
                /* numWindows= */ 2,
                /* addIncognitoExtras= */ true);
        List<Tab> tabs = new ArrayList<>();
        var activeTab = ThreadUtils.runOnUiThreadBlocking(activity::getActivityTab);
        tabs.add(activeTab);

        ChromeTabbedActivity newWindow =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () ->
                                mMultiInstanceManager.moveTabsToOtherWindow(
                                        tabs, MultiInstanceManager.NewWindowAppSource.MENU));

        assertFalse(
                "Target selector dialog should not be visible", mModalDialogManager.isShowing());
        assertFalse("New window should not be incognito", newWindow.isIncognitoWindow());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW})
    public void moveTabsToOtherWindow_multipleWindowsOpen_showTargetSelector() {
        var activity = mActivityTestRule.getActivity();
        createNewWindows(
                mActivityTestRule.getActivity(),
                /* numWindows= */ 2,
                /* addIncognitoExtras= */ false);
        List<Tab> tabs = new ArrayList<>();
        var activeTab = ThreadUtils.runOnUiThreadBlocking(activity::getActivityTab);
        tabs.add(activeTab);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.moveTabsToOtherWindow(
                            tabs, MultiInstanceManager.NewWindowAppSource.MENU);
                });
        assertTrue("Target selector dialog should be visible", mModalDialogManager.isShowing());
    }

    @Test
    @SmallTest
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "https://crbug.com/454379518")
    public void moveTabGroupToOtherWindow_singleWindowOpen() {
        verifyInstanceState(/* expectedActiveInstances= */ 1, /* expectedTotalInstances= */ 1);
        TabGroupMetadata tabGroupMetadata = getTabGroupMetaData();

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
    }

    @Test
    @SmallTest
    public void moveTabGroupToOtherWindow_multipleWindowsOpen() {
        createNewWindow(
                mActivityTestRule.getActivity(),
                /* instanceId= */ 2,
                /* addIncognitoExtras= */ false);
        TabGroupMetadata tabGroupMetadata = getTabGroupMetaData();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.moveTabGroupToOtherWindow(
                            tabGroupMetadata,
                            MultiInstanceManager.NewWindowAppSource.WINDOW_MANAGER);
                });
        assertTrue("Target selector dialog should be visible", mModalDialogManager.isShowing());
    }

    @Test
    @SmallTest
    @Restriction({
        DeviceFormFactor.TABLET_OR_DESKTOP,
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
        DeviceRestriction.RESTRICTION_TYPE_NON_FOLDABLE
    })
    @Features.EnableFeatures({
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW,
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT
    })
    public void moveTabGroupToOtherWindow_multipleWindowsOpen_incognitoWindow_hideTargetSelector() {
        createNewWindows(
                mActivityTestRule.getActivity(),
                /* numWindows= */ 2,
                /* addIncognitoExtras= */ true);
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
                "Target selector dialog should not be visible", mModalDialogManager.isShowing());
        assertFalse("New window should not be incognito", newWindow.isIncognitoWindow());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures({
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW,
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT
    })
    public void moveTabGroupToOtherWindow_multipleWindowsOpen_incognitoWindow_showTargetSelector() {
        createNewWindows(
                mActivityTestRule.getActivity(),
                /* numWindows= */ 3,
                /* addIncognitoExtras= */ true);
        TabGroupMetadata incognitoTabGroupMetadata =
                new TabGroupMetadata(
                        /* selectedTabId= */ TAB1_ID,
                        /* sourceWindowId= */ 1,
                        TAB_GROUP_ID1,
                        TAB_IDS_TO_URLS,
                        /* tabGroupColor= */ 0,
                        TAB_GROUP_TITLE,
                        /* mhtmlTabTitle= */ null,
                        /* tabGroupCollapsed= */ true,
                        /* isGroupShared= */ false,
                        /* isIncognito= */ true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.moveTabGroupToOtherWindow(
                            incognitoTabGroupMetadata,
                            MultiInstanceManager.NewWindowAppSource.WINDOW_MANAGER);
                });
        assertTrue("Target selector dialog should be visible", mModalDialogManager.isShowing());
    }

    @Test
    @SmallTest
    public void openUrlInOtherWindow_multipleWindowsOpen() {
        createNewWindow(
                mActivityTestRule.getActivity(),
                /* instanceId= */ 2,
                /* addIncognitoExtras= */ false);
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.openUrlInOtherWindow(
                            urlParams,
                            /* parentTabId= */ 1,
                            /* preferNew= */ false,
                            PersistedInstanceType.ACTIVE);
                });
        assertTrue("Target selector dialog should be visible", mModalDialogManager.isShowing());
    }

    @Test
    @SmallTest
    @DisableIf.Build(supported_abis_includes = "arm64-v8a", message = "https://crbug.com/454379518")
    public void openUrlInOtherWindow_singleWindowOpen() {
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));
        verifyInstanceState(/* expectedActiveInstances= */ 1, /* expectedTotalInstances= */ 1);

        ApplicationTestUtils.waitForActivityWithClass(
                ChromeTabbedActivity.class,
                Stage.RESUMED,
                () ->
                        mMultiInstanceManager.openUrlInOtherWindow(
                                urlParams,
                                /* parentTabId= */ 1,
                                /* preferNew= */ false,
                                PersistedInstanceType.ACTIVE));
        assertFalse(
                "Target selector dialog should not be visible", mModalDialogManager.isShowing());

        // A new instance should be created because of the new window opened.
        verifyInstanceState(/* expectedActiveInstances= */ 2, /* expectedTotalInstances= */ 2);
    }

    @Test
    @SmallTest
    public void openUrlInOtherWindow_openInNewWindow_preferNew() {
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));
        ChromeTabbedActivity firstActivity = mActivityTestRule.getActivity();
        createNewWindows(firstActivity, /* numWindows= */ 2, /* addIncognitoExtras= */ false);
        verifyInstanceState(/* expectedActiveInstances= */ 3, /* expectedTotalInstances= */ 3);

        ApplicationTestUtils.waitForActivityWithClass(
                ChromeTabbedActivity.class,
                Stage.RESUMED,
                () ->
                        mMultiInstanceManager.openUrlInOtherWindow(
                                urlParams,
                                /* parentTabId= */ 1,
                                /* preferNew= */ true,
                                PersistedInstanceType.ACTIVE));
        assertFalse(
                "Target selector dialog should not be visible", mModalDialogManager.isShowing());

        // A new instance should be created because of the new window opened.
        verifyInstanceState(/* expectedActiveInstances= */ 4, /* expectedTotalInstances= */ 4);
    }

    @Test
    @SmallTest
    public void openUrlInOtherWindow_openInNewWindow_reachInstanceLimit() {
        MultiWindowUtils.setMaxInstancesForTesting(5);
        ChromeTabbedActivity firstActivity = mActivityTestRule.getActivity();
        createNewWindows(firstActivity, /* numWindows= */ 4, /* addIncognitoExtras= */ false);
        verifyInstanceState(/* expectedActiveInstances= */ 5, /* expectedTotalInstances= */ 5);

        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mMultiInstanceManager.openUrlInOtherWindow(
                                urlParams,
                                /* parentTabId= */ 1,
                                /* preferNew= */ true,
                                PersistedInstanceType.ACTIVE));
        assertFalse(
                "Target selector dialog should not be visible", mModalDialogManager.isShowing());

        // Instance creation limit message should be shown and new instance should not be created.
        mActivityTestRule.waitForActivityCompletelyLoaded();
        verifyInstanceState(/* expectedActiveInstances= */ 5, /* expectedTotalInstances= */ 5);
        waitForMessage(firstActivity, MessageIdentifier.MULTI_INSTANCE_CREATION_LIMIT);
    }

    @Test
    @SmallTest
    @Restriction({
        DeviceFormFactor.TABLET_OR_DESKTOP,
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
        DeviceRestriction.RESTRICTION_TYPE_NON_FOLDABLE
    })
    @Features.EnableFeatures({
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW,
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT
    })
    public void
            openUrlInOtherIncognitoWindow_withoutActiveIncognitoWindows_opensNewIncognitoWindow() {
        createNewWindows(
                mActivityTestRule.getActivity(),
                /* numWindows= */ 2,
                /* addIncognitoExtras= */ false);
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));

        ChromeTabbedActivity newWindow =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.RESUMED,
                        () ->
                                mMultiInstanceManager.openUrlInOtherWindow(
                                        urlParams,
                                        /* parentTabId= */ 1,
                                        /* preferNew= */ false,
                                        PersistedInstanceType.ACTIVE
                                                | PersistedInstanceType.OFF_THE_RECORD));

        assertFalse(
                "Target selector dialog should not be visible", mModalDialogManager.isShowing());
        assertTrue("New window should be incognito", newWindow.isIncognitoWindow());
    }

    @Test
    @SmallTest
    @Restriction({
        DeviceFormFactor.TABLET_OR_DESKTOP,
        DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
        DeviceRestriction.RESTRICTION_TYPE_NON_FOLDABLE
    })
    @Features.EnableFeatures({
        ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW,
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT
    })
    public void
            openUrlInOtherIncognitoWindow_withActiveIncognitoWindows_showsTargetSelectorDialog() {
        createNewWindows(
                mActivityTestRule.getActivity(),
                /* numWindows= */ 2,
                /* addIncognitoExtras= */ true);
        LoadUrlParams urlParams = new LoadUrlParams(new GURL("about:blank"));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mMultiInstanceManager.openUrlInOtherWindow(
                            urlParams,
                            /* parentTabId= */ 1,
                            /* preferNew= */ false,
                            PersistedInstanceType.ACTIVE | PersistedInstanceType.OFF_THE_RECORD);
                });
        assertTrue("Target selector dialog should be visible", mModalDialogManager.isShowing());
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

    private void waitForMessage(
            ChromeTabbedActivity activity, @MessageIdentifier int messageIdentifier) {
        CriteriaHelper.pollUiThread(
                () -> {
                    MessageDispatcher messageDispatcher =
                            ThreadUtils.runOnUiThreadBlocking(
                                    () ->
                                            MessageDispatcherProvider.from(
                                                    activity.getWindowAndroid()));
                    List<MessageStateHandler> messages =
                            MessagesTestHelper.getEnqueuedMessages(
                                    messageDispatcher, messageIdentifier);
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
