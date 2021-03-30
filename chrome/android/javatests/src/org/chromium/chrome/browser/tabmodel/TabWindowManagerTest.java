// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;

/**
 * Test for {@link TabWindowManagerImpl} through {@link TabWindowManagerSingleton}.
 *
 * Makes sure the class handles multiple {@link Activity}s requesting {@link TabModelSelector}s,
 * {@link Activity}s getting destroyed, etc.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class TabWindowManagerTest {
    private static final TabModelSelectorFactory sMockTabModelSelectorFactory =
            new TabModelSelectorFactory() {
                @Override
                public TabModelSelector buildSelector(Activity activity,
                        TabCreatorManager tabCreatorManager,
                        NextTabPolicySupplier nextTabPolicySupplier, int selectorIndex) {
                    return new MockTabModelSelector(0, 0, null);
                }
            };

    @BeforeClass
    public static void setUpFixture() {
        TabWindowManagerSingleton.setTabModelSelectorFactoryForTesting(
                sMockTabModelSelectorFactory);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> { ApplicationStatus.resetActivitiesForInstrumentationTests(); });
    }

    private ChromeActivity buildActivity() {
        ChromeActivity activity = new CustomTabActivity();
        ThreadUtils.runOnUiThreadBlocking(() -> {
            ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        });
        return activity;
    }

    private void destroyActivity(Activity a) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> { ApplicationStatus.onStateChangeForTesting(a, ActivityState.DESTROYED); });
    }

    private Pair<Integer, TabModelSelector> requestSelector(
            ChromeActivity activity, int requestedIndex) {
        final TabWindowManager manager = TabWindowManagerSingleton.getInstance();
        return manager.requestSelector(
                activity, activity, () -> NextTabPolicy.HIERARCHICAL, requestedIndex);
    }

    /**
     * Test that a single {@link Activity} can request a {@link TabModelSelector}.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testSingleActivity() {
        final TabWindowManager manager = TabWindowManagerSingleton.getInstance();

        ChromeActivity activity0 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 = requestSelector(activity0, 0);

        Assert.assertEquals(0, assignment0.first.intValue());
        TabModelSelector selector0 = assignment0.second;
        Assert.assertNotNull("Was not able to build the TabModelSelector", selector0);
        Assert.assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));
    }

    /**
     * Test that two {@link Activity}s can request different {@link TabModelSelector}s.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testMultipleActivities() {
        Assert.assertTrue("Not enough selectors", TabWindowManager.MAX_SIMULTANEOUS_SELECTORS >= 2);
        final TabWindowManager manager = TabWindowManagerSingleton.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 = requestSelector(activity0, 0);
        Pair<Integer, TabModelSelector> assignment1 = requestSelector(activity1, 1);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertEquals(1, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 1, manager.getIndexForWindow(activity1));
    }

    /**
     * Test that trying to have too many {@link Activity}s requesting {@link TabModelSelector}s is
     * properly capped and returns {@code null}.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testTooManyActivities() {
        for (int i = 0; i < TabWindowManager.MAX_SIMULTANEOUS_SELECTORS; i++) {
            ChromeActivity a = buildActivity();
            Assert.assertNotNull("Could not build selector", requestSelector(a, 0));
        }

        ChromeActivity activity = buildActivity();
        Assert.assertNull(
                "Built selectors past the max number supported", requestSelector(activity, 0));
    }

    /**
     * Test that requesting the same {@link TabModelSelector} index will fall back and return a
     * model for a different available index instead.  In this case, a higher index (0 -> 1).
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testIndexFallback() {
        Assert.assertTrue("Not enough selectors", TabWindowManager.MAX_SIMULTANEOUS_SELECTORS >= 2);

        final TabWindowManager manager = TabWindowManagerSingleton.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 = requestSelector(activity0, 0);
        // Request 0 again, but should get 1 instead.
        Pair<Integer, TabModelSelector> assignment1 = requestSelector(activity1, 0);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertEquals(1, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 1, manager.getIndexForWindow(activity1));
    }

    /**
     * Test that requesting the same {@link TabModelSelector} index will fall back and return a
     * model for a different available index instead.  In this case, a lower index (2 -> 0).
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testIndexFallback2() {
        Assert.assertTrue("Not enough selectors", TabWindowManager.MAX_SIMULTANEOUS_SELECTORS >= 3);

        final TabWindowManager manager = TabWindowManagerSingleton.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 = requestSelector(activity0, 2);
        // Request 2 again, but should get 0 instead.
        Pair<Integer, TabModelSelector> assignment1 = requestSelector(activity1, 2);

        Assert.assertEquals(2, assignment0.first.intValue());
        Assert.assertEquals(0, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 2, manager.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity1));
    }

    /**
     * Test that a destroyed {@link Activity} properly gets removed from {@link
     * TabWindowManagerImpl}.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testActivityDeathRemovesSingle() {
        final TabWindowManager manager = TabWindowManagerSingleton.getInstance();

        ChromeActivity activity0 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 = requestSelector(activity0, 0);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));

        destroyActivity(activity0);

        Assert.assertEquals("Still found model", TabWindowManager.INVALID_WINDOW_INDEX,
                manager.getIndexForWindow(activity0));
    }

    /**
     * Test that an {@link Activity} requesting an index that was previously assigned to a destroyed
     * {@link Activity} can take that {@link TabModelSelector}.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testActivityDeathLetsModelReassign() {
        final TabWindowManager manager = TabWindowManagerSingleton.getInstance();

        ChromeActivity activity0 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 = requestSelector(activity0, 0);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));

        destroyActivity(activity0);

        Assert.assertEquals("Still found model", TabWindowManager.INVALID_WINDOW_INDEX,
                manager.getIndexForWindow(activity0));

        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment1 = requestSelector(activity1, 0);

        Assert.assertEquals(0, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity1));
    }

    /**
     * Test that an {@link Activity} requesting an index that was previously assigned to a destroyed
     * {@link Activity} can take that {@link TabModelSelector} when there are other
     * {@link Activity}s assigned {@link TabModelSelector}s.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testActivityDeathWithMultipleActivities() {
        Assert.assertTrue("Not enough selectors", TabWindowManager.MAX_SIMULTANEOUS_SELECTORS >= 2);

        final TabWindowManager manager = TabWindowManagerSingleton.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 = requestSelector(activity0, 0);
        Pair<Integer, TabModelSelector> assignment1 = requestSelector(activity1, 1);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertEquals(1, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 1, manager.getIndexForWindow(activity1));

        destroyActivity(activity1);

        Assert.assertEquals("Still found model", TabWindowManager.INVALID_WINDOW_INDEX,
                manager.getIndexForWindow(activity1));

        ChromeActivity activity2 = buildActivity();
        Pair<Integer, TabModelSelector> assignment2 = requestSelector(activity2, 1);

        Assert.assertEquals(1, assignment2.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment2.second);
        Assert.assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 1, manager.getIndexForWindow(activity2));
    }

    /**
     * Tests that tabExistsInAnySelector() functions properly.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testTabExistsInAnySelector() {
        final TabWindowManager manager = TabWindowManagerSingleton.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 = requestSelector(activity0, 0);
        Pair<Integer, TabModelSelector> assignment1 = requestSelector(activity1, 1);
        MockTabModelSelector selector0 = (MockTabModelSelector) assignment0.second;
        MockTabModelSelector selector1 = (MockTabModelSelector) assignment1.second;
        Tab tab1 = selector0.addMockTab();
        Tab tab2 = selector1.addMockIncognitoTab();

        Assert.assertNull(manager.getTabById(tab1.getId() - 1));
        Assert.assertNotNull(manager.getTabById(tab1.getId()));
        Assert.assertNotNull(manager.getTabById(tab2.getId()));
        Assert.assertNull(manager.getTabById(tab2.getId() + 1));

        AsyncTabParamsManager asyncTabParamsManager = AsyncTabParamsManagerSingleton.getInstance();
        asyncTabParamsManager.getAsyncTabParams().clear();
        final int asyncTabId = 123;
        final TabReparentingParams dummyParams =
                new TabReparentingParams(new MockTab(0, false), null);
        Assert.assertNull(manager.getTabById(asyncTabId));
        asyncTabParamsManager.add(asyncTabId, dummyParams);
        try {
            Assert.assertNotNull(manager.getTabById(asyncTabId));
        } finally {
            asyncTabParamsManager.getAsyncTabParams().clear();
        }
    }

    /**
     * Tests that getTabById() functions properly.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testGetTabById() {
        final TabWindowManager manager = TabWindowManagerSingleton.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 = requestSelector(activity0, 0);
        Pair<Integer, TabModelSelector> assignment1 = requestSelector(activity1, 1);
        MockTabModelSelector selector0 = (MockTabModelSelector) assignment0.second;
        MockTabModelSelector selector1 = (MockTabModelSelector) assignment1.second;
        Tab tab1 = selector0.addMockTab();
        Tab tab2 = selector1.addMockIncognitoTab();

        Assert.assertNull(manager.getTabById(tab1.getId() - 1));
        Assert.assertNotNull(manager.getTabById(tab1.getId()));
        Assert.assertNotNull(manager.getTabById(tab2.getId()));
        Assert.assertNull(manager.getTabById(tab2.getId() + 1));

        AsyncTabParamsManager asyncTabParamsManager = AsyncTabParamsManagerSingleton.getInstance();
        asyncTabParamsManager.getAsyncTabParams().clear();
        final int asyncTabId = 123;
        final TabReparentingParams dummyParams =
                new TabReparentingParams(new MockTab(0, false), null);
        Assert.assertNull(manager.getTabById(asyncTabId));
        asyncTabParamsManager.add(asyncTabId, dummyParams);
        try {
            Assert.assertNotNull(manager.getTabById(asyncTabId));
        } finally {
            asyncTabParamsManager.getAsyncTabParams().clear();
        }
    }

    /**
     * Tests that getTabModelForTab(...) functions properly.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void getTabModelForTab() {
        final TabWindowManager manager = TabWindowManagerSingleton.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 = requestSelector(activity0, 0);
        Pair<Integer, TabModelSelector> assignment1 = requestSelector(activity1, 1);
        MockTabModelSelector selector0 = (MockTabModelSelector) assignment0.second;
        MockTabModelSelector selector1 = (MockTabModelSelector) assignment1.second;
        Tab tab1 = selector0.addMockTab();
        Tab tab2 = selector1.addMockTab();
        Tab tab3 = selector0.addMockIncognitoTab();
        Tab tab4 = selector1.addMockIncognitoTab();

        Assert.assertEquals(
                selector0.getModel(/* incognito= */ false), manager.getTabModelForTab(tab1));
        Assert.assertEquals(
                selector1.getModel(/* incognito= */ false), manager.getTabModelForTab(tab2));
        Assert.assertEquals(
                selector0.getModel(/* incognito= */ true), manager.getTabModelForTab(tab3));
        Assert.assertEquals(
                selector1.getModel(/* incognito= */ true), manager.getTabModelForTab(tab4));
    }
}
