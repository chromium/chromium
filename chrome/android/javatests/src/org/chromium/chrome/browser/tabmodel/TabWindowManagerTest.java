// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabWindowManager.TabModelSelectorFactory;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;

import java.util.ArrayList;
import java.util.List;

/**
 * Test for {@link TabWindowManager} APIs.  Makes sure the class handles multiple {@link Activity}s
 * requesting {@link TabModelSelector}s, {@link Activity}s getting destroyed, etc..
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class TabWindowManagerTest {
    private List<Activity> mActivities = new ArrayList<>();
    private final TabModelSelectorFactory mMockTabModelSelectorFactory =
            new TabModelSelectorFactory() {
                @Override
                public TabModelSelector buildSelector(Activity activity,
                        TabCreatorManager tabCreatorManager, int selectorIndex) {
                    return new MockTabModelSelector(0, 0, null);
                }
    };

    private ChromeActivity buildActivity() {
        ChromeActivity activity = new CustomTabActivity();
        mActivities.add(activity);
        ApplicationStatus.onStateChangeForTesting(activity, ActivityState.CREATED);
        return activity;
    }

    private void destroyActivity(Activity a) {
        mActivities.remove(a);
        ApplicationStatus.onStateChangeForTesting(a, ActivityState.DESTROYED);
    }

    private MockTabModelSelector requestSelector(ChromeActivity activity, int requestedIndex) {
        final TabWindowManager manager = TabWindowManager.getInstance();
        manager.setTabModelSelectorFactory(mMockTabModelSelectorFactory);
        return (MockTabModelSelector) manager.requestSelector(activity, activity, requestedIndex);
    }

    @After
    public void tearDown() {
        for (Activity a : mActivities) {
            ApplicationStatus.onStateChangeForTesting(a, ActivityState.DESTROYED);
        }
        mActivities.clear();
    }

    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    /**
     * Test that a single {@link Activity} can request a {@link TabModelSelector}.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testSingleActivity() {
        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 0);

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
        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 0);
        TabModelSelector selector1 = requestSelector(activity1, 1);

        Assert.assertNotNull("Was not able to build the TabModelSelector", selector0);
        Assert.assertNotNull("Was not able to build the TabModelSelector", selector1);
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

        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 0);
        // Request 0 again, but should get 1 instead.
        TabModelSelector selector1 = requestSelector(activity1, 0);

        Assert.assertNotNull("Was not able to build the TabModelSelector", selector0);
        Assert.assertNotNull("Was not able to build the TabModelSelector", selector1);
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

        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 2);
        // Request 2 again, but should get 0 instead.
        TabModelSelector selector1 = requestSelector(activity1, 2);

        Assert.assertNotNull("Was not able to build the TabModelSelector", selector0);
        Assert.assertNotNull("Was not able to build the TabModelSelector", selector1);
        Assert.assertEquals("Unexpected model index", 2, manager.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity1));
    }

    /**
     * Test that a destroyed {@link Activity} properly gets removed from {@link TabWindowManager}.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testActivityDeathRemovesSingle() {
        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 0);

        Assert.assertNotNull("Was not able to build the TabModelSelector", selector0);
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
        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 0);

        Assert.assertNotNull("Was not able to build the TabModelSelector", selector0);
        Assert.assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));

        destroyActivity(activity0);

        Assert.assertEquals("Still found model", TabWindowManager.INVALID_WINDOW_INDEX,
                manager.getIndexForWindow(activity0));

        ChromeActivity activity1 = buildActivity();
        TabModelSelector selector1 = requestSelector(activity1, 0);

        Assert.assertNotNull("Was not able to build the TabModelSelector", selector1);
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

        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        TabModelSelector selector0 = requestSelector(activity0, 0);
        TabModelSelector selector1 = requestSelector(activity1, 1);

        Assert.assertNotNull("Was not able to build the TabModelSelector", selector0);
        Assert.assertNotNull("Was not able to build the TabModelSelector", selector1);
        Assert.assertEquals("Unexpected model index", 0, manager.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 1, manager.getIndexForWindow(activity1));

        destroyActivity(activity1);

        Assert.assertEquals("Still found model", TabWindowManager.INVALID_WINDOW_INDEX,
                manager.getIndexForWindow(activity1));

        ChromeActivity activity2 = buildActivity();
        TabModelSelector selector2 = requestSelector(activity2, 1);

        Assert.assertNotNull("Was not able to build the TabModelSelector", selector2);
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
        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        MockTabModelSelector selector0 = requestSelector(activity0, 0);
        MockTabModelSelector selector1 = requestSelector(activity1, 1);
        Tab tab1 = selector0.addMockTab();
        Tab tab2 = selector1.addMockIncognitoTab();

        Assert.assertFalse(manager.tabExistsInAnySelector(tab1.getId() - 1));
        Assert.assertTrue(manager.tabExistsInAnySelector(tab1.getId()));
        Assert.assertTrue(manager.tabExistsInAnySelector(tab2.getId()));
        Assert.assertFalse(manager.tabExistsInAnySelector(tab2.getId() + 1));

        AsyncTabParamsManager.getAsyncTabParams().clear();
        final int asyncTabId = 123;
        final TabReparentingParams dummyParams =
                new TabReparentingParams(new MockTab(0, false), null, null);
        Assert.assertFalse(manager.tabExistsInAnySelector(asyncTabId));
        AsyncTabParamsManager.add(asyncTabId, dummyParams);
        try {
            Assert.assertTrue(manager.tabExistsInAnySelector(asyncTabId));
        } finally {
            AsyncTabParamsManager.getAsyncTabParams().clear();
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
        final TabWindowManager manager = TabWindowManager.getInstance();

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        MockTabModelSelector selector0 = requestSelector(activity0, 0);
        MockTabModelSelector selector1 = requestSelector(activity1, 1);
        Tab tab1 = selector0.addMockTab();
        Tab tab2 = selector1.addMockIncognitoTab();

        Assert.assertNull(manager.getTabById(tab1.getId() - 1));
        Assert.assertNotNull(manager.getTabById(tab1.getId()));
        Assert.assertNotNull(manager.getTabById(tab2.getId()));
        Assert.assertNull(manager.getTabById(tab2.getId() + 1));

        AsyncTabParamsManager.getAsyncTabParams().clear();
        final int asyncTabId = 123;
        final TabReparentingParams dummyParams =
                new TabReparentingParams(new MockTab(0, false), null, null);
        Assert.assertNull(manager.getTabById(asyncTabId));
        AsyncTabParamsManager.add(asyncTabId, dummyParams);
        try {
            Assert.assertNotNull(manager.getTabById(asyncTabId));
        } finally {
            AsyncTabParamsManager.getAsyncTabParams().clear();
        }
    }
}
