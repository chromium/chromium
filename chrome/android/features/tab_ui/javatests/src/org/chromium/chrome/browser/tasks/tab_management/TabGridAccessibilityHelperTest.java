// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.leaveTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;

import android.content.Context;
import android.content.res.Configuration;
import android.util.Pair;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import androidx.annotation.IntDef;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Tests for reordering tabs in grid tab switcher in accessibility mode. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.PHONE)
@DisableFeatures(ChromeFeatureList.ANDROID_HUB_SEARCH)
@Batch(Batch.PER_CLASS)
public class TabGridAccessibilityHelperTest {
    @IntDef({
        TabMovementDirection.LEFT,
        TabMovementDirection.RIGHT,
        TabMovementDirection.UP,
        TabMovementDirection.DOWN
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabMovementDirection {
        int LEFT = 0;
        int RIGHT = 1;
        int UP = 2;
        int DOWN = 3;
        int NUM_ENTRIES = 4;
    }

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Before
    public void setUp() {
        CriteriaHelper.pollUiThread(
                sActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);

        TabUiTestHelper.getTabSwitcherLayoutAndVerify(sActivityTestRule.getActivity());
    }

    @After
    public void tearDown() {
        ActivityTestUtils.clearActivityOrientation(sActivityTestRule.getActivity());
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        if (cta != null && cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER)) {
            leaveTabSwitcher(cta);
        }
    }

    @Test
    @MediumTest
    // Low-end uses list mode.
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    // Fails to rotate on some ARM devices.
    // TODO(crbug.com/40917078): fix and re-enable on ARM devices.
    @DisableIf.Build(supported_abis_includes = "armeabi-v7a")
    @DisableIf.Build(supported_abis_includes = "arm64-v8a")
    public void testGetPotentialActionsForView() throws Exception {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        final AccessibilityActionChecker checker = new AccessibilityActionChecker(cta);
        createTabs(cta, false, 5);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 5);

        ViewGroup outerView =
                (ViewGroup) cta.findViewById(TabUiTestHelper.getTabSwitcherAncestorId(cta));
        View view = outerView.findViewById(R.id.tab_list_recycler_view);
        assertTrue(view instanceof TabListMediator.TabGridAccessibilityHelper);
        TabListMediator.TabGridAccessibilityHelper helper =
                (TabListMediator.TabGridAccessibilityHelper) view;

        // Verify action list in portrait mode with span count = 2.
        onView(
                        allOf(
                                isDescendantOfA(
                                        withId(TabUiTestHelper.getTabSwitcherAncestorId(cta))),
                                withId(R.id.tab_list_recycler_view)))
                .check(
                        (v, noMatchingViewException) -> {
                            if (noMatchingViewException != null) {
                                throw noMatchingViewException;
                            }
                            assertTrue(v instanceof RecyclerView);
                            RecyclerView recyclerView = (RecyclerView) v;
                            assertEquals(
                                    2,
                                    ((GridLayoutManager) recyclerView.getLayoutManager())
                                            .getSpanCount());

                            View item1 = getItemViewForPosition(recyclerView, 0);
                            checker.verifyListOfAccessibilityAction(
                                    helper.getPotentialActionsForView(item1),
                                    new ArrayList<>(
                                            Arrays.asList(
                                                    TabMovementDirection.RIGHT,
                                                    TabMovementDirection.DOWN)));

                            View item2 = getItemViewForPosition(recyclerView, 1);
                            checker.verifyListOfAccessibilityAction(
                                    helper.getPotentialActionsForView(item2),
                                    new ArrayList<>(
                                            Arrays.asList(
                                                    TabMovementDirection.LEFT,
                                                    TabMovementDirection.DOWN)));

                            View item3 = getItemViewForPosition(recyclerView, 2);
                            checker.verifyListOfAccessibilityAction(
                                    helper.getPotentialActionsForView(item3),
                                    new ArrayList<>(
                                            Arrays.asList(
                                                    TabMovementDirection.RIGHT,
                                                    TabMovementDirection.UP,
                                                    TabMovementDirection.DOWN)));

                            View item4 = getItemViewForPosition(recyclerView, 3);
                            checker.verifyListOfAccessibilityAction(
                                    helper.getPotentialActionsForView(item4),
                                    new ArrayList<>(
                                            Arrays.asList(
                                                    TabMovementDirection.LEFT,
                                                    TabMovementDirection.UP)));

                            View item5 = getItemViewForPosition(recyclerView, 4);
                            checker.verifyListOfAccessibilityAction(
                                    helper.getPotentialActionsForView(item5),
                                    new ArrayList<>(Arrays.asList(TabMovementDirection.UP)));
                        });

        assertTrue(view instanceof TabListRecyclerView);
        TabListRecyclerView tabListRecyclerView = (TabListRecyclerView) view;
        CallbackHelper callbackHelper = new CallbackHelper();
        OnLayoutChangeListener listener =
                (rv, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    callbackHelper.notifyCalled();
                };
        tabListRecyclerView.addOnLayoutChangeListener(listener);
        final int callCount = callbackHelper.getCallCount();
        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        callbackHelper.waitForCallback(callCount);

        // Verify action list in landscape mode with span count = 3.
        onView(
                        allOf(
                                isDescendantOfA(
                                        withId(TabUiTestHelper.getTabSwitcherAncestorId(cta))),
                                withId(R.id.tab_list_recycler_view)))
                .check(
                        (v, noMatchingViewException) -> {
                            if (noMatchingViewException != null) {
                                throw noMatchingViewException;
                            }
                            assertTrue(v instanceof RecyclerView);
                            RecyclerView recyclerView = (RecyclerView) v;
                            // This case only applies for a span of 3.
                            if (((GridLayoutManager) recyclerView.getLayoutManager()).getSpanCount()
                                    != 3) {
                                return;
                            }

                            View item1 = getItemViewForPosition(recyclerView, 0);
                            checker.verifyListOfAccessibilityAction(
                                    helper.getPotentialActionsForView(item1),
                                    new ArrayList<>(
                                            Arrays.asList(
                                                    TabMovementDirection.RIGHT,
                                                    TabMovementDirection.DOWN)));

                            View item2 = getItemViewForPosition(recyclerView, 1);
                            checker.verifyListOfAccessibilityAction(
                                    helper.getPotentialActionsForView(item2),
                                    new ArrayList<>(
                                            Arrays.asList(
                                                    TabMovementDirection.LEFT,
                                                    TabMovementDirection.RIGHT,
                                                    TabMovementDirection.DOWN)));

                            View item3 = getItemViewForPosition(recyclerView, 2);
                            checker.verifyListOfAccessibilityAction(
                                    helper.getPotentialActionsForView(item3),
                                    new ArrayList<>(Arrays.asList(TabMovementDirection.LEFT)));

                            View item4 = getItemViewForPosition(recyclerView, 3);
                            checker.verifyListOfAccessibilityAction(
                                    helper.getPotentialActionsForView(item4),
                                    new ArrayList<>(
                                            Arrays.asList(
                                                    TabMovementDirection.RIGHT,
                                                    TabMovementDirection.UP)));

                            View item5 = getItemViewForPosition(recyclerView, 4);
                            checker.verifyListOfAccessibilityAction(
                                    helper.getPotentialActionsForView(item5),
                                    new ArrayList<>(
                                            Arrays.asList(
                                                    TabMovementDirection.LEFT,
                                                    TabMovementDirection.UP)));
                        });
    }

