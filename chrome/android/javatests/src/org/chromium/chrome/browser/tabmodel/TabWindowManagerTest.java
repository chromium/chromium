// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;

import java.util.ArrayList;
import java.util.List;

/**
 * Test for {@link TabWindowManagerImpl}.
 *
 * Makes sure the class handles multiple {@link Activity}s requesting {@link TabModelSelector}s,
 * {@link Activity}s getting destroyed, etc.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class TabWindowManagerTest {
    private TabWindowManager mSubject;
    private AsyncTabParamsManager mAsyncTabParamsManager;
    private NextTabPolicySupplier mNextTabPolicySupplier = () -> NextTabPolicy.HIERARCHICAL;

    private static final TabModelSelectorFactory sMockTabModelSelectorFactory =
            new TabModelSelectorFactory() {
                @Override
                public TabModelSelector buildSelector(Activity activity,
                        TabCreatorManager tabCreatorManager,
                        NextTabPolicySupplier nextTabPolicySupplier, int selectorIndex) {
                    return new MockTabModelSelector(0, 0, null);
                }
            };

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            mAsyncTabParamsManager = AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
            int maxInstances = MultiWindowUtils.getMaxInstances();
            mSubject = TabWindowManagerFactory.createInstance(
                    sMockTabModelSelectorFactory, mAsyncTabParamsManager, maxInstances);
        });
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

    /**
     * Test that a single {@link Activity} can request a {@link TabModelSelector}.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testSingleActivity() {
        ChromeActivity activity0 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(activity0, activity0, mNextTabPolicySupplier, 0);

        Assert.assertEquals(0, assignment0.first.intValue());
        TabModelSelector selector0 = assignment0.second;
        Assert.assertNotNull("Was not able to build the TabModelSelector", selector0);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));

        destroyActivity(activity0);
    }

    /**
     * Test that two {@link Activity}s can request different {@link TabModelSelector}s.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testMultipleActivities() {
        Assert.assertTrue("Not enough selectors", mSubject.getMaxSimultaneousSelectors() >= 2);

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(activity0, activity0, mNextTabPolicySupplier, 0);
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(activity1, activity1, mNextTabPolicySupplier, 1);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertEquals(1, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 1, mSubject.getIndexForWindow(activity1));

        destroyActivity(activity0);
        destroyActivity(activity1);
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
        List<ChromeActivity> activityList = new ArrayList<>();
        for (int i = 0; i < mSubject.getMaxSimultaneousSelectors(); i++) {
            ChromeActivity a = buildActivity();
            activityList.add(a);
            Assert.assertNotNull("Could not build selector",
                    mSubject.requestSelector(a, a, mNextTabPolicySupplier, 0));
        }

        ChromeActivity activity = buildActivity();
        activityList.add(activity);
        Assert.assertNull("Built selectors past the max number supported",
                mSubject.requestSelector(activity, activity, mNextTabPolicySupplier, 0));

        for (ChromeActivity a : activityList) {
            destroyActivity(a);
        }
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
        Assert.assertTrue("Not enough selectors", mSubject.getMaxSimultaneousSelectors() >= 2);

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(activity0, activity0, mNextTabPolicySupplier, 0);
        // Request 0 again, but should get 1 instead.
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(activity1, activity1, mNextTabPolicySupplier, 0);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertEquals(1, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 1, mSubject.getIndexForWindow(activity1));

        destroyActivity(activity0);
        destroyActivity(activity1);
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
        Assert.assertTrue("Not enough selectors", mSubject.getMaxSimultaneousSelectors() >= 3);

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(activity0, activity0, mNextTabPolicySupplier, 2);
        // Request 2 again, but should get 0 instead.
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(activity1, activity1, mNextTabPolicySupplier, 2);

        Assert.assertEquals(2, assignment0.first.intValue());
        Assert.assertEquals(0, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 2, mSubject.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity1));

        destroyActivity(activity0);
        destroyActivity(activity1);
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
        ChromeActivity activity0 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(activity0, activity0, mNextTabPolicySupplier, 0);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));

        destroyActivity(activity0);

        Assert.assertEquals("Still found model", TabWindowManager.INVALID_WINDOW_INDEX,
                mSubject.getIndexForWindow(activity0));
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
        ChromeActivity activity0 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(activity0, activity0, mNextTabPolicySupplier, 0);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));

        destroyActivity(activity0);

        Assert.assertEquals("Still found model", TabWindowManager.INVALID_WINDOW_INDEX,
                mSubject.getIndexForWindow(activity0));

        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(activity1, activity1, mNextTabPolicySupplier, 0);

        Assert.assertEquals(0, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity1));

        destroyActivity(activity1);
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
        Assert.assertTrue("Not enough selectors", mSubject.getMaxSimultaneousSelectors() >= 2);

        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(activity0, activity0, mNextTabPolicySupplier, 0);
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(activity1, activity1, mNextTabPolicySupplier, 1);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertEquals(1, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 1, mSubject.getIndexForWindow(activity1));

        destroyActivity(activity1);

        Assert.assertEquals("Still found model", TabWindowManager.INVALID_WINDOW_INDEX,
                mSubject.getIndexForWindow(activity1));

        ChromeActivity activity2 = buildActivity();
        Pair<Integer, TabModelSelector> assignment2 =
                mSubject.requestSelector(activity2, activity2, mNextTabPolicySupplier, 1);

        Assert.assertEquals(1, assignment2.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment2.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 1, mSubject.getIndexForWindow(activity2));

        destroyActivity(activity0);
        destroyActivity(activity2);
    }

    /**
     * Tests that tabExistsInAnySelector() functions properly.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testTabExistsInAnySelector() {
        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(activity0, activity0, mNextTabPolicySupplier, 0);
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(activity1, activity1, mNextTabPolicySupplier, 1);
        MockTabModelSelector selector0 = (MockTabModelSelector) assignment0.second;
        MockTabModelSelector selector1 = (MockTabModelSelector) assignment1.second;
        Tab tab1 = selector0.addMockTab();
        Tab tab2 = selector1.addMockIncognitoTab();

        Assert.assertNull(mSubject.getTabById(tab1.getId() - 1));
        Assert.assertNotNull(mSubject.getTabById(tab1.getId()));
        Assert.assertNotNull(mSubject.getTabById(tab2.getId()));
        Assert.assertNull(mSubject.getTabById(tab2.getId() + 1));

        mAsyncTabParamsManager.getAsyncTabParams().clear();
        final int asyncTabId = 123;
        final TabReparentingParams dummyParams =
                new TabReparentingParams(new MockTab(0, false), null);
        Assert.assertNull(mSubject.getTabById(asyncTabId));
        mAsyncTabParamsManager.add(asyncTabId, dummyParams);
        try {
            Assert.assertNotNull(mSubject.getTabById(asyncTabId));
        } finally {
            mAsyncTabParamsManager.getAsyncTabParams().clear();
        }

        destroyActivity(activity0);
        destroyActivity(activity1);
    }

    /**
     * Tests that getTabById() functions properly.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void testGetTabById() {
        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(activity0, activity0, mNextTabPolicySupplier, 0);
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(activity1, activity1, mNextTabPolicySupplier, 1);
        MockTabModelSelector selector0 = (MockTabModelSelector) assignment0.second;
        MockTabModelSelector selector1 = (MockTabModelSelector) assignment1.second;
        Tab tab1 = selector0.addMockTab();
        Tab tab2 = selector1.addMockIncognitoTab();

        Assert.assertNull(mSubject.getTabById(tab1.getId() - 1));
        Assert.assertNotNull(mSubject.getTabById(tab1.getId()));
        Assert.assertNotNull(mSubject.getTabById(tab2.getId()));
        Assert.assertNull(mSubject.getTabById(tab2.getId() + 1));

        mAsyncTabParamsManager.getAsyncTabParams().clear();
        final int asyncTabId = 123;
        final TabReparentingParams dummyParams =
                new TabReparentingParams(new MockTab(0, false), null);
        Assert.assertNull(mSubject.getTabById(asyncTabId));
        mAsyncTabParamsManager.add(asyncTabId, dummyParams);
        try {
            Assert.assertNotNull(mSubject.getTabById(asyncTabId));
        } finally {
            mAsyncTabParamsManager.getAsyncTabParams().clear();
        }

        destroyActivity(activity0);
        destroyActivity(activity1);
    }

    /**
     * Tests that getTabModelForTab(...) functions properly.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    @UiThreadTest
    public void getTabModelForTab() {
        ChromeActivity activity0 = buildActivity();
        ChromeActivity activity1 = buildActivity();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(activity0, activity0, mNextTabPolicySupplier, 0);
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(activity1, activity1, mNextTabPolicySupplier, 1);
        MockTabModelSelector selector0 = (MockTabModelSelector) assignment0.second;
        MockTabModelSelector selector1 = (MockTabModelSelector) assignment1.second;
        Tab tab1 = selector0.addMockTab();
        Tab tab2 = selector1.addMockTab();
        Tab tab3 = selector0.addMockIncognitoTab();
        Tab tab4 = selector1.addMockIncognitoTab();

        Assert.assertEquals(
                selector0.getModel(/* incognito= */ false), mSubject.getTabModelForTab(tab1));
        Assert.assertEquals(
                selector1.getModel(/* incognito= */ false), mSubject.getTabModelForTab(tab2));
        Assert.assertEquals(
                selector0.getModel(/* incognito= */ true), mSubject.getTabModelForTab(tab3));
        Assert.assertEquals(
                selector1.getModel(/* incognito= */ true), mSubject.getTabModelForTab(tab4));

        destroyActivity(activity0);
        destroyActivity(activity1);
    }
}
