// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;

import java.util.ArrayList;
import java.util.List;

/**
 * Test for {@link TabWindowManagerImpl}.
 *
 * <p>Makes sure the class handles multiple {@link Activity}s requesting {@link TabModelSelector}s,
 * {@link Activity}s getting destroyed, etc.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabWindowManagerTest {
    private TabWindowManager mSubject;
    private AsyncTabParamsManager mAsyncTabParamsManager;
    @Mock private ProfileProvider mProfileProvider;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    private NextTabPolicySupplier mNextTabPolicySupplier = () -> NextTabPolicy.HIERARCHICAL;
    private OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier =
            new OneshotSupplierImpl<>();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Mockito.when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        mProfileProviderSupplier.set(mProfileProvider);

        TabModelSelectorFactory mockTabModelSelectorFactory =
                new TabModelSelectorFactory() {
                    @Override
                    public TabModelSelector buildSelector(
                            Context context,
                            OneshotSupplier<ProfileProvider> profileProviderSupplier,
                            TabCreatorManager tabCreatorManager,
                            NextTabPolicySupplier nextTabPolicySupplier) {
                        return new MockTabModelSelector(mProfile, mIncognitoProfile, 0, 0, null);
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAsyncTabParamsManager =
                            AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
                    int maxInstances =
                            (Build.VERSION.SDK_INT >= 31 /*S*/
                                    ? TabWindowManager.MAX_SELECTORS_S
                                    : TabWindowManager.MAX_SELECTORS_LEGACY);
                    mSubject =
                            TabWindowManagerFactory.createInstance(
                                    mockTabModelSelectorFactory,
                                    mAsyncTabParamsManager,
                                    maxInstances);
                });
    }

    private ActivityController<Activity> createActivity() {
        ActivityController<Activity> controller = Robolectric.buildActivity(Activity.class);
        controller.setup();
        return controller;
    }

    private void destroyActivity(ActivityController<Activity> controller) {
        controller.destroy();
    }

    /** Test that a single {@link Activity} can request a {@link TabModelSelector}. */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    public void testSingleActivity() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        0);

        Assert.assertEquals(0, assignment0.first.intValue());
        TabModelSelector selector0 = assignment0.second;
        Assert.assertNotNull("Was not able to build the TabModelSelector", selector0);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));

        destroyActivity(activityController0);
    }

    /** Test that two {@link Activity}s can request different {@link TabModelSelector}s. */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    public void testMultipleActivities() {
        Assert.assertTrue("Not enough selectors", mSubject.getMaxSimultaneousSelectors() >= 2);

        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        0);
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        1);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertEquals(1, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 1, mSubject.getIndexForWindow(activity1));

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }

    /**
     * Test that trying to have too many {@link Activity}s requesting {@link TabModelSelector}s is
     * properly capped and returns {@code null}.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    public void testTooManyActivities() {
        List<ActivityController<Activity>> activityControllerList = new ArrayList<>();
        for (int i = 0; i < mSubject.getMaxSimultaneousSelectors(); i++) {
            ActivityController<Activity> c = createActivity();
            activityControllerList.add(c);
            Assert.assertNotNull(
                    "Could not build selector",
                    mSubject.requestSelector(
                            c.get(),
                            mProfileProviderSupplier,
                            mTabCreatorManager,
                            mNextTabPolicySupplier,
                            0));
        }

        ActivityController<Activity> activityController = createActivity();
        activityControllerList.add(activityController);
        Assert.assertNull(
                "Built selectors past the max number supported",
                mSubject.requestSelector(
                        activityController.get(),
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        0));

        for (ActivityController<Activity> c : activityControllerList) {
            destroyActivity(c);
        }
    }

    /**
     * Test that requesting the same {@link TabModelSelector} index will fall back and return a
     * model for a different available index instead. In this case, a higher index (0 -> 1).
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    public void testIndexFallback() {
        Assert.assertTrue("Not enough selectors", mSubject.getMaxSimultaneousSelectors() >= 2);

        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        0);
        // Request 0 again, but should get 1 instead.
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        0);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertEquals(1, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 1, mSubject.getIndexForWindow(activity1));

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }

    /**
     * Test that requesting the same {@link TabModelSelector} index will fall back and return a
     * model for a different available index instead. In this case, a lower index (2 -> 0).
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    public void testIndexFallback2() {
        Assert.assertTrue("Not enough selectors", mSubject.getMaxSimultaneousSelectors() >= 3);

        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        2);
        // Request 2 again, but should get 0 instead.
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        2);

        Assert.assertEquals(2, assignment0.first.intValue());
        Assert.assertEquals(0, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 2, mSubject.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity1));

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }

    /**
     * Test that a destroyed {@link Activity} properly gets removed from {@link
     * TabWindowManagerImpl}.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    public void testActivityDeathRemovesSingle() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        0);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));

        destroyActivity(activityController0);

        Assert.assertEquals(
                "Still found model",
                TabWindowManager.INVALID_WINDOW_INDEX,
                mSubject.getIndexForWindow(activity0));
    }

    /**
     * Test that an {@link Activity} requesting an index that was previously assigned to a destroyed
     * {@link Activity} can take that {@link TabModelSelector}.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    public void testActivityDeathLetsModelReassign() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        0);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));

        destroyActivity(activityController0);

        Assert.assertEquals(
                "Still found model",
                TabWindowManager.INVALID_WINDOW_INDEX,
                mSubject.getIndexForWindow(activity0));

        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        0);

        Assert.assertEquals(0, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity1));

        destroyActivity(activityController1);
    }

    /**
     * Test that an {@link Activity} requesting an index that was previously assigned to a destroyed
     * {@link Activity} can take that {@link TabModelSelector} when there are other {@link
     * Activity}s assigned {@link TabModelSelector}s.
     */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    public void testActivityDeathWithMultipleActivities() {
        Assert.assertTrue("Not enough selectors", mSubject.getMaxSimultaneousSelectors() >= 2);

        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        0);
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        1);

        Assert.assertEquals(0, assignment0.first.intValue());
        Assert.assertEquals(1, assignment1.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 1, mSubject.getIndexForWindow(activity1));

        destroyActivity(activityController1);

        Assert.assertEquals(
                "Still found model",
                TabWindowManager.INVALID_WINDOW_INDEX,
                mSubject.getIndexForWindow(activity1));

        ActivityController<Activity> activityController2 = createActivity();
        Activity activity2 = activityController2.get();
        Pair<Integer, TabModelSelector> assignment2 =
                mSubject.requestSelector(
                        activity2,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        1);

        Assert.assertEquals(1, assignment2.first.intValue());
        Assert.assertNotNull("Was not able to build the TabModelSelector", assignment2.second);
        Assert.assertEquals("Unexpected model index", 0, mSubject.getIndexForWindow(activity0));
        Assert.assertEquals("Unexpected model index", 1, mSubject.getIndexForWindow(activity2));

        destroyActivity(activityController0);
        destroyActivity(activityController2);
    }

    /** Tests that tabExistsInAnySelector() functions properly. */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    public void testTabExistsInAnySelector() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        0);
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        1);
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
                new TabReparentingParams(new MockTab(0, mProfile), null);
        Assert.assertNull(mSubject.getTabById(asyncTabId));
        mAsyncTabParamsManager.add(asyncTabId, dummyParams);
        try {
            Assert.assertNotNull(mSubject.getTabById(asyncTabId));
        } finally {
            mAsyncTabParamsManager.getAsyncTabParams().clear();
        }

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }

    /** Tests that getTabById() functions properly. */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    public void testGetTabById() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        0);
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        1);
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
                new TabReparentingParams(new MockTab(0, mProfile), null);
        Assert.assertNull(mSubject.getTabById(asyncTabId));
        mAsyncTabParamsManager.add(asyncTabId, dummyParams);
        try {
            Assert.assertNotNull(mSubject.getTabById(asyncTabId));
        } finally {
            mAsyncTabParamsManager.getAsyncTabParams().clear();
        }

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }

    /** Tests that getTabModelForTab(...) functions properly. */
    @Test
    @SmallTest
    @Feature({"Multiwindow"})
    public void getTabModelForTab() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        0);
        Pair<Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        1);
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

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }
}
