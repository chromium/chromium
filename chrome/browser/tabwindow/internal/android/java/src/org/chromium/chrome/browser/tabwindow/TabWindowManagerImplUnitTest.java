// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabwindow;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.tabwindow.TabWindowManager.INVALID_WINDOW_ID;

import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.util.Pair;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.InstanceInfo;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManagerFactory;
import org.chromium.chrome.browser.tabmodel.MismatchedIndicesHandler;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabReparentingParams;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Test for {@link TabWindowManagerImpl}.
 *
 * <p>Makes sure the class handles multiple {@link Activity}s requesting {@link TabModelSelector}s,
 * {@link Activity}s getting destroyed, etc.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH)
public class TabWindowManagerImplUnitTest {
    private static final Token GROUP_ID = new Token(12, 34);
    private static final int TAB_ID = 2;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final NextTabPolicySupplier mNextTabPolicySupplier = () -> NextTabPolicy.HIERARCHICAL;

    @Mock private ProfileProvider mProfileProvider;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private MismatchedIndicesHandler mMismatchedIndicesHandler0;
    @Mock private MismatchedIndicesHandler mMismatchedIndicesHandler1;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    @Mock private TabModelSelector mArchivedTabModelSelector;
    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private TabModelSelectorFactory mTabModelSelectorFactory;
    @Mock private Destroyable mDestroyable;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private TabGroupSyncService mTabGroupSyncService;

    private OneshotSupplierImpl<ProfileProvider> mProfileProviderSupplier;
    private AsyncTabParamsManager mAsyncTabParamsManager;
    private TabWindowManager mSubject;

    @Before
    public void setUp() {
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        mProfileProviderSupplier = new OneshotSupplierImpl<>();
        mProfileProviderSupplier.set(mProfileProvider);

        TabModelSelectorFactory mockTabModelSelectorFactory =
                new TabModelSelectorFactory() {
                    @Override
                    public TabModelSelector buildTabbedSelector(
                            Context context,
                            ModalDialogManager modalDialogManager,
                            OneshotSupplier<ProfileProvider> profileProviderSupplier,
                            TabCreatorManager tabCreatorManager,
                            NextTabPolicySupplier nextTabPolicySupplier) {
                        return new MockTabModelSelector(
                                mProfile,
                                mIncognitoProfile,
                                /* tabCount= */ 0,
                                /* incognitoTabCount= */ 0,
                                /* delegate= */ null);
                    }

                    @Override
                    public Pair<TabModelSelector, Destroyable> buildHeadlessSelector(
                            @WindowId int windowId, Profile profile) {
                        return Pair.create(
                                new MockTabModelSelector(
                                        mProfile,
                                        mIncognitoProfile,
                                        /* tabCount= */ 0,
                                        /* incognitoTabCount= */ 0,
                                        /* delegate= */ null),
                                () -> {});
                    }
                };

        mSubject = createTabWindowManager(mockTabModelSelectorFactory);
    }

    private ActivityController<Activity> createActivity() {
        ActivityController<Activity> controller = Robolectric.buildActivity(Activity.class);
        controller.setup();
        return controller;
    }

    private void destroyActivity(ActivityController<Activity> controller) {
        controller.destroy();
    }

