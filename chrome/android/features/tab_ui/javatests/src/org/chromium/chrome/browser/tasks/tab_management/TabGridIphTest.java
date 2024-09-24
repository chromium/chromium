// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeFirstTabInTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.createTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.getSwipeToDismissAction;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.prepareTabsWithThumbnail;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.content.res.Configuration;
import android.graphics.drawable.Animatable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.io.IOException;

/** End-to-end tests for TabGridIph component. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "enable-features=IPH_TabGroupsDragAndDrop<TabGroupsDragAndDrop",
    "force-fieldtrials=TabGroupsDragAndDrop/Enabled",
    "force-fieldtrial-params=TabGroupsDragAndDrop.Enabled:availability/any/"
            + "event_trigger/"
            + "name%3Aiph_tabgroups_drag_and_drop;comparator%3A==0;window%3A30;storage%3A365/"
            + "event_trigger2/"
            + "name%3Aiph_tabgroups_drag_and_drop;comparator%3A<2;window%3A90;storage%3A365/"
            + "event_used/"
            + "name%3Atab_drag_and_drop_to_group;comparator%3A==0;window%3A365;storage%3A365/"
            + "session_rate/<1"
})
@Restriction({DeviceFormFactor.PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
// Remove the ANDROID_HUB_FLOATING_ACTION_BUTTON restriction and regenerate goldens when launching.
@DisableFeatures({
    ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON,
    ChromeFeatureList.ANDROID_HUB_SEARCH
})
@DoNotBatch(reason = "Batching can cause message state to leak between tests.")
public class TabGridIphTest {
    private ModalDialogManager mModalDialogManager;
    private Tracker mTracker;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(
                            ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID)
                    .setRevision(2)
                    .build();

    @Before
    public void setUp() {
        IphMessageService.setSkipIphInTestsForTesting(false);
        mActivityTestRule.startMainActivityOnBlankPage();
        TabUiTestHelper.verifyTabSwitcherLayoutType(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized);
        mModalDialogManager =
                ThreadUtils.runOnUiThreadBlocking(
                        mActivityTestRule.getActivity()::getModalDialogManager);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTracker =
                            TrackerFactory.getTrackerForProfile(
                                    mActivityTestRule.getProfile(false));
                });
        CriteriaHelper.pollUiThread(mTracker::isInitialized);
        CriteriaHelper.pollUiThread(
                () -> {
                    return mTracker.wouldTriggerHelpUI(
                            FeatureConstants.TAB_GROUPS_DRAG_AND_DROP_FEATURE);
                });
    }

    @After
    public void tearDown() {
        ActivityTestUtils.clearActivityOrientation(mActivityTestRule.getActivity());
        ThreadUtils.runOnUiThreadBlocking(
                TabSwitcherMessageManager::resetHasAppendedMessagesForTesting);
    }

    @Test
    @MediumTest
    public void testShowAndHideIphDialog() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        // Check the IPH message card is showing and open the IPH dialog.
        onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        verifyIphDialogShowing(cta);

        // Exit by clicking the "OK" button.
        exitIphDialogByClickingButton(cta);
        verifyIphDialogHiding(cta);

        // Check the IPH message card is showing and open the IPH dialog.
        onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        verifyIphDialogShowing(cta);

        // Press back should dismiss the IPH dialog.
        pressBack();
        verifyIphDialogHiding(cta);
        onView(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Check the IPH message card is showing and open the IPH dialog.
        onView(allOf(withId(R.id.action_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        verifyIphDialogShowing(cta);

        // Click outside of the dialog area to close the IPH dialog.
        View dialogView =
                mModalDialogManager
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        int[] location = new int[2];
        // Get the position of the dialog view and click slightly above so that we essentially click
        // on the scrim.
        dialogView.getLocationOnScreen(location);
        UiDevice.getInstance(InstrumentationRegistry.getInstrumentation())
                .click(location[0], location[1] / 2);
        verifyIphDialogHiding(cta);
    }

    @Test
    @MediumTest
    public void testIphItemShowingInIncognito() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        createTabs(cta, true, 1);
        enterTabSwitcher(cta);
        assertTrue(cta.getTabModelSelector().getCurrentModel().isIncognito());
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testDismissIphItem() throws Exception {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Restart chrome to verify that IPH message card is still there.
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityFromLauncher();
        cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Remove the message card and dismiss the feature by clicking close button.
        onView(allOf(withId(R.id.close_button), withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());

        // Restart chrome to verify that IPH message card no longer shows.
        TabUiTestHelper.finishActivity(mActivityTestRule.getActivity());
        mActivityTestRule.startMainActivityFromLauncher();
        cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderIph_Portrait() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        ChromeRenderTestRule.sanitize(cta.findViewById(R.id.tab_grid_message_item));
        mRenderTestRule.render(
                cta.findViewById(R.id.tab_grid_message_item), "iph_entrance_portrait");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderIph_Landscape() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onViewWaiting(
                        allOf(
                                isDescendantOfA(
                                        withId(TabUiTestHelper.getTabSwitcherAncestorId(cta))),
                                withId(R.id.tab_list_recycler_view)))
                .perform(RecyclerViewActions.scrollTo(withId(R.id.tab_grid_message_item)));
        onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        ChromeRenderTestRule.sanitize(cta.findViewById(R.id.tab_grid_message_item));
        mRenderTestRule.render(
                cta.findViewById(R.id.tab_grid_message_item), "iph_entrance_landscape");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderIphDialog_Portrait() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onViewWaiting(
                        allOf(
                                withId(R.id.action_button),
                                withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        verifyIphDialogShowing(cta);

        View iphDialogView =
                mModalDialogManager
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        // Freeze animation and wait until animation is really frozen.
        ChromeRenderTestRule.sanitize(iphDialogView);
        ImageView iphImageView = iphDialogView.findViewById(R.id.animation_drawable);
        Animatable iphAnimation = (Animatable) iphImageView.getDrawable();
        CriteriaHelper.pollUiThread(() -> !iphAnimation.isRunning());

        ChromeRenderTestRule.sanitize(iphDialogView);
        mRenderTestRule.render(iphDialogView, "iph_dialog_portrait");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testRenderIphDialog_Landscape() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        ActivityTestUtils.rotateActivityToOrientation(cta, Configuration.ORIENTATION_LANDSCAPE);

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        // Scroll to the position of the IPH entrance so that it is completely showing for Espresso
        // click.
        onViewWaiting(
                        allOf(
                                isDescendantOfA(
                                        withId(TabUiTestHelper.getTabSwitcherAncestorId(cta))),
                                withId(R.id.tab_list_recycler_view)))
                .perform(RecyclerViewActions.scrollToPosition(1));
        onViewWaiting(
                        allOf(
                                withId(R.id.action_button),
                                withParent(withId(R.id.tab_grid_message_item))))
                .perform(click());
        verifyIphDialogShowing(cta);

        View iphDialogView =
                mModalDialogManager
                        .getCurrentDialogForTest()
                        .get(ModalDialogProperties.CUSTOM_VIEW);
        // Freeze animation and wait until animation is really frozen.
        ChromeRenderTestRule.sanitize(iphDialogView);
        ImageView iphImageView = iphDialogView.findViewById(R.id.animation_drawable);
        Animatable iphAnimation = (Animatable) iphImageView.getDrawable();
        CriteriaHelper.pollUiThread(() -> !iphAnimation.isRunning());

        ChromeRenderTestRule.sanitize(iphDialogView);
        mRenderTestRule.render(iphDialogView, "iph_dialog_landscape");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON)
    public void testIphMessageRenderedCorrectly_withFloatingActionButton() throws IOException {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        prepareTabsWithThumbnail(mActivityTestRule, 8, 0, null);

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        View view = cta.findViewById(R.id.pane_frame);
        mRenderTestRule.render(view, "iph_message_card");
    }

    @Test
    @MediumTest
    public void testIphItemChangeWithLastTab() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Close the last tab in tab switcher and the IPH item should not be showing.
        closeFirstTabInTabSwitcher(cta);
        CriteriaHelper.pollUiThread(
                () -> !TabSwitcherMessageManager.hasAppendedMessagesForTesting());
        verifyTabSwitcherCardCount(cta, 0);
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());

        // Undo the closure of the last tab and the IPH item should reshow.
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Close the last tab in the tab switcher.
        closeFirstTabInTabSwitcher(cta);
        CriteriaHelper.pollUiThread(
                () -> !TabSwitcherMessageManager.hasAppendedMessagesForTesting());
        verifyTabSwitcherCardCount(cta, 0);
        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());

        // Add the first tab to an empty tab switcher and the IPH item should show.
        ChromeTabUtils.newTabFromMenu(
                InstrumentationRegistry.getInstrumentation(), cta, false, true);
        enterTabSwitcher(cta);
        verifyTabSwitcherCardCount(cta, 1);
        CriteriaHelper.pollUiThread(TabSwitcherMessageManager::hasAppendedMessagesForTesting);
        onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Consistent failures despite revival effort in b/341267765")
    public void testSwipeToDismiss_IPH() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);
        onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));
        RecyclerView.ViewHolder viewHolder =
                ((RecyclerView) cta.findViewById(R.id.tab_list_recycler_view))
                        .findViewHolderForAdapterPosition(1);
        assertEquals(TabProperties.UiType.MESSAGE, viewHolder.getItemViewType());

        onView(
                        allOf(
                                isDescendantOfA(
                                        withId(TabUiTestHelper.getTabSwitcherAncestorId(cta))),
                                withId(R.id.tab_list_recycler_view)))
                .perform(
                        RecyclerViewActions.actionOnItemAtPosition(
                                1, getSwipeToDismissAction(true)));

        onView(withId(R.id.tab_grid_message_item)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Still flaky on arm builds despite revival effort in b/341267765")
    public void testNotShowIPHInMultiWindowMode() {
        ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        enterTabSwitcher(cta);
        onViewWaiting(withId(R.id.tab_grid_message_item)).check(matches(isDisplayed()));

        // Mock that user enters multi-window mode, and the IPH message should not show in tab
        // switcher.
        clickFirstCardFromTabSwitcher(cta);
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        enterTabSwitcher(cta);
        CriteriaHelper.pollUiThread(() -> cta.findViewById(R.id.tab_grid_message_item) == null);

        // Mock that user exits multi-window mode, and the IPH message should show in tab switcher.
        clickFirstCardFromTabSwitcher(cta);
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(false);
        enterTabSwitcher(cta);
        onViewWaiting(allOf(withId(R.id.tab_grid_message_item), isDisplayed()));
    }

    private void verifyIphDialogShowing(ChromeTabbedActivity cta) {
        // Verify IPH dialog view.
        onViewWaiting(withId(R.id.iph_dialog))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;

                            String title = cta.getString(R.string.iph_drag_and_drop_title);
                            assertEquals(title, ((TextView) v.findViewById(R.id.title)).getText());

                            String description = cta.getString(R.string.iph_drag_and_drop_content);
                            assertEquals(
                                    description,
                                    ((TextView) v.findViewById(R.id.description)).getText());
                        });
        // Verify ModalDialog button.
        onView(withId(R.id.positive_button))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check(matches(withText(cta.getString(R.string.ok))));
    }

    private void verifyIphDialogHiding(ChromeTabbedActivity cta) {
        onView(withId(R.id.iph_dialog)).check(doesNotExist());
    }

    private void exitIphDialogByClickingButton(ChromeTabbedActivity cta) {
        onView(withId(R.id.positive_button)).perform(click());
    }
}
