// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.CardCountAssertion;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createOverviewHideWatcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;

import android.graphics.Rect;
import android.support.test.espresso.Espresso;
import android.support.test.espresso.NoMatchingRootException;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.ArrayList;
import java.util.List;

/** End-to-end tests for TabGridDialog component. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_UI_IMPROVEMENTS_ANDROID})
public class TabGridDialogTest {
    // clang-format on

    private boolean mHasReceivedSourceRect;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Before
    public void setUp() {
        FeatureUtilities.setGridTabSwitcherEnabledForTesting(true);
        FeatureUtilities.setTabGroupsAndroidEnabledForTesting(true);
        mActivityTestRule.startMainActivityFromLauncher();
        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        CriteriaHelper.pollUiThread(mActivityTestRule.getActivity()
                                            .getTabModelSelector()
                                            .getTabModelFilterProvider()
                                            .getCurrentTabModelFilter()::isTabModelRestored);
    }

    @Test
    @MediumTest
    public void testBackPressCloseDialog() throws InterruptedException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Open dialog from tab switcher and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2);

        // Press back and dialog should be hidden.
        Espresso.pressBack();
        CriteriaHelper.pollInstrumentationThread(() -> !isDialogShowing(cta));

        verifyTabSwitcherCardCount(cta, 1);

        // Enter first tab page.
        assertTrue(cta.getLayoutManager().overviewVisible());
        clickFirstCardFromTabSwitcher(cta);
        clickFirstTabFromDialog(cta);
        // Open dialog from tab strip and verify dialog is showing correct content.
        openDialogFromStripAndVerify(cta, 2);

        // Press back and dialog should be hidden.
        Espresso.pressBack();
        CriteriaHelper.pollInstrumentationThread(() -> !isDialogShowing(cta));
    }

    @Test
    @MediumTest
    public void testDisableTabGroupsContinuation() throws InterruptedException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2);

        // Verify TabGroupsContinuation related functionality is not exposed.
        verifyTabGroupsContinuation(cta, false);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testEnableTabGroupsContinuation() throws InterruptedException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2);

        // Verify TabGroupsContinuation related functionality is exposed.
        verifyTabGroupsContinuation(cta, true);
    }

    @Test
    @MediumTest
    public void testTabGridDialogAnimation() throws InterruptedException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Add 400px top margin to the recyclerView.
        RecyclerView recyclerView = cta.findViewById(R.id.tab_list_view);
        float tabGridCardPadding = cta.getResources().getDimension(R.dimen.tab_list_card_padding);
        int deltaTopMargin = 400;
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) recyclerView.getLayoutParams();
        params.topMargin += deltaTopMargin;
        TestThreadUtils.runOnUiThreadBlocking(() -> { recyclerView.setLayoutParams(params); });
        CriteriaHelper.pollUiThread(() -> !recyclerView.isComputingLayout());

        // Calculate expected values of animation source rect.
        mHasReceivedSourceRect = false;
        View parentView = cta.getCompositorViewHolder();
        Rect parentRect = new Rect();
        parentView.getGlobalVisibleRect(parentRect);
        Rect sourceRect = new Rect();
        recyclerView.getChildAt(0).getGlobalVisibleRect(sourceRect);
        // TODO(yuezhanggg): Figure out why the sourceRect.left is wrong after setting the margin.
        float expectedTop = sourceRect.top - parentRect.top + tabGridCardPadding;
        float expectedWidth = sourceRect.width() - 2 * tabGridCardPadding;
        float expectedHeight = sourceRect.height() - 2 * tabGridCardPadding;

        // Setup the callback to verify the animation source Rect.
        StartSurfaceLayout layout = (StartSurfaceLayout) cta.getLayoutManager().getOverviewLayout();
        TabSwitcher.TabDialogDelegation delegation =
                layout.getStartSurfaceForTesting().getTabDialogDelegate();
        delegation.setSourceRectCallbackForTesting((result -> {
            mHasReceivedSourceRect = true;
            assertTrue(expectedTop == result.top);
            assertTrue(expectedHeight == result.height());
            assertTrue(expectedWidth == result.width());
        }));

        TabUiTestHelper.clickFirstCardFromTabSwitcher(cta);
        CriteriaHelper.pollUiThread(() -> mHasReceivedSourceRect);
        CriteriaHelper.pollInstrumentationThread(() -> isDialogShowing(cta));
    }

    private void mergeAllTabsToAGroup(ChromeTabbedActivity cta) {
        List<Tab> tabGroup = new ArrayList<>();
        TabModel tabModel = cta.getTabModelSelector().getModel(false);
        for (int i = 0; i < tabModel.getCount(); i++) {
            tabGroup.add(tabModel.getTabAt(i));
        }
        createTabGroup(cta, false, tabGroup);
        assertTrue(cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                           instanceof TabGroupModelFilter);
        TabGroupModelFilter filter = (TabGroupModelFilter) cta.getTabModelSelector()
                                             .getTabModelFilterProvider()
                                             .getCurrentTabModelFilter();
        assertEquals(1, filter.getCount());
    }

    private void openDialogFromTabSwitcherAndVerify(ChromeTabbedActivity cta, int tabCount) {
        clickFirstCardFromTabSwitcher(cta);
        CriteriaHelper.pollInstrumentationThread(() -> isDialogShowing(cta));
        verifyShowingDialog(cta, tabCount);
    }

    private void openDialogFromStripAndVerify(ChromeTabbedActivity cta, int tabCount) {
        showDialogFromStrip(cta);
        CriteriaHelper.pollInstrumentationThread(() -> isDialogShowing(cta));
        verifyShowingDialog(cta, tabCount);
    }

    private void verifyShowingDialog(ChromeTabbedActivity cta, int tabCount) {
        onView(withId(R.id.tab_list_view))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check(CardCountAssertion.havingTabCount(tabCount));

        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    Assert.assertTrue(v instanceof EditText);
                    EditText titleText = (EditText) v;
                    String title = cta.getResources().getQuantityString(
                            R.plurals.bottom_tab_grid_title_placeholder, tabCount, tabCount);
                    Assert.assertEquals(title, titleText.getText().toString());
                    assertFalse(v.isFocused());
                });
    }

    private boolean isDialogShowing(ChromeTabbedActivity cta) {
        boolean isShowing = true;
        try {
            onView(withId(R.id.tab_list_view))
                    .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                    .check(matches(isDisplayed()));
        } catch (NoMatchingRootException e) {
            isShowing = false;
        } catch (Exception e) {
            assert false : "error when inspecting dialog recyclerView.";
        }
        return isShowing;
    }

    private void clickFirstTabFromDialog(ChromeTabbedActivity cta) {
        OverviewModeBehaviorWatcher hideWatcher = createOverviewHideWatcher(cta);
        onView(withId(R.id.tab_list_view))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        hideWatcher.waitForBehavior();
    }

    private void showDialogFromStrip(ChromeTabbedActivity cta) {
        assertFalse(cta.getLayoutManager().overviewVisible());
        onView(withId(R.id.toolbar_left_button)).perform(click());
    }

    private void verifyTabGroupsContinuation(ChromeTabbedActivity cta, boolean isEnabled) {
        assertEquals(isEnabled, FeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        // Verify whether the menu button exists.
        onView(withId(R.id.toolbar_menu_button))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check(isEnabled ? matches(isDisplayed()) : doesNotExist());

        // Try to grab focus of the title text field by clicking on it.
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    // Verify if we can grab focus on the editText or not.
                    assertEquals(isEnabled, v.isFocused());
                });
    }
}