    private TabWindowManager createTabWindowManager(
            TabModelSelectorFactory tabModelSelectorFactory) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAsyncTabParamsManager =
                            AsyncTabParamsManagerFactory.createAsyncTabParamsManager();
                    int maxInstances =
                            (Build.VERSION.SDK_INT >= VERSION_CODES.S
                                    ? TabWindowManager.MAX_SELECTORS_S
                                    : TabWindowManager.MAX_SELECTORS_LEGACY);
                    return TabWindowManagerFactory.createInstance(
                            tabModelSelectorFactory, mAsyncTabParamsManager, maxInstances);
                });
    }

    /** Test that a single {@link Activity} can request a {@link TabModelSelector}. */
    @Test
    @Feature({"Multiwindow"})
    public void testSingleActivity() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        Pair<@WindowId Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        0);

        assertEquals(0, assignment0.first.intValue());
        TabModelSelector selector0 = assignment0.second;
        assertNotNull("Was not able to build the TabModelSelector", selector0);
        assertEquals("Unexpected window id", 0, mSubject.getIdForWindow(activity0));

        destroyActivity(activityController0);
    }

    /** Test that two {@link Activity}s can request different {@link TabModelSelector}s. */
    @Test
    @Feature({"Multiwindow"})
    public void testMultipleActivities() {
        assertTrue("Not enough selectors", mSubject.getMaxSimultaneousSelectors() >= 2);

        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<@WindowId Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        0);
        Pair<@WindowId Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler1,
                        1);

        assertEquals(0, assignment0.first.intValue());
        assertEquals(1, assignment1.first.intValue());
        assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        assertEquals("Unexpected window id", 0, mSubject.getIdForWindow(activity0));
        assertEquals("Unexpected window id", 1, mSubject.getIdForWindow(activity1));

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }

    /**
     * Test that trying to have too many {@link Activity}s requesting {@link TabModelSelector}s is
     * properly capped and returns {@code null}.
     */
    @Test
    @Feature({"Multiwindow"})
    public void testTooManyActivities() {
        List<ActivityController<Activity>> activityControllerList = new ArrayList<>();
        for (int i = 0; i < mSubject.getMaxSimultaneousSelectors(); i++) {
            ActivityController<Activity> c = createActivity();
            activityControllerList.add(c);
            assertNotNull(
                    "Could not build selector",
                    mSubject.requestSelector(
                            c.get(),
                            mModalDialogManager,
                            mProfileProviderSupplier,
                            mTabCreatorManager,
                            mNextTabPolicySupplier,
                            mMismatchedIndicesHandler0,
                            0));
        }

        ActivityController<Activity> activityController = createActivity();
        activityControllerList.add(activityController);
        assertNull(
                "Built selectors past the max number supported",
                mSubject.requestSelector(
                        activityController.get(),
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        0));

        for (ActivityController<Activity> c : activityControllerList) {
            destroyActivity(c);
        }
    }

    /**
     * Test that requesting the same {@link TabModelSelector} window id will fall back and return a
     * model for a different available window id instead. In this case, a higher window id (0 -> 1).
     */
    @Test
    @Feature({"Multiwindow"})
    public void testWindowIdFallback() {
        assertTrue("Not enough selectors", mSubject.getMaxSimultaneousSelectors() >= 2);

        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<@WindowId Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        0);
        // Request 0 again, but should get 1 instead.
        Pair<@WindowId Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler1,
                        0);

        assertEquals(0, assignment0.first.intValue());
        assertEquals(1, assignment1.first.intValue());
        assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        assertEquals("Unexpected window id", 0, mSubject.getIdForWindow(activity0));
        assertEquals("Unexpected window id", 1, mSubject.getIdForWindow(activity1));

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }

    /**
     * Test that requesting the same {@link TabModelSelector} window id will fall back and return a
     * model for a different available window id instead. In this case, a lower window id (2 -> 0).
     */
    @Test
    @Feature({"Multiwindow"})
    public void testWindowIdFallback2() {
        assertTrue("Not enough selectors", mSubject.getMaxSimultaneousSelectors() >= 3);

        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<@WindowId Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        2);
        // Request 2 again, but should get 0 instead.
        Pair<@WindowId Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler1,
                        2);

        assertEquals(2, assignment0.first.intValue());
        assertEquals(0, assignment1.first.intValue());
        assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        assertEquals("Unexpected window id", 2, mSubject.getIdForWindow(activity0));
        assertEquals("Unexpected window id", 0, mSubject.getIdForWindow(activity1));

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }

    /**
     * Test that a destroyed {@link Activity} properly gets removed from {@link
     * TabWindowManagerImpl}.
     */
    @Test
    @Feature({"Multiwindow"})
    public void testActivityDeathRemovesSingle() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        Pair<@WindowId Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        0);

        assertEquals(0, assignment0.first.intValue());
        assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        assertEquals("Unexpected window id", 0, mSubject.getIdForWindow(activity0));

        destroyActivity(activityController0);

        assertEquals("Still found model", INVALID_WINDOW_ID, mSubject.getIdForWindow(activity0));
    }

    /**
     * Test that an {@link Activity} requesting an window id that was previously assigned to a
     * destroyed {@link Activity} can take that {@link TabModelSelector}.
     */
    @Test
    @Feature({"Multiwindow"})
    public void testActivityDeathLetsModelReassign() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        Pair<@WindowId Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        0);

        assertEquals(0, assignment0.first.intValue());
        assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        assertEquals("Unexpected window id", 0, mSubject.getIdForWindow(activity0));

        destroyActivity(activityController0);

        assertEquals("Still found model", INVALID_WINDOW_ID, mSubject.getIdForWindow(activity0));

        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<@WindowId Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler1,
                        0);

        assertEquals(0, assignment1.first.intValue());
        assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        assertEquals("Unexpected window id", 0, mSubject.getIdForWindow(activity1));

        destroyActivity(activityController1);
    }

    /**
     * Test that an {@link Activity} requesting an window id that was previously assigned to a
     * destroyed {@link Activity} can take that {@link TabModelSelector} when there are other {@link
     * Activity}s assigned {@link TabModelSelector}s.
     */
    @Test
    @Feature({"Multiwindow"})
    public void testActivityDeathWithMultipleActivities() {
        assertTrue("Not enough selectors", mSubject.getMaxSimultaneousSelectors() >= 2);

        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<@WindowId Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        0);
        Pair<@WindowId Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler1,
                        1);

        assertEquals(0, assignment0.first.intValue());
        assertEquals(1, assignment1.first.intValue());
        assertNotNull("Was not able to build the TabModelSelector", assignment0.second);
        assertNotNull("Was not able to build the TabModelSelector", assignment1.second);
        assertEquals("Unexpected window id", 0, mSubject.getIdForWindow(activity0));
        assertEquals("Unexpected window id", 1, mSubject.getIdForWindow(activity1));

        destroyActivity(activityController1);

        assertEquals("Still found model", INVALID_WINDOW_ID, mSubject.getIdForWindow(activity1));

        ActivityController<Activity> activityController2 = createActivity();
        Activity activity2 = activityController2.get();
        MismatchedIndicesHandler handler = mock(MismatchedIndicesHandler.class);
        Pair<@WindowId Integer, TabModelSelector> assignment2 =
                mSubject.requestSelector(
                        activity2,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        handler,
                        1);

        assertEquals(1, assignment2.first.intValue());
        assertNotNull("Was not able to build the TabModelSelector", assignment2.second);
        assertEquals("Unexpected window id", 0, mSubject.getIdForWindow(activity0));
        assertEquals("Unexpected window id", 1, mSubject.getIdForWindow(activity2));

        destroyActivity(activityController0);
        destroyActivity(activityController2);
    }

    /** Tests that tabExistsInAnySelector() functions properly. */
    @Test
    @Feature({"Multiwindow"})
    public void testTabExistsInAnySelector() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<@WindowId Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        0);
        Pair<@WindowId Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler1,
                        1);
        MockTabModelSelector selector0 = (MockTabModelSelector) assignment0.second;
        MockTabModelSelector selector1 = (MockTabModelSelector) assignment1.second;
        Tab tab1 = selector0.addMockTab();
        Tab tab2 = selector1.addMockIncognitoTab();

        assertNull(mSubject.getTabById(tab1.getId() - 1));
        assertNotNull(mSubject.getTabById(tab1.getId()));
        assertNotNull(mSubject.getTabById(tab2.getId()));
        assertNull(mSubject.getTabById(tab2.getId() + 1));

        mAsyncTabParamsManager.getAsyncTabParams().clear();
        final @TabId int asyncTabId = 123;
        final TabReparentingParams placeholderParams =
                new TabReparentingParams(new MockTab(0, mProfile), null);
        assertNull(mSubject.getTabById(asyncTabId));
        mAsyncTabParamsManager.add(asyncTabId, placeholderParams);
        try {
            assertNotNull(mSubject.getTabById(asyncTabId));
        } finally {
            mAsyncTabParamsManager.getAsyncTabParams().clear();
        }

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }

    /** Tests that getTabById() functions properly. */
    @Test
    @Feature({"Multiwindow"})
    public void testGetTabById() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<@WindowId Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        0);
        Pair<@WindowId Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler1,
                        1);
        MockTabModelSelector selector0 = (MockTabModelSelector) assignment0.second;
        MockTabModelSelector selector1 = (MockTabModelSelector) assignment1.second;
        Tab tab1 = selector0.addMockTab();
        Tab tab2 = selector1.addMockIncognitoTab();

        assertNull(mSubject.getTabById(tab1.getId() - 1));
        assertNotNull(mSubject.getTabById(tab1.getId()));
        assertNotNull(mSubject.getTabById(tab2.getId()));
        assertNull(mSubject.getTabById(tab2.getId() + 1));

        mAsyncTabParamsManager.getAsyncTabParams().clear();
        final @TabId int asyncTabId = 123;
        final TabReparentingParams placeholderParams =
                new TabReparentingParams(new MockTab(0, mProfile), null);
        assertNull(mSubject.getTabById(asyncTabId));
        mAsyncTabParamsManager.add(asyncTabId, placeholderParams);
        try {
            assertNotNull(mSubject.getTabById(asyncTabId));
        } finally {
            mAsyncTabParamsManager.getAsyncTabParams().clear();
        }

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }

    /** Tests that getTabModelForTab(...) functions properly. */
    @Test
    @Feature({"Multiwindow"})
    public void getTabModelForTab() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        Pair<@WindowId Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        0);
        Pair<@WindowId Integer, TabModelSelector> assignment1 =
                mSubject.requestSelector(
                        activity1,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler1,
                        1);
        MockTabModelSelector selector0 = (MockTabModelSelector) assignment0.second;
        MockTabModelSelector selector1 = (MockTabModelSelector) assignment1.second;
        Tab tab1 = selector0.addMockTab();
        Tab tab2 = selector1.addMockTab();
        Tab tab3 = selector0.addMockIncognitoTab();
        Tab tab4 = selector1.addMockIncognitoTab();

        assertEquals(selector0.getModel(/* incognito= */ false), mSubject.getTabModelForTab(tab1));
        assertEquals(selector1.getModel(/* incognito= */ false), mSubject.getTabModelForTab(tab2));
        assertEquals(selector0.getModel(/* incognito= */ true), mSubject.getTabModelForTab(tab3));
        assertEquals(selector1.getModel(/* incognito= */ true), mSubject.getTabModelForTab(tab4));

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }

    @Test
    @Config(sdk = VERSION_CODES.Q)
    public void testAssertIndicesMismatch() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        mSubject.requestSelector(
                activity0,
                mModalDialogManager,
                mProfileProviderSupplier,
                mTabCreatorManager,
                mNextTabPolicySupplier,
                mMismatchedIndicesHandler0,
                0);

        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        TabWindowManager.ASSERT_INDICES_MATCH_HISTOGRAM_NAME
                                + TabWindowManager
                                        .ASSERT_INDICES_MATCH_HISTOGRAM_SUFFIX_NOT_REASSIGNED)) {
            mSubject.requestSelector(
                    activity1,
                    mModalDialogManager,
                    mProfileProviderSupplier,
                    mTabCreatorManager,
                    mNextTabPolicySupplier,
                    mMismatchedIndicesHandler1,
                    0);
        } finally {
            destroyActivity(activityController1);
        }

        String umaPreExistingActivityDestroyed =
                "Android.MultiWindowMode.AssertIndicesMatch.PreExistingActivityDestroyed";
        String umaTimeToPreExistingActivityDestruction =
                "Android.MultiWindowMode.MismatchedIndices.TimeToPreExistingActivityDestruction";
        try (var ignored =
                HistogramWatcher.newBuilder()
                        .expectAnyRecord(umaPreExistingActivityDestroyed)
                        .expectAnyRecord(umaTimeToPreExistingActivityDestruction)
                        .build()) {
            destroyActivity(activityController0);
        }
    }

    @Test
    @Config(sdk = VERSION_CODES.Q)
    public void testWindowIdReassignmentWhenIndicesMismatch() {
        // Simulate successful window id mismatch handling, that will trigger reassignment.
        when(mMismatchedIndicesHandler1.handleMismatchedIndices(any(), anyBoolean(), anyBoolean()))
                .thenReturn(true);

        // Create activity0 and request its tab model selector to use window id 0.
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        mSubject.requestSelector(
                activity0,
                mModalDialogManager,
                mProfileProviderSupplier,
                mTabCreatorManager,
                mNextTabPolicySupplier,
                mMismatchedIndicesHandler0,
                0);

        // Create activity1 and request its tab model selector to use window id 0.
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();

        try (var ignored =
                HistogramWatcher.newSingleRecordWatcher(
                        TabWindowManager.ASSERT_INDICES_MATCH_HISTOGRAM_NAME
                                + TabWindowManager
                                        .ASSERT_INDICES_MATCH_HISTOGRAM_SUFFIX_REASSIGNED)) {
            var assignment =
                    mSubject.requestSelector(
                            activity1,
                            mModalDialogManager,
                            mProfileProviderSupplier,
                            mTabCreatorManager,
                            mNextTabPolicySupplier,
                            mMismatchedIndicesHandler1,
                            0);
            assertEquals(
                    "Requested selector's window id assignment is incorrect.",
                    0,
                    (int) assignment.first);
        }

        // activity0's window id 0 assignment should be cleared and activity1 should be able to use
        // the
        // requested window id 0.
        assertEquals(
                "Window Id for activity0 should be cleared.",
                INVALID_WINDOW_ID,
                mSubject.getIdForWindow(activity0));
        assertEquals(
                "Requested window id for activity1 should be used.",
                0,
                mSubject.getIdForWindow(activity1));

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }

    @Test
    @Config(sdk = VERSION_CODES.Q)
    public void testWindowIdReassignmentSkipped() {
        // Simulate need for skipping reassignment.
        when(mMismatchedIndicesHandler1.skipIndexReassignment()).thenReturn(true);

        // Create activity0 and request its tab model selector to use window id 0.
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        mSubject.requestSelector(
                activity0,
                mModalDialogManager,
                mProfileProviderSupplier,
                mTabCreatorManager,
                mNextTabPolicySupplier,
                mMismatchedIndicesHandler0,
                0);

        // Create activity1 and request its tab model selector to use window id 0, but it should be
        // assigned 1 instead, since reassignment on conflict should be ignored.
        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        var assignment =
                mSubject.requestSelector(
                        activity1,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler1,
                        0);
        assertEquals(
                "Requested selector's window id assignment is incorrect.",
                1,
                (int) assignment.first);
        verify(mMismatchedIndicesHandler1, never())
                .handleMismatchedIndices(any(), anyBoolean(), anyBoolean());

        destroyActivity(activityController0);
        destroyActivity(activityController1);
    }

    @Test
    public void testCanTabStateBeDeleted_ArchiveDisabled() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabStateCleanupAbortedByArchive", false);
        assertTrue(mSubject.canTabStateBeDeleted(0));
        histogramWatcher.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH)
    public void testCanTabStateBeDeleted() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        Pair<@WindowId Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        0);

        assertEquals(0, assignment0.first.intValue());
        MockTabModelSelector selector0 = (MockTabModelSelector) assignment0.second;

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabStateCleanupAbortedByArchive", true);
        // First check if a non-existent tab can be deleted when the archived tab model is
        // null.
        assertFalse(mSubject.canTabStateBeDeleted(0));
        histogramWatcher.assertExpected();

        // Next set the archived tab model, but simulate like it hasn't finished loading.
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabStateCleanupAbortedByArchive", true);
        mSubject.setArchivedTabModelSelector(mArchivedTabModelSelector);
        doReturn(false).when(mArchivedTabModelSelector).isTabStateInitialized();
        assertFalse(mSubject.canTabStateBeDeleted(0));
        histogramWatcher.assertExpected();

        // Next simulate the archived tab model being loaded. This should call through to
        // #getTabById, but there is no tab.
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabStateCleanupAbortedByArchive", false);
        doReturn(true).when(mArchivedTabModelSelector).isTabStateInitialized();
        assertTrue(mSubject.canTabStateBeDeleted(0));
        histogramWatcher.assertExpected();

        // Now a tab exists, so it shouldn't be deletable.
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabStateCleanupAbortedByArchive", false);
        Tab tab1 = selector0.addMockTab();
        assertFalse(mSubject.canTabStateBeDeleted(tab1.getId()));
        histogramWatcher.assertExpected();

        // Simulate moving it to the archived model.
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabStateCleanupAbortedByArchive", false);
        doReturn(tab1).when(mArchivedTabModelSelector).getTabById(tab1.getId());
        selector0.tryCloseTab(
                TabClosureParams.closeTab(tab1).allowUndo(false).build(), /* allowDialog= */ false);
        assertFalse(mSubject.canTabStateBeDeleted(tab1.getId()));
        histogramWatcher.assertExpected();

        destroyActivity(activityController0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER_RESCUE_KILLSWITCH)
    public void testCanTabThumbnailBeDeleted() {
        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        Pair<@WindowId Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity0,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        0);

        assertEquals(0, assignment0.first.intValue());
        MockTabModelSelector selector0 = (MockTabModelSelector) assignment0.second;

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabThumbnailCleanupAbortedByArchive", true);
        // First check if a non-existent tab can be deleted when the archived tab model is
        // null.
        assertFalse(mSubject.canTabThumbnailBeDeleted(0));
        histogramWatcher.assertExpected();

        // Next set the archived tab model, but simulate like it hasn't finished loading.
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabThumbnailCleanupAbortedByArchive", true);
        mSubject.setArchivedTabModelSelector(mArchivedTabModelSelector);
        doReturn(false).when(mArchivedTabModelSelector).isTabStateInitialized();
        assertFalse(mSubject.canTabThumbnailBeDeleted(0));
        histogramWatcher.assertExpected();

        // Next simulate the archived tab model being loaded. This should call through to
        // #getTabById, but there is no tab.
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabThumbnailCleanupAbortedByArchive", false);
        doReturn(true).when(mArchivedTabModelSelector).isTabStateInitialized();
        assertTrue(mSubject.canTabThumbnailBeDeleted(0));
        histogramWatcher.assertExpected();

        // Now a tab exists, so it shouldn't be deletable.
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabThumbnailCleanupAbortedByArchive", false);
        Tab tab1 = selector0.addMockTab();
        assertFalse(mSubject.canTabThumbnailBeDeleted(tab1.getId()));
        histogramWatcher.assertExpected();

        // Simulate moving it to the archived model.
        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.TabThumbnailCleanupAbortedByArchive", false);
        doReturn(tab1).when(mArchivedTabModelSelector).getTabById(tab1.getId());
        selector0.tryCloseTab(
                TabClosureParams.closeTab(tab1).allowUndo(false).build(), /* allowDialog= */ false);
        assertFalse(mSubject.canTabThumbnailBeDeleted(tab1.getId()));
        histogramWatcher.assertExpected();

        destroyActivity(activityController0);
    }

    @Test
    public void testRequestSelectorWithoutActivity() {
        // Request calls for the same window id should reuse the same selector.
        TabModelSelector selector1 = mSubject.requestSelectorWithoutActivity(0, mProfile);
        TabModelSelector selector2 = mSubject.requestSelectorWithoutActivity(0, mProfile);
        assertEquals(selector1, selector2);

        // Shutting down the selector will cause the next request to generate a new selector.
        mSubject.shutdownIfHeadless(0);
        TabModelSelector selector3 = mSubject.requestSelectorWithoutActivity(0, mProfile);
        assertNotEquals(selector1, selector3);

        // Making an activity will shutdown the old headless selector and make a tabbed selector.
        ActivityController<Activity> activityController = createActivity();
        Activity activity = activityController.get();
        Pair<@WindowId Integer, TabModelSelector> assignment0 =
                mSubject.requestSelector(
                        activity,
                        mModalDialogManager,
                        mProfileProviderSupplier,
                        mTabCreatorManager,
                        mNextTabPolicySupplier,
                        mMismatchedIndicesHandler0,
                        0);
        TabModelSelector selector4 = assignment0.second;
        assertNotEquals(selector3, selector4);

        // This selector should reuse the tabbed selector created by the activity.
        TabModelSelector selector5 = mSubject.requestSelectorWithoutActivity(0, mProfile);
        assertEquals(selector4, selector5);

        // Shutdown should now no-op because the backing selector is tabbed.
        mSubject.shutdownIfHeadless(0);
        TabModelSelector selector6 = mSubject.requestSelectorWithoutActivity(0, mProfile);
        assertEquals(selector5, selector6);

        // Destroying the activity should result in a new (headless) selector when requested.
        destroyActivity(activityController);
        TabModelSelector selector7 = mSubject.requestSelectorWithoutActivity(0, mProfile);
        assertNotEquals(selector6, selector7);

        // Different window ids gets different selectors.
        TabModelSelector selector8 = mSubject.requestSelectorWithoutActivity(1, mProfile);
        assertNotEquals(selector7, selector8);
    }

    @Test
    public void testKeepAllTabModelsLoaded() {
        List<InstanceInfo> instanceInfoList = new ArrayList<>();
        instanceInfoList.add(
                new InstanceInfo(0, 0, InstanceInfo.Type.OTHER, "", "", 0, 0, false, 0));
        instanceInfoList.add(
                new InstanceInfo(1, 0, InstanceInfo.Type.OTHER, "", "", 0, 0, false, 0));
        instanceInfoList.add(
                new InstanceInfo(2, 0, InstanceInfo.Type.OTHER, "", "", 0, 0, false, 0));
        when(mMultiInstanceManager.getInstanceInfo()).thenReturn(instanceInfoList);

        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        mSubject.requestSelector(
                activity0,
                mModalDialogManager,
                mProfileProviderSupplier,
                mTabCreatorManager,
                mNextTabPolicySupplier,
                mMismatchedIndicesHandler0,
                0);

        assertEquals(1, mSubject.getAllTabModelSelectors().size());

        mSubject.keepAllTabModelsLoaded(mMultiInstanceManager, mProfile, mTabModelSelector);
        assertEquals(3, mSubject.getAllTabModelSelectors().size());

        ActivityController<Activity> activityController1 = createActivity();
        Activity activity1 = activityController1.get();
        mSubject.requestSelector(
                activity1,
                mModalDialogManager,
                mProfileProviderSupplier,
                mTabCreatorManager,
                mNextTabPolicySupplier,
                mMismatchedIndicesHandler0,
                1);

        mSubject.keepAllTabModelsLoaded(mMultiInstanceManager, mProfile, mTabModelSelector);
        assertEquals(3, mSubject.getAllTabModelSelectors().size());

        destroyActivity(activityController1);
        assertEquals(3, mSubject.getAllTabModelSelectors().size());

        // Shutting down the last activity shouldn't trigger any headless init.
        destroyActivity(activityController0);
        assertEquals(2, mSubject.getAllTabModelSelectors().size());
    }

    @Test
    public void testKeepAllTabModelsLoaded_broadcast() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        List<InstanceInfo> instanceInfoList = new ArrayList<>();
        instanceInfoList.add(
                new InstanceInfo(0, 0, InstanceInfo.Type.OTHER, "", "", 0, 0, false, 0));
        when(mMultiInstanceManager.getInstanceInfo()).thenReturn(instanceInfoList);

        // The default mock TabModelSelectorFactory is hard to verify
        // broadcastSessionRestoreComplete with. So this test creates just enough to verify it
        // grabs a random selector and broadcasts.
        when(mTabModelSelectorFactory.buildHeadlessSelector(anyInt(), any()))
                .thenReturn(new Pair<>(mTabModelSelector, mDestroyable));
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getModel(anyBoolean())).thenReturn(mTabModel);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(anyBoolean()))
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabGroupModelFilter.getRootIdFromTabGroupId(GROUP_ID)).thenReturn(TAB_ID);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        TabWindowManager tabWindowManager = createTabWindowManager(mTabModelSelectorFactory);

        tabWindowManager.keepAllTabModelsLoaded(mMultiInstanceManager, mProfile, mTabModelSelector);
        ShadowLooper.runUiThreadTasks();
        verify(mTabModel).broadcastSessionRestoreComplete();
    }

    @Test
    public void testKeepAllTabModelsLoaded_fallback() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getModel(anyBoolean())).thenReturn(mTabModel);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        // This is the behavior a pre-31 device would exhibit.
        when(mMultiInstanceManager.getInstanceInfo()).thenReturn(Collections.emptyList());

        ActivityController<Activity> activityController0 = createActivity();
        Activity activity0 = activityController0.get();
        mSubject.requestSelector(
                activity0,
                mModalDialogManager,
                mProfileProviderSupplier,
                mTabCreatorManager,
                mNextTabPolicySupplier,
                mMismatchedIndicesHandler0,
                0);
        assertEquals(1, mSubject.getAllTabModelSelectors().size());

        mSubject.keepAllTabModelsLoaded(mMultiInstanceManager, mProfile, mTabModelSelector);
        assertEquals(1, mSubject.getAllTabModelSelectors().size());
        ShadowLooper.runUiThreadTasks();
        verify(mTabModel).broadcastSessionRestoreComplete();
    }

    @Test
    public void testFindWindowIdForTabGroup_found() {
        when(mTabModelSelectorFactory.buildHeadlessSelector(anyInt(), any()))
                .thenReturn(new Pair<>(mTabModelSelector, mDestroyable));
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getTabGroupModelFilter(anyBoolean()))
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getRootIdFromTabGroupId(GROUP_ID)).thenReturn(TAB_ID);
        TabWindowManager tabWindowManager = createTabWindowManager(mTabModelSelectorFactory);
        tabWindowManager.requestSelectorWithoutActivity(1, mProfile);
        assertEquals(1, tabWindowManager.findWindowIdForTabGroup(GROUP_ID));
    }

    @Test
    public void testFindWindowIdForTabGroup_notFound() {
        assertEquals(INVALID_WINDOW_ID, mSubject.findWindowIdForTabGroup(GROUP_ID));
    }
}
