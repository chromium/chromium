// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_AUTO;
import static android.view.View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.matcher.BundleMatchers.hasEntry;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasExtras;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasType;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.core.IsEqual.equalTo;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUPS_ANDROID;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickNthTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeNthTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.finishActivity;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getSwipeToDismissAction;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.prepareTabsWithThumbnail;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyAllTabsHaveThumbnail;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabStripFaviconCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.chrome.features.start_surface.InstantStartTest.createTabStateFile;
import static org.chromium.chrome.features.start_surface.InstantStartTest.createThumbnailBitmapAndWriteToFile;
import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;
import static org.chromium.chrome.test.util.ViewUtils.waitForView;

import android.content.Intent;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.rule.IntentsTestRule;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.features.start_surface.StartSurfaceLayout;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.ui.util.ColorUtils;

import java.util.concurrent.ExecutionException;

/** End-to-end tests for TabGridDialog component. */
// clang-format off
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction({UiRestriction.RESTRICTION_TYPE_PHONE,
        Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE})
@Features.EnableFeatures({TAB_GRID_LAYOUT_ANDROID, TAB_GROUPS_ANDROID})
public class TabGridDialogTest {
    // clang-format on
    private static final String CUSTOMIZED_TITLE1 = "wfh tips";
    private static final String CUSTOMIZED_TITLE2 = "wfh funs";
    private static final String START_SURFACE_BASE_PARAMS =
            "force-fieldtrial-params=Study.Group:start_surface_variation";
    private static final String TAB_GROUP_LAUNCH_POLISH_PARAMS =
            "force-fieldtrial-params=Study.Group:enable_launch_polish/true";

