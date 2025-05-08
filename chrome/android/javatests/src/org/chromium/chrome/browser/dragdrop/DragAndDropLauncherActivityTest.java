// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Build.VERSION_CODES;

import androidx.annotation.RequiresApi;
import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Matchers;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadataExtractor;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.ui.dragdrop.DragDropMetricUtils.UrlIntentSource;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;

/** Tests for {@link DragAndDropLauncherActivity}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "This class tests activity start behavior and thus cannot be batched.")
@MinAndroidSdkLevel(VERSION_CODES.S)
@RequiresApi(VERSION_CODES.S)
public class DragAndDropLauncherActivityTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Context mContext;
    private String mLinkUrl;
    private final CallbackHelper mTabAddedCallback = new CallbackHelper();
    private UserActionTester mActionTester;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityOnBlankPage();
        mContext = ContextUtils.getApplicationContext();
        mLinkUrl = JUnitTestGURLs.EXAMPLE_URL.getSpec();
        mActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
    }

    /**
     * Tests that a dragged link intent is launched in a new Chrome window from
     * DragAndDropLauncherActivity.
     */
    @Test
    @LargeTest
    public void testDraggedLink_newWindow() throws Exception {
        Intent intent = createLinkDragDropIntent(mLinkUrl, TabWindowManager.INVALID_WINDOW_ID);
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.DragDrop.Tab.Type", DragDropType.LINK_TO_NEW_INSTANCE);
        ChromeTabbedActivity lastAccessedActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> mContext.startActivity(intent));

        // Verify that a new Chrome instance is created.
        Assert.assertEquals(
                "Number of Chrome instances should be correct.",
                2,
                MultiWindowUtils.getInstanceCount());

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activityTab = lastAccessedActivity.getActivityTab();
                    Criteria.checkThat(
                            "Activity tab should be non-null.",
                            activityTab,
                            Matchers.notNullValue());
                });

        // Verify that the link is opened in the activity tab of the new Chrome instance.
        Tab activityTab = ThreadUtils.runOnUiThreadBlocking(lastAccessedActivity::getActivityTab);
        Assert.assertEquals(
                "Activity tab URL should match the dragged link URL.",
                new GURL(mLinkUrl).getSpec(),
                ChromeTabUtils.getUrlOnUiThread(activityTab).getSpec());
        Assert.assertTrue(
                "User action should be logged.",
                mActionTester
                        .getActions()
                        .contains(DragAndDropLauncherActivity.LAUNCHED_FROM_LINK_USER_ACTION));
        // Verify metric is recorded.
        histogramExpectation.assertExpected();

        lastAccessedActivity.finish();
    }

    /**
     * Tests that a dragged link intent is launched in a new tab on the Chrome window with the
     * specified windowId from DragAndDropLauncherActivity when the max number of Chrome instances
     * is open.
     */
    @Test
    @LargeTest
    public void testDraggedLink_existingWindow_maxInstances() throws Exception {
        // Simulate creation of max Chrome instances (= 2 for the purpose of testing) and establish
        // the last accessed instance. Actual max # of instances will not be created as this would
        // cause a significant overhead for testing this scenario where a link is opened in an
        // existing instance.
        Intent intent = createLinkDragDropIntent(mLinkUrl, TabWindowManager.INVALID_WINDOW_ID);
        ChromeTabbedActivity lastAccessedActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> mContext.startActivity(intent));
        MultiWindowUtils.setMaxInstancesForTesting(2);
        int lastAccessedInstanceId =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> MultiWindowUtils.getInstanceIdForLinkIntent(lastAccessedActivity));
        addTabModelSelectorObserver(lastAccessedActivity);

        // Simulate an attempt to open a new window from a dragged link intent when max instances
        // are open. Do this by setting the EXTRA_WINDOW_ID extra on the intent that is reflective
        // of this state.
        Intent newIntent =
                createLinkDragDropIntent(JUnitTestGURLs.MAPS_URL.getSpec(), lastAccessedInstanceId);
        mContext.startActivity(newIntent);
        mTabAddedCallback.waitForCallback(0);

        // Verify that no new Chrome instance is created.
        Assert.assertEquals(
                "Number of Chrome instances should be correct.",
                2,
                MultiWindowUtils.getInstanceCount());

        // Verify that the link is opened in the activity tab of the last accessed Chrome instance.
        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activityTab = lastAccessedActivity.getActivityTab();
                    Criteria.checkThat(
                            "Activity tab URL should match the dragged link URL.",
                            ChromeTabUtils.getUrlOnUiThread(activityTab).getSpec(),
                            Matchers.is(JUnitTestGURLs.MAPS_URL.getSpec()));
                });

        lastAccessedActivity.finish();
    }

    /**
     * Tests that a dragged link intent is dropped by DragAndDropLauncherActivity when the intent
     * creation timestamp is invalid.
     */
    @Test
    @LargeTest
    public void testDraggedLink_invalidIntentCreationTimestamp() throws Exception {
        DragAndDropLauncherActivity.setDropTimeoutMsForTesting(500L);
        Intent intent = createLinkDragDropIntent(mLinkUrl, TabWindowManager.INVALID_WINDOW_ID);
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder().expectNoRecords("Android.DragDrop.Tab.Type").build();
        Thread.sleep(DragAndDropLauncherActivity.getDropTimeoutMs() + 1);
        mContext.startActivity(intent);
        // Verify that no new Chrome instance is created.
        Assert.assertEquals(
                "Number of instances should be correct.", 1, MultiWindowUtils.getInstanceCount());
        // Verify metric is not recorded.
        histogramExpectation.assertExpected();
    }

    /**
     * Tests that a dragged tab intent is launched by DragAndDropLauncherActivity in a new Chrome
     * window with successful tab reparenting.
     */
    @Test
    @LargeTest
    public void testDraggedTab_newWindow() throws Exception {
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        ChromeTabbedActivity.HISTOGRAM_DRAGGED_TAB_OPENED_NEW_WINDOW, true);
        var sourceActivity = mActivityTestRule.getActivity();

        // Open a new tab in the current activity, that will be used as the dragged tab.
        ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(), sourceActivity);

        var draggedTab = ThreadUtils.runOnUiThreadBlocking(sourceActivity::getActivityTab);
        var initialTabCountInSourceActivity =
                sourceActivity.getTabModelSelector().getTotalTabCount();

        // Simulate a tab drag/drop event to launch an intent in a new Chrome instance.
        Intent intent = createTabDragDropIntent(draggedTab, sourceActivity);
        ChromeTabbedActivity newActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> mContext.startActivity(intent));

        // Verify that a new Chrome instance is created.
        Assert.assertEquals(
                "Number of Chrome instances should be correct.",
                2,
                MultiWindowUtils.getInstanceCount());

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activityTab = newActivity.getActivityTab();
                    Criteria.checkThat(
                            "Activity tab should be non-null.",
                            activityTab,
                            Matchers.notNullValue());
                    Criteria.checkThat(
                            "Tab should be moved from the source window.",
                            sourceActivity.getTabModelSelector().getTotalTabCount(),
                            Matchers.is(initialTabCountInSourceActivity - 1));
                });

        // Verify that the dragged tab is reparented in the new instance.
        int tabCountInNewActivity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> newActivity.getTabModelSelector().getTotalTabCount());
        Assert.assertEquals(
                "New window should have only the dragged tab.", 1, tabCountInNewActivity);
        Tab newActivityTab = ThreadUtils.runOnUiThreadBlocking(newActivity::getActivityTab);
        Assert.assertEquals(
                "New activity tab should be the same as the dragged tab.",
                draggedTab,
                newActivityTab);

        // Verify metrics are recorded.
        Assert.assertTrue(
                "User action should be logged.",
                mActionTester
                        .getActions()
                        .contains(DragAndDropLauncherActivity.LAUNCHED_FROM_TAB_USER_ACTION));
        histogramExpectation.assertExpected();

        newActivity.finish();
    }

    /**
     * Tests that a dragged tab group intent is launched by DragAndDropLauncherActivity in a new
     * Chrome window with successful tab group reparenting.
     */
    @Test
    @LargeTest
    @EnableFeatures(ChromeFeatureList.TAB_STRIP_GROUP_DRAG_DROP_ANDROID)
    public void testDraggedTabGroup_newWindow() throws Exception {
        var sourceActivity = mActivityTestRule.getActivity();

        // Open a tab group in the current activity, that will be used as the dragged tab group.
        List<Tab> draggedTabGroup = prepareTabGroup();

        var initialTabCountInSourceActivity =
                sourceActivity.getTabModelSelector().getTotalTabCount();

        // Simulate a tab drag/drop event to launch an intent in a new Chrome instance.
        Intent intent = createTabGroupDragDropIntent(draggedTabGroup, sourceActivity);
        ChromeTabbedActivity newActivity =
                ApplicationTestUtils.waitForActivityWithClass(
                        ChromeTabbedActivity.class,
                        Stage.CREATED,
                        () -> mContext.startActivity(intent));

        // Verify that a new Chrome instance is created.
        Assert.assertEquals(
                "Number of Chrome instances should be correct.",
                2,
                MultiWindowUtils.getInstanceCount());

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab activityTab = newActivity.getActivityTab();
                    Criteria.checkThat(
                            "Activity tab should be non-null.",
                            activityTab,
                            Matchers.notNullValue());
                    Criteria.checkThat(
                            "Tab group should be moved from the source window.",
                            sourceActivity.getTabModelSelector().getTotalTabCount(),
                            Matchers.is(initialTabCountInSourceActivity - draggedTabGroup.size()));
                });

        // Verify that the dragged tab is reparented in the new instance.
        int tabCountInNewActivity =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> newActivity.getTabModelSelector().getTotalTabCount());
        Assert.assertEquals(
                "New window should have only the dragged tab group.",
                draggedTabGroup.size(),
                tabCountInNewActivity);
        Tab newActivityTab = ThreadUtils.runOnUiThreadBlocking(newActivity::getActivityTab);
        Assert.assertEquals(
                "New activity selected tab should be the same as the dragged tab group selected"
                        + " tab.",
                draggedTabGroup.get(0),
                newActivityTab);

        // Verify metrics are recorded.
        Assert.assertTrue(
                "User action should be logged.",
                mActionTester
                        .getActions()
                        .contains(DragAndDropLauncherActivity.LAUNCHED_FROM_TAB_GROUP_USER_ACTION));
        newActivity.finish();
    }

    private void addTabModelSelectorObserver(ChromeTabbedActivity activity) {
        TabModelSelector tabModelSelector = activity.getTabModelSelector();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelSelectorObserver tabModelSelectorObserver =
                            new TabModelSelectorObserver() {
                                @Override
                                public void onNewTabCreated(
                                        Tab tab, @TabCreationState int creationState) {
                                    mTabAddedCallback.notifyCalled();
                                }
                            };
                    tabModelSelector.addObserver(tabModelSelectorObserver);
                });
    }

    private Intent createLinkDragDropIntent(String linkUrl, Integer windowId)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        DragAndDropLauncherActivity.getLinkLauncherIntent(
                                mContext, linkUrl, windowId, UrlIntentSource.LINK));
    }

    private Intent createTabDragDropIntent(Tab tab, Activity sourceActivity)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int sourceWindowId =
                            TabWindowManagerSingleton.getInstance().getIdForWindow(sourceActivity);

                    return DragAndDropLauncherActivity.buildTabOrGroupIntent(
                            createTabDropData(tab, /* allowDragToCreateNewInstance= */ true),
                            mContext,
                            sourceWindowId,
                            /* destWindowId= */ TabWindowManager.INVALID_WINDOW_ID);
                });
    }

    private Intent createTabGroupDragDropIntent(List<Tab> draggedTabGroup, Activity sourceActivity)
            throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int sourceWindowId =
                            TabWindowManagerSingleton.getInstance().getIdForWindow(sourceActivity);

                    TabGroupMetadata tabGroupMetadata =
                            TabGroupMetadataExtractor.extractTabGroupMetadata(
                                    draggedTabGroup,
                                    sourceWindowId,
                                    draggedTabGroup.get(0).getId(),
                                    /* isGroupShared= */ false);
                    return DragAndDropLauncherActivity.buildTabOrGroupIntent(
                            createTabGroupDropData(
                                    tabGroupMetadata, /* allowDragToCreateNewInstance= */ true),
                            mContext,
                            sourceWindowId,
                            /* destWindowId= */ TabWindowManager.INVALID_WINDOW_ID);
                });
    }

    private List<Tab> prepareTabGroup() {
        // 1. Create two new tabs.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());

        // 2. Assert the normal tab strip is selected and there are 3 normal tabs in total.
        Assert.assertFalse(
                "Expected normal strip to be selected",
                mActivityTestRule.getActivity().getTabModelSelector().isIncognitoSelected());
        Assert.assertEquals(
                "There should be three tabs present",
                3,
                mActivityTestRule.getActivity().getCurrentTabModel().getCount());

        // 3. Create tab group with 2 tabs.
        List<Tab> tabGroup =
                new ArrayList<>(
                        Arrays.asList(
                                mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(0),
                                mActivityTestRule.getActivity().getCurrentTabModel().getTabAt(1)));
        TabUiTestHelper.createTabGroup(
                mActivityTestRule.getActivity(), /* isIncognito= */ false, tabGroup);
        return tabGroup;
    }

    private ChromeDropDataAndroid createTabGroupDropData(
            TabGroupMetadata tabGroupMetadata, boolean allowDragToCreateNewInstance) {
        return new ChromeTabGroupDropDataAndroid.Builder()
                .withTabGroupMetadata(tabGroupMetadata)
                .withAllowDragToCreateInstance(allowDragToCreateNewInstance)
                .build();
    }

    private ChromeDropDataAndroid createTabDropData(Tab tab, boolean allowDragToCreateNewInstance) {
        return new ChromeTabDropDataAndroid.Builder()
                .withTab(tab)
                .withAllowDragToCreateInstance(allowDragToCreateNewInstance)
                .build();
    }
}