    @Test
    @MediumTest
    // Low-end uses list mode.
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    // Fails to rotate on some ARM devices.
    // TODO(crbug.com/40917078): fix and re-enable on ARM devices.
    @DisableIf.Build(supported_abis_includes = "armeabi-v7a")
    @DisableIf.Build(supported_abis_includes = "arm64-v8a")
    public void testGetPositionsOfReorderAction() throws Exception {
        final ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        int leftActionId = R.id.move_tab_left;
        int rightActionId = R.id.move_tab_right;
        int upActionId = R.id.move_tab_up;
        int downActionId = R.id.move_tab_down;
        createTabs(cta, false, 5);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 5);

        ViewGroup outerView =
                (ViewGroup) cta.findViewById(TabUiTestHelper.getTabSwitcherAncestorId(cta));
        View view = outerView.findViewById(R.id.tab_list_recycler_view);
        assertTrue(view instanceof TabListMediator.TabGridAccessibilityHelper);
        TabListMediator.TabGridAccessibilityHelper helper =
                (TabListMediator.TabGridAccessibilityHelper) view;

        // Span count 2.
        onView(
                        allOf(
                                isDescendantOfA(
                                        withId(TabUiTestHelper.getTabSwitcherAncestorId(cta))),
                                withId(R.id.tab_list_recycler_view)))
                .check(
                        (v, noMatchingViewException) -> {
                            if (noMatchingViewException != null) {
                                throw noMatchingViewException;
                            }
                            assertTrue(v instanceof RecyclerView);
                            RecyclerView recyclerView = (RecyclerView) v;
                            assertEquals(
                                    2,
                                    ((GridLayoutManager) recyclerView.getLayoutManager())
                                            .getSpanCount());

                            Pair<Integer, Integer> positions;

                            View item1 = getItemViewForPosition(recyclerView, 0);
                            positions = helper.getPositionsOfReorderAction(item1, rightActionId);
                            assertEquals(0, (int) positions.first);
                            assertEquals(1, (int) positions.second);

                            positions = helper.getPositionsOfReorderAction(item1, downActionId);
                            assertEquals(0, (int) positions.first);
                            assertEquals(2, (int) positions.second);

                            View item4 = getItemViewForPosition(recyclerView, 3);
                            positions = helper.getPositionsOfReorderAction(item4, leftActionId);
                            assertEquals(3, (int) positions.first);
                            assertEquals(2, (int) positions.second);

                            positions = helper.getPositionsOfReorderAction(item4, upActionId);
                            assertEquals(3, (int) positions.first);
                            assertEquals(1, (int) positions.second);
                        });