    private boolean mHasReceivedSourceRect;
    private TabSelectionEditorTestingRobot mSelectionEditorRobot =
            new TabSelectionEditorTestingRobot();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public IntentsTestRule<ChromeActivity> mShareActivityTestRule =
            new IntentsTestRule<>(ChromeActivity.class, false, false);

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        TabUiFeatureUtilities.setTabManagementModuleSupportedForTesting(true);
        mActivityTestRule.startMainActivityOnBlankPage();
        Layout layout = mActivityTestRule.getActivity().getLayoutManager().getOverviewLayout();
        assertTrue(layout instanceof StartSurfaceLayout);
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }

    @After
    public void tearDown() {
        TabUiFeatureUtilities.setTabManagementModuleSupportedForTesting(null);
        ActivityUtils.clearActivityOrientation(mActivityTestRule.getActivity());
    }

    @Test
    @MediumTest
    public void testBackPressCloseDialog() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Open dialog from tab switcher and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Press back and dialog should be hidden.
        Espresso.pressBack();
        waitForDialogHidingAnimationInTabSwitcher(cta);

        verifyTabSwitcherCardCount(cta, 1);

        // Enter first tab page.
        assertTrue(cta.getLayoutManager().overviewVisible());
        clickFirstCardFromTabSwitcher(cta);
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);

        // Open dialog from tab strip and verify dialog is showing correct content.
        openDialogFromStripAndVerify(cta, 2, null);

        // Press back and dialog should be hidden.
        Espresso.pressBack();
        waitForDialogHidingAnimation(cta);
    }

    @Test
    @MediumTest
    public void testClickScrimCloseDialog() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Open dialog from tab switcher and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Click scrim view and dialog should be hidden.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);

        verifyTabSwitcherCardCount(cta, 1);

        // Enter first tab page.
        assertTrue(cta.getLayoutManager().overviewVisible());
        clickFirstCardFromTabSwitcher(cta);
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);

        // Open dialog from tab strip and verify dialog is showing correct content.
        openDialogFromStripAndVerify(cta, 2, null);

        // Click scrim view and dialog should be hidden.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);

        // Checkout the scrim view observer is correctly setup by closing dialog in tab switcher
        // again.
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
    }

    @Test
    @MediumTest
    public void testDisableTabGroupsContinuation() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Verify TabGroupsContinuation related functionality is not exposed.
        verifyTabGroupsContinuation(cta, false);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testEnableTabGroupsContinuation() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Verify TabGroupsContinuation related functionality is exposed.
        verifyTabGroupsContinuation(cta, true);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky test - see: https://crbug.com/1177149")
    public void testTabGridDialogAnimation() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Add 400px top margin to the recyclerView.
        RecyclerView recyclerView = cta.findViewById(R.id.tab_list_view);
        float tabGridCardPadding = cta.getResources().getDimension(R.dimen.tab_list_card_padding);
        int deltaTopMargin = 400;
        ViewGroup.MarginLayoutParams params =
                (ViewGroup.MarginLayoutParams) recyclerView.getLayoutParams();
        params.topMargin += deltaTopMargin;
        TestThreadUtils.runOnUiThreadBlocking(() -> recyclerView.setLayoutParams(params));
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
        TabGridDialogView.setSourceRectCallbackForTesting((result -> {
            mHasReceivedSourceRect = true;
            assertEquals(expectedTop, result.top, 0.0);
            assertEquals(expectedHeight, result.height(), 0.0);
            assertEquals(expectedWidth, result.width(), 0.0);
        }));

        TabUiTestHelper.clickFirstCardFromTabSwitcher(cta);
        CriteriaHelper.pollUiThread(() -> mHasReceivedSourceRect);
        CriteriaHelper.pollUiThread(() -> isDialogShowing(cta));
    }

    @Test
    @MediumTest
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1121363")
    public void testUndoClosureInDialog_DialogUndoBar() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Verify close and undo in dialog from tab switcher.
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 1, null);
        verifyDialogUndoBarAndClick();
        verifyShowingDialog(cta, 2, null);

        // Verify close and undo in dialog from tab strip.
        clickFirstTabInDialog(cta);
        openDialogFromStripAndVerify(cta, 2, null);
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 1, null);
        verifyDialogUndoBarAndClick();
        verifyShowingDialog(cta, 2, null);
        clickScrimToExitDialog(cta);
        verifyTabStripFaviconCount(cta, 2);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testDialogToolbarMenuShareGroup() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Click to show the menu and verify it.
        openDialogToolbarMenuAndVerify(cta);

        // Trigger the share sheet by clicking the share button and verify it.
        triggerShareGroupAndVerify(cta);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
            ChromeFeatureList.CHROME_SHARING_HUB})
    public void testDialogToolbarMenuShareGroup_WithSharingHub() {
        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openDialogToolbarMenuAndVerify(cta);

        // We should still show Android share sheet even with sharing hub enabled.
        triggerShareGroupAndVerify(cta);
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
        ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:enable_launch_polish/true"})
    public void testSelectionEditorShowHide() throws ExecutionException {
        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and open selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);

        // Click navigation button should close selection editor but not tab grid dialog.
        mSelectionEditorRobot.actionRobot.clickToolbarNavigationButton();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        assertTrue(isDialogShowing(cta));

        // Back press should close both the dialog and selection editor.
        openSelectionEditorAndVerify(cta, 2);
        Espresso.pressBack();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Clicking ScrimView should close both the dialog and selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);
        clickScrimToExitDialog(cta);
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testSelectionEditorUngroup() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        assertTrue(cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                           instanceof TabGroupModelFilter);
        final TabGroupModelFilter filter = (TabGroupModelFilter) cta.getTabModelSelector()
                                                   .getTabModelFilterProvider()
                                                   .getCurrentTabModelFilter();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        assertEquals(1, filter.getCount());

        // Open dialog and open selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 3, null);
        openSelectionEditorAndVerify(cta, 3);

        // Select and ungroup the first tab.
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0);
        mSelectionEditorRobot.resultRobot.verifyItemSelectedAtAdapterPosition(0)
                .verifyToolbarActionButtonEnabled()
                .verifyToolbarSelectionText("1 selected");

        mSelectionEditorRobot.actionRobot.clickToolbarActionButton();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        verifyShowingDialog(cta, 2, null);
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        assertEquals(2, filter.getCount());

        // Open dialog and open selection editor.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        openSelectionEditorAndVerify(cta, 2);

        // Select and ungroup all two tabs in dialog.
        mSelectionEditorRobot.actionRobot.clickItemAtAdapterPosition(0).clickItemAtAdapterPosition(
                1);
        mSelectionEditorRobot.resultRobot.verifyItemSelectedAtAdapterPosition(0)
                .verifyItemSelectedAtAdapterPosition(1)
                .verifyToolbarActionButtonEnabled()
                .verifyToolbarSelectionText("2 selected");

        mSelectionEditorRobot.actionRobot.clickToolbarActionButton();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        assertEquals(3, filter.getCount());
    }

    @Test
    @MediumTest
    public void testSwipeToDismiss_Dialog() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        // Create 2 tabs and merge them into one group.
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Swipe to dismiss two tabs in dialog.
        onView(allOf(withId(R.id.tab_list_view), withParent(withId(R.id.dialog_container_view))))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        1, getSwipeToDismissAction(true)));
        verifyShowingDialog(cta, 1, null);
        onView(allOf(withId(R.id.tab_list_view), withParent(withId(R.id.dialog_container_view))))
                .perform(RecyclerViewActions.actionOnItemAtPosition(
                        0, getSwipeToDismissAction(false)));
        waitForDialogHidingAnimation(cta);
        verifyTabSwitcherCardCount(cta, 0);
    }

    @Test
    @MediumTest
    // clang-format off
    @EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
        ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group",
        "force-fieldtrial-params=Study.Group:enable_launch_polish/true"})
    public void testSelectionEditorPosition() {
        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        View parentView = cta.getCompositorViewHolder();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Verify the size and position of TabGridDialog in portrait mode.
        openDialogFromTabSwitcherAndVerify(cta, 3, null);
        checkPosition(cta, true, true);

        // Verify the size and position of TabSelectionEditor in portrait mode.
        openSelectionEditorAndVerify(cta, 3);
        checkPosition(cta, false, true);

        // Verify the size and position of TabSelectionEditor in landscape mode.
        ActivityUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(() -> parentView.getHeight() < parentView.getWidth());
        checkPosition(cta, false, false);

        // Verify the size and position of TabGridDialog in landscape mode.
        mSelectionEditorRobot.actionRobot.clickToolbarNavigationButton();
        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsHidden();
        assertTrue(isDialogShowing(cta));
        checkPosition(cta, true, false);

        // Verify the positioning in multi-window mode. Adjusting the height of the root view to
        // mock entering/exiting multi-window mode.
        ActivityUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_PORTRAIT);
        CriteriaHelper.pollUiThread(() -> parentView.getHeight() > parentView.getWidth());
        View rootView = cta.findViewById(R.id.coordinator);
        int rootViewHeight = rootView.getHeight();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup.LayoutParams params = rootView.getLayoutParams();
            params.height = rootViewHeight / 2;
            rootView.setLayoutParams(params);
        });
        checkPosition(cta, true, true);
        openSelectionEditorAndVerify(cta, 3);
        checkPosition(cta, false, true);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup.LayoutParams params = rootView.getLayoutParams();
            params.height = rootViewHeight;
            rootView.setLayoutParams(params);
        });
        checkPosition(cta, false, true);
        checkPosition(cta, true, true);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures(ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID)
    public void testTabGroupNaming() throws ExecutionException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Open dialog and modify group title.
        openDialogFromTabSwitcherAndVerify(cta, 2,
                cta.getResources().getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder, 2, 2));
        editDialogTitle(cta, CUSTOMIZED_TITLE1);

        // Verify the title is updated in both tab switcher and dialog.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        verifyFirstCardTitle(CUSTOMIZED_TITLE1);
        openDialogFromTabSwitcherAndVerify(cta, 2, CUSTOMIZED_TITLE1);

        // Modify title in dialog from tab strip.
        clickFirstTabInDialog(cta);
        openDialogFromStripAndVerify(cta, 2, CUSTOMIZED_TITLE1);
        editDialogTitle(cta, CUSTOMIZED_TITLE2);

        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        enterTabSwitcher(cta);
        verifyFirstCardTitle(CUSTOMIZED_TITLE2);
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
            ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", TAB_GROUP_LAUNCH_POLISH_PARAMS})
    public void testTabGroupNaming_KeyboardVisibility() throws ExecutionException {
        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 2,
                cta.getResources().getQuantityString(
                        R.plurals.bottom_tab_grid_title_placeholder, 2, 2));

        // Test title text focus in dialog in tab switcher.
        testTitleTextFocus(cta);

        // Test title text focus in dialog from tab strip.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);
        openDialogFromStripAndVerify(cta, 2, null);
        testTitleTextFocus(cta);
    }

    @Test
    @MediumTest
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1124336")
    public void testDialogInitialShowFromStrip() throws Exception {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsWithThumbnail(mActivityTestRule, 2, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Restart the activity and open the dialog from strip to check the initial setup of dialog.
        finishActivity(cta);
        mActivityTestRule.startMainActivityFromLauncher();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
        openDialogFromStripAndVerify(mActivityTestRule.getActivity(), 2, null);
        closeNthTabInDialog(0);
        verifyShowingDialog(cta, 1, null);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    @FlakyTest(message = "https://crbug.com/1139475")
    public void testRenderDialog_3Tabs_Portrait(boolean nightModeEnabled) throws Exception {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsWithThumbnail(mActivityTestRule, 3, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        verifyAllTabsHaveThumbnail(cta.getCurrentTabModel());

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 3, null);

        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        mRenderTestRule.render(dialogView, "3_tabs_portrait");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @FlakyTest(message = "https://crbug.com/1110099")
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderDialog_3Tabs_Landscape(boolean nightModeEnabled) throws Exception {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsWithThumbnail(mActivityTestRule, 3, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        verifyAllTabsHaveThumbnail(cta.getCurrentTabModel());

        // Rotate to landscape mode and create a tab group.
        ActivityUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 3, null);

        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        mRenderTestRule.render(dialogView, "3_tabs_landscape");
    }

    @Test
    @DisabledTest
    @MediumTest
    @Feature({"RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderDialog_5Tabs_InitialScroll(boolean nightModeEnabled) throws Exception {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsWithThumbnail(mActivityTestRule, 5, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 5);
        verifyAllTabsHaveThumbnail(cta.getCurrentTabModel());

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 5, null);

        // Select the last tab and reopen the dialog. Verify that the dialog has scrolled to the
        // correct position.
        clickNthTabInDialog(cta, 4);
        enterTabSwitcher(cta);
        openDialogFromTabSwitcherAndVerify(cta, 5, null);

        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        mRenderTestRule.render(dialogView, "5_tabs_select_last");
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.INSTANT_START})
    public void testSetup_WithInstantStart() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsWithThumbnail(mActivityTestRule, 2, 0, "about:blank");
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Verify TabModelObserver is correctly setup by checking if tab grid dialog changes with
        // tab closure.
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 1, null);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1157518")
    public void testAdjustBackGroundViewAccessibilityImportance() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Verify accessibility importance adjustment when opening dialog from tab switcher.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        verifyBackgroundViewAccessibilityImportance(cta, true);
        Espresso.pressBack();
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyBackgroundViewAccessibilityImportance(cta, false);

        // Verify accessibility importance adjustment when opening dialog from tab strip.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);
        openDialogFromStripAndVerify(cta, 2, null);
        verifyBackgroundViewAccessibilityImportance(cta, true);
        Espresso.pressBack();
        waitForDialogHidingAnimation(cta);
        verifyBackgroundViewAccessibilityImportance(cta, false);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "TODO(crbug.com/1128345): Fix flakiness.")
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID,
            ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", TAB_GROUP_LAUNCH_POLISH_PARAMS})
    public void testAccessibilityString() throws ExecutionException {
        // clang-format on
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 3);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 3);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Verify the initial group card content description.
        RecyclerView recyclerView = cta.findViewById(R.id.tab_list_view);
        View firstItem = recyclerView.findViewHolderForAdapterPosition(0).itemView;
        String expandTargetString = "Expand tab group with 3 tabs.";
        assertEquals(expandTargetString, firstItem.getContentDescription());

        // Back button content description should update with group title.
        String collapseTargetString = "Collapse tab group with 3 tabs.";
        openDialogFromTabSwitcherAndVerify(cta, 3, null);
        verifyDialogBackButtonContentDescription(cta, collapseTargetString);
        editDialogTitle(cta, CUSTOMIZED_TITLE1);
        collapseTargetString = "Collapse " + CUSTOMIZED_TITLE1 + " tab group with 3 tabs.";
        verifyDialogBackButtonContentDescription(cta, collapseTargetString);

        // Group card content description should update with group title.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        verifyFirstCardTitle(CUSTOMIZED_TITLE1);
        expandTargetString = "Expand " + CUSTOMIZED_TITLE1 + " tab group with 3 tabs.";
        assertEquals(expandTargetString, firstItem.getContentDescription());

        // Verify the TabSwitcher group card close button content description should update with
        // group title.
        View closeButton = firstItem.findViewById(R.id.action_button);
        String closeButtonTargetString = "Close " + CUSTOMIZED_TITLE1 + " group with 3 tabs";
        assertEquals(closeButtonTargetString, closeButton.getContentDescription());

        // Back button content description should update with group count change.
        openDialogFromTabSwitcherAndVerify(cta, 3, CUSTOMIZED_TITLE1);
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 2, CUSTOMIZED_TITLE1);
        collapseTargetString = "Collapse " + CUSTOMIZED_TITLE1 + " tab group with 2 tabs.";
        verifyDialogBackButtonContentDescription(cta, collapseTargetString);

        // Group card content description should update with group count change.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        expandTargetString = "Expand " + CUSTOMIZED_TITLE1 + " tab group with 2 tabs.";
        assertEquals(expandTargetString, firstItem.getContentDescription());

        // TabSwitcher group card Close button content description should update with group count
        // change.
        closeButtonTargetString = "Close " + CUSTOMIZED_TITLE1 + " group with 2 tabs";
        assertEquals(closeButtonTargetString, closeButton.getContentDescription());

        // Back button content description should restore when the group loses customized title.
        openDialogFromTabSwitcherAndVerify(cta, 2, CUSTOMIZED_TITLE1);
        editDialogTitle(cta, "");
        verifyShowingDialog(cta, 2, null);
        collapseTargetString = "Collapse tab group with 2 tabs.";
        verifyDialogBackButtonContentDescription(cta, collapseTargetString);

        // Back button content description should update when the group becomes a single tab.
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 1, "1 tab");
        collapseTargetString = "Collapse 1 tab.";
        verifyDialogBackButtonContentDescription(cta, collapseTargetString);

        // Group card content description should restore when the group becomes a single tab.
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimationInTabSwitcher(cta);
        assertEquals(null, firstItem.getContentDescription());

        // TabSwitcher Group card Close button content description should restore when the group
        // becomes a single tab.
        closeButtonTargetString = "Close New tab tab";
        assertEquals(closeButtonTargetString, closeButton.getContentDescription());
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", START_SURFACE_BASE_PARAMS + "/single"})
    public void testDialogSetup_WithStartSurface() throws Exception {
        // Create a tab group with 2 tabs.
        finishActivity(mActivityTestRule.getActivity());
        createThumbnailBitmapAndWriteToFile(0);
        createThumbnailBitmapAndWriteToFile(1);
        TabAttributeCache.setRootIdForTesting(0, 0);
        TabAttributeCache.setRootIdForTesting(1, 0);
        createTabStateFile(new int[] {0, 1});

        // Restart Chrome and make sure tab strip is showing.
        mActivityTestRule.startMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
        CriteriaHelper.pollUiThread(()
                                            -> mActivityTestRule.getActivity()
                                                       .getBrowserControlsManager()
                                                       .getBottomControlOffset()
                        == 0);
        waitForView(allOf(withId(R.id.toolbar_left_button), isCompletelyDisplayed()));

        // Test opening dialog from strip and from tab switcher.
        openDialogFromStripAndVerify(cta, 2, null);
        Espresso.pressBack();
        // Tab switcher is created, and the dummy signal to hide dialog is sent. This line would
        // crash if the dummy signal is not properly handled. See crbug.com/1096358.
        enterTabSwitcher(cta);
        onView(allOf(withParent(withId(R.id.tasks_surface_body)), withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        CriteriaHelper.pollUiThread(() -> isDialogShowing(mActivityTestRule.getActivity()));
        verifyShowingDialog(cta, 2, null);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.START_SURFACE_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", START_SURFACE_BASE_PARAMS + "/single"})
    @DisabledTest(message = "crbug.com/1119899, crbug.com/1131545")
    public void testUndoClosureInDialog_WithStartSurface() throws Exception {
        // Create a tab group with 2 tabs.
        finishActivity(mActivityTestRule.getActivity());
        createThumbnailBitmapAndWriteToFile(0);
        createThumbnailBitmapAndWriteToFile(1);
        TabAttributeCache.setRootIdForTesting(0, 0);
        TabAttributeCache.setRootIdForTesting(1, 0);
        createTabStateFile(new int[] {0, 1});

        // Restart Chrome and make sure tab strip is showing.
        mActivityTestRule.startMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
        CriteriaHelper.pollUiThread(
                () -> cta.getBrowserControlsManager().getBottomControlOffset() == 0);
        waitForView(allOf(withId(R.id.toolbar_left_button), isCompletelyDisplayed()));

        // Test undo closure in dialog from tab strip.
        openDialogFromStripAndVerify(cta, 2, null);
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 1, null);
        verifyDialogUndoBarAndClick();
        verifyShowingDialog(cta, 2, null);
        clickScrimToExitDialog(cta);
        verifyTabStripFaviconCount(cta, 2);

        // Test undo closure in dialog from StartSurface tab switcher.
        enterTabSwitcher(cta);
        onView(allOf(withParent(withId(R.id.tasks_surface_body)), withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        CriteriaHelper.pollUiThread(() -> isDialogShowing(cta));
        verifyShowingDialog(cta, 2, null);
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 1, null);
        verifyDialogUndoBarAndClick();
        verifyShowingDialog(cta, 2, null);

        // Test undo closure in dialog from StartSurface home page.
        clickScrimToExitDialog(cta);
        onView(withId(org.chromium.chrome.start_surface.R.id.new_tab_button)).perform(click());
        onView(allOf(withParent(withId(R.id.carousel_tab_switcher_container)),
                       withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(0, click()));
        CriteriaHelper.pollUiThread(() -> isDialogShowing(cta));
        verifyShowingDialog(cta, 2, null);
        closeFirstTabInDialog();
        verifyShowingDialog(cta, 1, null);
        verifyDialogUndoBarAndClick();
        verifyShowingDialog(cta, 2, null);
    }

    @Test
    @MediumTest
    public void testCreateTabInDialog() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);

        // Create a tab group.
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        // Open dialog from tab switcher and verify dialog is showing correct content.
        openDialogFromTabSwitcherAndVerify(cta, 2, null);

        // Create a tab by tapping "+" on the dialog.
        onView(allOf(withId(R.id.toolbar_right_button),
                       isDescendantOfA(withId(R.id.dialog_container_view))))
                .perform(click());
        waitForDialogHidingAnimation(cta);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
        openDialogFromTabSwitcherAndVerify(cta, 3, null);

        // Enter first tab page.
        clickFirstTabInDialog(cta);
        waitForDialogHidingAnimation(cta);
        // Open dialog from tab strip and verify dialog is showing correct content.
        openDialogFromStripAndVerify(cta, 3, null);

        // Create a tab by tapping "+" on the dialog.
        onView(allOf(withId(R.id.toolbar_right_button),
                       isDescendantOfA(withId(R.id.dialog_container_view))))
                .perform(click());
        waitForDialogHidingAnimation(cta);
        openDialogFromStripAndVerify(cta, 4, null);
    }

    private void openDialogFromTabSwitcherAndVerify(
            ChromeTabbedActivity cta, int tabCount, String customizedTitle) {
        clickFirstCardFromTabSwitcher(cta);
        CriteriaHelper.pollUiThread(() -> isDialogShowing(cta));
        verifyShowingDialog(cta, tabCount, customizedTitle);
    }

    private void openDialogFromStripAndVerify(
            ChromeTabbedActivity cta, int tabCount, String customizedTitle) {
        showDialogFromStrip(cta);
        CriteriaHelper.pollUiThread(() -> isDialogShowing(cta));
        verifyShowingDialog(cta, tabCount, customizedTitle);
    }

    private void verifyShowingDialog(
            ChromeTabbedActivity cta, int tabCount, String customizedTitle) {
        onView(allOf(withId(R.id.tab_list_view), withParent(withId(R.id.dialog_container_view))))
                .check(matches(isDisplayed()))
                .check(TabUiTestHelper.ChildrenCountAssertion.havingTabCount(tabCount));

        // Check contents within dialog.
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    Assert.assertTrue(v instanceof EditText);
                    EditText titleText = (EditText) v;
                    String title = customizedTitle == null
                            ? cta.getResources().getQuantityString(
                                    R.plurals.bottom_tab_grid_title_placeholder, tabCount, tabCount)
                            : customizedTitle;
                    Assert.assertEquals(title, titleText.getText().toString());
                    assertFalse(v.isFocused());
                });

        // Check dummy views used for animations are not visible.
        onView(allOf(withParent(withId(R.id.dialog_parent_view)), withId(R.id.dialog_frame)))
                .check((v, e) -> assertEquals(0f, v.getAlpha(), 0.0));
        onView(allOf(withParent(withId(R.id.dialog_parent_view)),
                       withId(R.id.dialog_animation_card_view)))
                .check((v, e) -> assertEquals(0f, v.getAlpha(), 0.0));

        // For devices with version higher or equal to O_MR1 and use light color navigation bar,
        // make sure that the color of navigation bar is changed by dialog scrim.
        Resources resources = cta.getResources();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O_MR1
                || !resources.getBoolean(R.bool.window_light_navigation_bar)) {
            return;
        }
        @ColorInt
        int scrimDefaultColor = ApiCompatibilityUtils.getColor(resources, R.color.black_alpha_65);
        @ColorInt
        int navigationBarColor =
                ApiCompatibilityUtils.getColor(resources, R.color.bottom_system_nav_color);
        float scrimColorAlpha = (scrimDefaultColor >>> 24) / 255f;
        int scrimColorOpaque = scrimDefaultColor & 0xFF000000;
        int navigationBarColorWithScrimOverlay = ColorUtils.getColorWithOverlay(
                navigationBarColor, scrimColorOpaque, scrimColorAlpha, true);

        assertEquals(cta.getWindow().getNavigationBarColor(), navigationBarColorWithScrimOverlay);
        assertNotEquals(navigationBarColor, navigationBarColorWithScrimOverlay);
    }

    private boolean isDialogShowing(ChromeTabbedActivity cta) {
        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        return dialogView.getVisibility() == View.VISIBLE && dialogView.getAlpha() == 1f;
    }

    private boolean isDialogHiding(ChromeTabbedActivity cta) {
        View dialogView = cta.findViewById(R.id.dialog_parent_view);
        return dialogView.getVisibility() == View.GONE;
    }

    private void showDialogFromStrip(ChromeTabbedActivity cta) {
        assertFalse(cta.getLayoutManager().overviewVisible());
        onView(allOf(withId(R.id.toolbar_left_button),
                       isDescendantOfA(withId(R.id.bottom_controls))))
                .perform(click());
    }

    private void verifyTabGroupsContinuation(ChromeTabbedActivity cta, boolean isEnabled) {
        assertEquals(isEnabled, TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled());

        // Verify whether the menu button exists.
        onView(withId(R.id.toolbar_menu_button))
                .check(isEnabled ? matches(isDisplayed()) : doesNotExist());

        // Try to grab focus of the title text field by clicking on it.
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .perform(click());
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    // Verify if we can grab focus on the editText or not.
                    assertEquals(isEnabled, v.isFocused());
                });
        // Verify if the keyboard shows or not.
        CriteriaHelper.pollUiThread(()
                                            -> isEnabled
                        == KeyboardVisibilityDelegate.getInstance().isKeyboardShowing(
                                cta, cta.getCompositorViewHolder()));
    }

    private void openDialogToolbarMenuAndVerify(ChromeTabbedActivity cta) {
        onView(withId(R.id.toolbar_menu_button))
                .perform(click());
        onView(withId(R.id.tab_switcher_action_menu_list))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;
                    Assert.assertTrue(v instanceof ListView);
                    ListView listView = (ListView) v;
                    verifyTabGridDialogToolbarMenuItem(listView, 0,
                            cta.getString(R.string.tab_grid_dialog_toolbar_remove_from_group));
                    verifyTabGridDialogToolbarMenuItem(listView, 1,
                            cta.getString(R.string.tab_grid_dialog_toolbar_share_group));
                    if (TabUiFeatureUtilities.isLaunchPolishEnabled()) {
                        assertEquals(3, listView.getCount());
                        verifyTabGridDialogToolbarMenuItem(listView, 2,
                                cta.getString(R.string.tab_grid_dialog_toolbar_edit_group_name));
                    } else {
                        assertEquals(2, listView.getCount());
                    }
                });
    }

    private void verifyTabGridDialogToolbarMenuItem(ListView listView, int index, String text) {
        View menuItemView = listView.getChildAt(index);
        TextView menuItemText = menuItemView.findViewById(R.id.menu_item_text);
        assertEquals(text, menuItemText.getText());
    }

    private void selectTabGridDialogToolbarMenuItem(ChromeTabbedActivity cta, String buttonText) {
        onView(withText(buttonText))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
    }

    private void triggerShareGroupAndVerify(ChromeTabbedActivity cta) {
        Intents.init();
        selectTabGridDialogToolbarMenuItem(cta, "Share group");
        intended(allOf(hasAction(equalTo(Intent.ACTION_CHOOSER)),
                hasExtras(hasEntry(equalTo(Intent.EXTRA_INTENT),
                        allOf(hasAction(equalTo(Intent.ACTION_SEND)), hasType("text/plain"))))));
        Intents.release();
    }

    private void waitForDialogHidingAnimation(ChromeTabbedActivity cta) {
        CriteriaHelper.pollUiThread(() -> isDialogHiding(cta));
    }

    private void waitForDialogHidingAnimationInTabSwitcher(ChromeTabbedActivity cta) {
        waitForDialogHidingAnimation(cta);
        // Animation source card becomes alpha = 0f when dialog is showing and animates back to 1f
        // when dialog hides. Make sure the source card has restored its alpha change.
        CriteriaHelper.pollUiThread(() -> {
            RecyclerView recyclerView = cta.findViewById(R.id.tab_list_view);
            for (int i = 0; i < recyclerView.getAdapter().getItemCount(); i++) {
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(i);
                if (viewHolder == null) continue;
                if (viewHolder.itemView.getAlpha() != 1f) return false;
            }
            return true;
        });
    }

    private void openSelectionEditorAndVerify(ChromeTabbedActivity cta, int count) {
        // Open tab selection editor by selecting ungroup item in tab grid dialog menu.
        onView(withId(R.id.toolbar_menu_button))
                .perform(click());
        onView(withText(cta.getString(R.string.tab_grid_dialog_toolbar_remove_from_group)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());

        mSelectionEditorRobot.resultRobot.verifyTabSelectionEditorIsVisible()
                .verifyToolbarActionButtonDisabled()
                .verifyToolbarActionButtonWithResourceId(
                        R.string.tab_grid_dialog_selection_mode_remove)
                .verifyToolbarSelectionTextWithResourceId(
                        R.string.tab_selection_editor_toolbar_select_tabs)
                .verifyAdapterHasItemCount(count);
    }

    private void checkPosition(ChromeTabbedActivity cta, boolean isDialog, boolean isPortrait) {
        // If isDialog is true, we are checking the position of TabGridDialog; otherwise we are
        // checking the position of TabSelectionEditor.
        int contentViewId = isDialog ? R.id.dialog_container_view : R.id.selectable_list;
        int smallMargin =
                (int) cta.getResources().getDimension(R.dimen.tab_grid_dialog_side_margin);
        int largeMargin = (int) cta.getResources().getDimension(R.dimen.tab_grid_dialog_top_margin);
        int topMargin = isPortrait ? largeMargin : smallMargin;
        int sideMargin = isPortrait ? smallMargin : largeMargin;
        View parentView = cta.getCompositorViewHolder();
        Rect parentRect = new Rect();
        parentView.getGlobalVisibleRect(parentRect);

        onView(isDialog ? withId(contentViewId) : withId(contentViewId)).check((v, e) -> {
            int[] location = new int[2];
            v.getLocationOnScreen(location);
            // Check the position.
            assertEquals(sideMargin, location[0]);
            assertEquals(topMargin + parentRect.top, location[1]);
            // Check the size.
            assertEquals(parentView.getHeight() - 2 * topMargin, v.getHeight());
            assertEquals(parentView.getWidth() - 2 * sideMargin, v.getWidth());
        });
    }

    private void editDialogTitle(ChromeTabbedActivity cta, String title) {
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title)))
                .perform(click())
                .check((v, e) -> {
                    // Verify all texts in the field are selected.
                    EditText titleView = (EditText) v;
                    assertEquals(titleView.getText().length(),
                            titleView.getSelectionEnd() - titleView.getSelectionStart());
                })
                .perform(replaceText(title))
                .perform(pressImeActionButton());
        // Wait until the keyboard is hidden to make sure the edit has taken effect.
        KeyboardVisibilityDelegate delegate = KeyboardVisibilityDelegate.getInstance();
        CriteriaHelper.pollUiThread(
                () -> !delegate.isKeyboardShowing(cta, cta.getCompositorViewHolder()));
    }

    private void verifyFirstCardTitle(String title) {
        onView(allOf(withParent(withId(R.id.compositor_view_holder)), withId(R.id.tab_list_view)))
                .check((v, noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;

                    RecyclerView recyclerView = (RecyclerView) v;
                    TextView firstCardTitleTextView =
                            recyclerView.findViewHolderForAdapterPosition(0).itemView.findViewById(
                                    R.id.tab_title);
                    assertEquals(title, firstCardTitleTextView.getText().toString());
                });
    }

    private void clickScrimToExitDialog(ChromeTabbedActivity cta) throws ExecutionException {
        CriteriaHelper.pollUiThread(() -> isDialogShowing(cta));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            View scrimView =
                    cta.getRootUiCoordinatorForTesting().getScrimCoordinator().getViewForTesting();
            scrimView.performClick();
        });
    }

    private void verifyBackgroundViewAccessibilityImportance(
            ChromeTabbedActivity cta, boolean isDialogShowing) {
        View controlContainer = cta.findViewById(R.id.control_container);
        assertEquals(isDialogShowing ? IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                                     : IMPORTANT_FOR_ACCESSIBILITY_AUTO,
                controlContainer.getImportantForAccessibility());
        View bottomControls = cta.findViewById(R.id.bottom_controls);
        assertEquals(isDialogShowing ? IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                                     : IMPORTANT_FOR_ACCESSIBILITY_AUTO,
                bottomControls.getImportantForAccessibility());
        View compositorViewHolder = cta.getCompositorViewHolder();
        assertEquals(isDialogShowing ? IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                                     : IMPORTANT_FOR_ACCESSIBILITY_AUTO,
                compositorViewHolder.getImportantForAccessibility());
        View bottomContainer = cta.findViewById(R.id.bottom_container);
        assertEquals(isDialogShowing ? IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
                                     : IMPORTANT_FOR_ACCESSIBILITY_AUTO,
                bottomContainer.getImportantForAccessibility());
    }

    private void verifyDialogUndoBarAndClick() {
        // Verify that the dialog undo bar is showing and the default undo bar is hidden.
        onViewWaiting(allOf(withId(R.id.snackbar_button),
                isDescendantOfA(withId(R.id.dialog_snack_bar_container_view)), isDisplayed()));
        onView(allOf(withId(R.id.snackbar), isDescendantOfA(withId(R.id.bottom_container))))
                .check(doesNotExist());
        onView(allOf(withId(R.id.snackbar_button),
                       isDescendantOfA(withId(R.id.dialog_snack_bar_container_view)),
                       isDisplayed()))
                .perform(click());
    }

    private void verifyDialogBackButtonContentDescription(ChromeTabbedActivity cta, String s) {
        assertTrue(isDialogShowing(cta));
        onView(allOf(withId(R.id.toolbar_left_button),
                       isDescendantOfA(withId(R.id.dialog_container_view))))
                .check((v, e) -> assertEquals(s, v.getContentDescription()));
    }

    private void testTitleTextFocus(ChromeTabbedActivity cta) throws ExecutionException {
        // Click the text field to grab focus and click back button to lose focus.
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title))).perform(click());
        verifyTitleTextFocus(cta, true);
        Espresso.pressBack();
        verifyTitleTextFocus(cta, false);
        verifyShowingDialog(cta, 2, null);

        // Use toolbar menu to grab focus and click back button to lose focus.
        openDialogToolbarMenuAndVerify(cta);
        selectTabGridDialogToolbarMenuItem(cta, "Edit group name");
        verifyTitleTextFocus(cta, true);
        Espresso.pressBack();
        verifyTitleTextFocus(cta, false);
        verifyShowingDialog(cta, 2, null);

        // Click the text field to grab focus and click scrim to lose focus.
        onView(allOf(withParent(withId(R.id.main_content)), withId(R.id.title))).perform(click());
        verifyTitleTextFocus(cta, true);
        clickScrimToExitDialog(cta);
        waitForDialogHidingAnimation(cta);
        verifyTitleTextFocus(cta, false);
    }

    private void verifyTitleTextFocus(ChromeTabbedActivity cta, boolean shouldFocus) {
        CriteriaHelper.pollUiThread(() -> {
            View titleTextView = cta.findViewById(R.id.tab_group_toolbar).findViewById(R.id.title);
            KeyboardVisibilityDelegate delegate = KeyboardVisibilityDelegate.getInstance();
            boolean keyboardVisible =
                    delegate.isKeyboardShowing(cta, cta.getCompositorViewHolder());
            boolean isFocused = titleTextView.isFocused();
            return (!shouldFocus ^ isFocused) && (!shouldFocus ^ keyboardVisible);
        });
    }
}
