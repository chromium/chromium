// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.INVISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.Visibility.VISIBLE;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUPS_ANDROID;
import static org.chromium.chrome.browser.flags.ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickNthTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.finishActivity;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabStripFaviconCount;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.createTabStateFile;
import static org.chromium.chrome.features.start_surface.StartSurfaceTestUtils.createThumbnailBitmapAndWriteToFile;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
import static org.chromium.ui.test.util.ViewUtils.waitForView;

import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.NoMatchingRootException;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.bottomsheet.TestBottomSheetContent;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.IOException;
import java.util.concurrent.atomic.AtomicReference;

/** End-to-end tests for TabGroupUi component. */
@RunWith(ChromeJUnit4ClassRunner.class)
// clang-format off
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Features.EnableFeatures({TAB_GRID_LAYOUT_ANDROID, TAB_GROUPS_ANDROID,
    TAB_GROUPS_CONTINUATION_ANDROID})
public class TabGroupUiTest {
    // clang-format on

    private static final String TAB_GROUP_LAUNCH_BUG_FIX_PARAMS =
            "force-fieldtrial-params=Study.Group:enable_launch_bug_fix/true";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_TAB_GROUPS)
                    .setRevision(1)
                    .build();

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        TabUiTestHelper.verifyTabSwitcherLayoutType(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
    }

    @Test
    @MediumTest
    public void testStripShownOnGroupTabPage() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        createTabs(cta, false, 2);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 2);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the 1st tab in group.
        clickFirstCardFromTabSwitcher(cta);
        clickFirstTabInDialog(cta);
        assertFalse(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        onView(withId(R.id.bottom_controls)).check(matches(isDisplayed()));
        verifyTabStripFaviconCount(cta, 2);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/1208386")
    public void testRenderStrip_Select5thTabIn10Tabs() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        AtomicReference<RecyclerView> recyclerViewReference = new AtomicReference<>();
        TabUiTestHelper.addBlankTabs(cta, false, 9);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 10);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the 5th tab in group.
        clickFirstCardFromTabSwitcher(cta);
        clickNthTabInDialog(cta, 4);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup bottomToolbar = cta.findViewById(R.id.bottom_controls);
            RecyclerView stripRecyclerView = bottomToolbar.findViewById(R.id.tab_list_view);
            recyclerViewReference.set(stripRecyclerView);
        });
        mRenderTestRule.render(recyclerViewReference.get(), "5th_tab_selected");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    public void testRenderStrip_Select10thTabIn10Tabs() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        AtomicReference<RecyclerView> recyclerViewReference = new AtomicReference<>();
        TabUiTestHelper.addBlankTabs(cta, false, 9);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 10);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the 10th tab in group.
        clickFirstCardFromTabSwitcher(cta);
        clickNthTabInDialog(cta, 9);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup bottomToolbar = cta.findViewById(R.id.bottom_controls);
            RecyclerView stripRecyclerView = bottomToolbar.findViewById(R.id.tab_list_view);
            recyclerViewReference.set(stripRecyclerView);
        });
        mRenderTestRule.render(recyclerViewReference.get(), "10th_tab_selected");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/1208386")
    public void testRenderStrip_AddTab() throws IOException {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        AtomicReference<RecyclerView> recyclerViewReference = new AtomicReference<>();
        TabUiTestHelper.addBlankTabs(cta, false, 9);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 10);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);

        // Select the first tab in group and add one new tab to group.
        clickFirstCardFromTabSwitcher(cta);
        clickNthTabInDialog(cta, 0);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            ViewGroup bottomToolbar = cta.findViewById(R.id.bottom_controls);
            RecyclerView stripRecyclerView = bottomToolbar.findViewById(R.id.tab_list_view);
            recyclerViewReference.set(stripRecyclerView);
            // Disable animation to reduce flakiness.
            stripRecyclerView.setItemAnimator(null);
        });
        onView(allOf(withId(R.id.toolbar_right_button), withParent(withId(R.id.main_content)),
                       withEffectiveVisibility(VISIBLE)))
                .perform(click());
        mRenderTestRule.render(recyclerViewReference.get(), "11th_tab_selected");
    }

    @Test
    @MediumTest
    // clang-format off
    @Features.EnableFeatures({ChromeFeatureList.TAB_GROUPS_CONTINUATION_ANDROID + "<Study"})
    @CommandLineFlags.Add({"force-fieldtrials=Study/Group", TAB_GROUP_LAUNCH_BUG_FIX_PARAMS})
    public void testVisibilityChangeWithOmnibox() throws Exception {
        // clang-format on

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
        waitForView(allOf(withId(R.id.tab_list_view), isDescendantOfA(withId(R.id.bottom_controls)),
                isCompletelyDisplayed()));

        // The strip should be hidden when omnibox is focused.
        onView(withId(R.id.url_bar)).perform(click());
        onView(allOf(withId(R.id.tab_list_view), isDescendantOfA(withId(R.id.bottom_controls))))
                .check(matches(withEffectiveVisibility((INVISIBLE))));
    }

    @Test
    @MediumTest
    // clang-format off
    @CommandLineFlags.Add({
            "enable-features=IPH_TabGroupsTapToSeeAnotherTab<TabGroupsTapToSeeAnotherTab",
            "force-fieldtrials=TabGroupsTapToSeeAnotherTab/Enabled/",
            "force-fieldtrial-params=TabGroupsTapToSeeAnotherTab.Enabled:availability/any/" +
                    "event_trigger/" +
                    "name%3Aiph_tabgroups_strip;comparator%3A==0;window%3A30;storage%3A365/" +
                    "event_trigger2/" +
                    "name%3Aiph_tabgroups_strip;comparator%3A<2;window%3A90;storage%3A365/" +
                    "event_used/" +
                    "name%3Aiph_tabgroups_strip;comparator%3A==0;window%3A365;storage%3A365/" +
                    "session_rate/<1"})
    @DisabledTest(message = "https://crbug.com/1145060")
    public void testIphSuppressedByBottomSheet() throws Exception {
        // clang-format on

        // Create a tab group with 2 tabs, and turn on enable_launch_bug_fix variation.
        finishActivity(mActivityTestRule.getActivity());
        createThumbnailBitmapAndWriteToFile(0);
        createThumbnailBitmapAndWriteToFile(1);
        TabAttributeCache.setRootIdForTesting(0, 0);
        TabAttributeCache.setRootIdForTesting(1, 0);
        createTabStateFile(new int[] {0, 1});
        TabUiFeatureUtilities.ENABLE_LAUNCH_BUG_FIX.setForTesting(true);

        // Restart Chrome and make sure both tab strip and IPH text bubble are showing.
        mActivityTestRule.startMainActivityFromLauncher();
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized);
        waitForView(allOf(withId(R.id.tab_list_view), isDescendantOfA(withId(R.id.bottom_controls)),
                isCompletelyDisplayed()));
        CriteriaHelper.pollInstrumentationThread(() -> isTabStripIphShowing(cta));

        // Show a dummy bottom sheet, and the IPH should be hidden.
        final BottomSheetController bottomSheetController =
                cta.getRootUiCoordinatorForTesting().getBottomSheetController();
        final BottomSheetTestSupport bottomSheetTestSupport =
                new BottomSheetTestSupport(bottomSheetController);
        runOnUiThreadBlocking(() -> {
            TestBottomSheetContent bottomSheetContent =
                    new TestBottomSheetContent(cta, BottomSheetContent.ContentPriority.HIGH, false);
            bottomSheetController.requestShowContent(bottomSheetContent, false);
        });
        CriteriaHelper.pollUiThread(
                () -> bottomSheetController.getSheetState() != SheetState.HIDDEN);
        assertFalse(isTabStripIphShowing(cta));

        // Hide the dummy bottom sheet, and the IPH should reshow.
        runOnUiThreadBlocking(() -> bottomSheetTestSupport.setSheetState(SheetState.HIDDEN, false));
        CriteriaHelper.pollUiThread(
                () -> bottomSheetController.getSheetState() == SheetState.HIDDEN);
        CriteriaHelper.pollInstrumentationThread(() -> isTabStripIphShowing(cta));

        // When the IPH is clicked and dismissed, opening bottom sheet should never reshow it.
        onView(withText(cta.getString(R.string.iph_tab_groups_tap_to_see_another_tab_text)))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .perform(click());
        CriteriaHelper.pollInstrumentationThread(() -> !isTabStripIphShowing(cta));
        runOnUiThreadBlocking(() -> {
            TestBottomSheetContent bottomSheetContent =
                    new TestBottomSheetContent(cta, BottomSheetContent.ContentPriority.HIGH, false);
            bottomSheetController.requestShowContent(bottomSheetContent, false);
        });
        CriteriaHelper.pollUiThread(
                () -> bottomSheetController.getSheetState() != SheetState.HIDDEN);
        assertFalse(isTabStripIphShowing(cta));
    }

    private boolean isTabStripIphShowing(ChromeTabbedActivity cta) {
        String iphText = cta.getString(R.string.iph_tab_groups_tap_to_see_another_tab_text);
        boolean isShowing = true;
        try {
            onView(withText(iphText))
                    .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                    .check(matches(isDisplayed()));
        } catch (NoMatchingRootException e) {
            isShowing = false;
        } catch (Exception e) {
            assert false : "error when inspecting IPH text bubble.";
        }
        return isShowing;
    }
}