        assertTrue(view instanceof TabListRecyclerView);
        TabListRecyclerView tabListRecyclerView = (TabListRecyclerView) view;
        CallbackHelper callbackHelper = new CallbackHelper();
        OnLayoutChangeListener listener =
                (rv, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    callbackHelper.notifyCalled();
                };
        tabListRecyclerView.addOnLayoutChangeListener(listener);
        final int callCount = callbackHelper.getCallCount();
        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        callbackHelper.waitForCallback(callCount);

        // Span count 3.
        onView(
                        allOf(
                                isDescendantOfA(
                                        withId(TabUiTestHelper.getTabSwitcherAncestorId(cta))),
                                withId(R.id.tab_list_recycler_view)))
                .check(
                        (v, noMatchingViewException) -> {
                            if (noMatchingViewException != null) {
                                throw noMatchingViewException;
                            }
                            assertTrue(v instanceof RecyclerView);
                            RecyclerView recyclerView = (RecyclerView) v;
                            // This case only applies for a span of 3.
                            if (((GridLayoutManager) recyclerView.getLayoutManager()).getSpanCount()
                                    != 3) {
                                return;
                            }

                            Pair<Integer, Integer> positions;

                            View item2 = getItemViewForPosition(recyclerView, 1);
                            positions = helper.getPositionsOfReorderAction(item2, leftActionId);
                            assertEquals(1, (int) positions.first);
                            assertEquals(0, (int) positions.second);

                            positions = helper.getPositionsOfReorderAction(item2, rightActionId);
                            assertEquals(1, (int) positions.first);
                            assertEquals(2, (int) positions.second);

                            positions = helper.getPositionsOfReorderAction(item2, downActionId);
                            assertEquals(1, (int) positions.first);
                            assertEquals(4, (int) positions.second);

                            View item5 = getItemViewForPosition(recyclerView, 4);
                            positions = helper.getPositionsOfReorderAction(item5, leftActionId);
                            assertEquals(4, (int) positions.first);
                            assertEquals(3, (int) positions.second);

                            positions = helper.getPositionsOfReorderAction(item5, upActionId);
                            assertEquals(4, (int) positions.first);
                            assertEquals(1, (int) positions.second);
                        });
    }

    private View getItemViewForPosition(RecyclerView recyclerView, int position) {
        // Scroll to position to ensure the ViewHolder is not recycled.
        ((LinearLayoutManager) recyclerView.getLayoutManager()).scrollToPosition(position);
        RecyclerView.ViewHolder viewHolder =
                recyclerView.findViewHolderForAdapterPosition(position);
        assertNotNull(viewHolder);
        return viewHolder.itemView;
    }

    private static class AccessibilityActionChecker {
        private final Context mContext;

        AccessibilityActionChecker(ChromeTabbedActivity cta) {
            mContext = cta;
        }

        void verifyListOfAccessibilityAction(
                List<AccessibilityAction> actions, List<Integer> directions) {
            assertEquals(directions.size(), actions.size());
            for (int i = 0; i < actions.size(); i++) {
                verifyAccessibilityAction(actions.get(i), directions.get(i));
            }
        }

        void verifyAccessibilityAction(
                AccessibilityAction action, @TabMovementDirection int direction) {
            switch (direction) {
                case TabMovementDirection.LEFT:
                    assertEquals(R.id.move_tab_left, action.getId());
                    assertEquals(
                            mContext.getString(R.string.accessibility_tab_movement_left),
                            action.getLabel());
                    break;
                case TabMovementDirection.RIGHT:
                    assertEquals(R.id.move_tab_right, action.getId());
                    assertEquals(
                            mContext.getString(R.string.accessibility_tab_movement_right),
                            action.getLabel());
                    break;
                case TabMovementDirection.UP:
                    assertEquals(R.id.move_tab_up, action.getId());
                    assertEquals(
                            mContext.getString(R.string.accessibility_tab_movement_up),
                            action.getLabel());
                    break;
                case TabMovementDirection.DOWN:
                    assertEquals(R.id.move_tab_down, action.getId());
                    assertEquals(
                            mContext.getString(R.string.accessibility_tab_movement_down),
                            action.getLabel());
                    break;
                default:
                    assert false;
            }
        }
    }
}
