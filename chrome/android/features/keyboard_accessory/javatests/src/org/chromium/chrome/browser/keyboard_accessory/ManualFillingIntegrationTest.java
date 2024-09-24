// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItem;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertNotNull;

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabWithDescription;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.waitToBeHidden;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import android.view.View;
import android.view.ViewGroup;

import androidx.test.espresso.Espresso;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupView;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.ui.messages.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Integration tests for keyboard accessory and accessory sheet with other Chrome components. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManualFillingIntegrationTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    @After
    public void tearDown() {
        mHelper.clear();
    }

    @Test
    @SmallTest
    public void testAccessoryIsAvailable() {
        mHelper.loadTestPage(false);

        assertNotNull(
                "Controller for Manual filling should be available.",
                mHelper.getManualFillingCoordinator());
    }

    @Test
    @SmallTest
    public void testKeyboardAccessoryHiddenUntilKeyboardShows() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        onView(withId(R.id.keyboard_accessory)).check(doesNotExist());
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Check that ONLY the accessory is there but the sheet is still hidden.
        whenDisplayed(withId(R.id.keyboard_accessory));
        onView(withChild(withId(R.id.keyboard_accessory_sheet_frame))).check(doesNotExist());
    }

    @Test
    @SmallTest
    public void testKeyboardAccessoryDisappearsWithKeyboard() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(withId(R.id.keyboard_accessory));

        // Dismiss the keyboard to hide the accessory again.
        mHelper.clickSubmit();
        mHelper.waitForKeyboardAccessoryToDisappear();
    }

    @Test
    @SmallTest
    public void testAccessorySheetHiddenUntilManuallyTriggered() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Check that ONLY the accessory is there but the sheet is still hidden.
        whenDisplayed(withId(R.id.keyboard_accessory));
        onView(withChild(withId(R.id.keyboard_accessory_sheet_frame))).check(doesNotExist());

        // Trigger the sheet and wait for it to open and the keyboard to disappear.
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabWithDescription(
                                        R.string.password_accessory_sheet_toggle)));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
    }

    @Test
    @SmallTest
    public void testAccessorySheetShown() throws TimeoutException {
        mHelper.loadTestPage(false);
        // Register a sheet data provider so that sheet is available when needed.
        mHelper.registerSheetDataProvider(AccessoryTabType.CREDIT_CARDS);

        // Show the passwords accessory sheet without focusing on any fields.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mHelper.getManualFillingCoordinator()
                                .showAccessorySheetTab(AccessoryTabType.CREDIT_CARDS));

        // Verify that the accessory sheet is shown.
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
    }

    @Test
    @SmallTest
    public void testAccessorySheetHiddenWhenRefocusingField() throws TimeoutException {
        AtomicReference<ViewGroup.MarginLayoutParams> accessoryMargins = new AtomicReference<>();
        AtomicReference<View> accessorySheetView = new AtomicReference<>();
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Check that ONLY the accessory is there but the sheet is still hidden.
        whenDisplayed(withId(R.id.keyboard_accessory))
                .check(
                        (view, e) -> {
                            accessoryMargins.set(
                                    (ViewGroup.MarginLayoutParams) view.getLayoutParams());
                            assertThat(
                                    accessoryMargins.get().bottomMargin,
                                    is(0)); // Attached to keyboard.
                        });
        onView(withChild(withId(R.id.keyboard_accessory_sheet_frame))).check(doesNotExist());

        // Trigger the sheet and wait for it to open and the keyboard to disappear.
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabWithDescription(
                                        R.string.password_accessory_sheet_toggle)));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)))
                .check(
                        (view, e) -> {
                            accessorySheetView.set(view);
                        });

        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
        CriteriaHelper.pollUiThread(() -> accessoryMargins.get().bottomMargin == 0);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1466461")
    public void testAccessoryHiddenAfterTappingAutoGenerationButton() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory and add the generation button.
        mHelper.focusPasswordField();
        mHelper.signalAutoGenerationStatus(true);
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(isAssignableFrom(KeyboardAccessoryButtonGroupView.class))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)));

        // Click the generation button. This should hide the accessory sheet and bar.
        onView(withText(R.string.password_generation_accessory_button)).perform(click());
        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
        waitToBeHidden(withId(R.id.keyboard_accessory));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1406328,https://crbug.com/1466461")
    public void testHidingSheetBringsBackKeyboard() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(isAssignableFrom(KeyboardAccessoryButtonGroupView.class))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)));

        // Click the tab again to hide the sheet and show the keyboard.
        whenDisplayed(isAssignableFrom(KeyboardAccessoryButtonGroupView.class))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardAccessoryToBeShown();
        onView(withId(R.id.keyboard_accessory)).check(matches(isDisplayed()));
        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
    }

    @Test
    @SmallTest
    public void testSelectingNonPasswordInputDismissesAccessory() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the password field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(isAssignableFrom(KeyboardAccessoryButtonGroupView.class));

        // Clicking a field without completion hides the accessory again.
        mHelper.clickFieldWithoutCompletion();
        mHelper.waitForKeyboardAccessoryToDisappear();
    }

    @Test
    @SmallTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testInvokingTabSwitcherHidesAccessory() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabWithDescription(
                                        R.string.password_accessory_sheet_toggle)));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)));

        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.TAB_SWITCHER, false);

        LayoutTestUtils.startShowingAndWaitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING, false);

        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
    }

    @Test
    @SmallTest
    public void testResumingTheAppDismissesAllInputMethods() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabWithDescription(
                                        R.string.password_accessory_sheet_toggle)));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)));

        // Simulate backgrounding the main activity.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().onPauseWithNative();
                });

        // This should completely dismiss any input method.
        mHelper.waitForKeyboardToDisappear();
        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
        mHelper.waitForKeyboardAccessoryToDisappear();

        // Simulate foregrounding the main activity.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().onResumeWithNative();
                });

        // Clicking the field should bring the accessory back up.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabWithDescription(
                                        R.string.password_accessory_sheet_toggle)));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
    }

    @Test
    @SmallTest
    public void testPressingBackButtonHidesAccessorySheet() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabWithDescription(
                                        R.string.password_accessory_sheet_toggle)));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)));

        Espresso.pressBack();

        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
        mHelper.waitForKeyboardAccessoryToDisappear();
    }

    @Test
    @SmallTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // crbug.com/362214348
    public void testInfobarStaysHiddenWhileChangingFieldsWithOpenKeyboard()
            throws TimeoutException {
        mHelper.loadTestPage(false);

        // Initialize and wait for the infobar.
        InfoBarTestAnimationListener listener = new InfoBarTestAnimationListener();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getInfoBarContainer().addAnimationListener(listener));
        final String kInfoBarText = "SomeInfoBar";
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    SimpleConfirmInfoBarBuilder.create(
                            mActivityTestRule.getActivity().getActivityTab().getWebContents(),
                            InfoBarIdentifier.DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID,
                            kInfoBarText,
                            false);
                });
        listener.addInfoBarAnimationFinished("InfoBar not added.");
        whenDisplayed(withText(kInfoBarText));

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(not(View.VISIBLE)));

        // Clicking another field hides the accessory, but the InfoBar should remain invisible.
        mHelper.clickEmailField(false);
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(not(View.VISIBLE)));

        // Close the keyboard to bring back the InfoBar.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getKeyboardDelegate()
                            .hideKeyboard(mActivityTestRule.getActivity().getCurrentFocus());
                    mActivityTestRule
                            .getInfoBarContainer()
                            .getContainerViewForTesting()
                            .requestLayout();
                });

        mHelper.waitForKeyboardToDisappear();
        mHelper.waitForKeyboardAccessoryToDisappear();

        whenDisplayed(withText(kInfoBarText));
    }

    @Test
    @SmallTest
    public void testInfobarStaysHiddenWhenOpeningSheet() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Initialize and wait for the infobar.
        InfoBarTestAnimationListener listener = new InfoBarTestAnimationListener();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getInfoBarContainer().addAnimationListener(listener));
        final String kInfoBarText = "SomeInfoBar";
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    SimpleConfirmInfoBarBuilder.create(
                            mActivityTestRule.getActivity().getActivityTab().getWebContents(),
                            InfoBarIdentifier.DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID,
                            kInfoBarText,
                            false);
                });
        listener.addInfoBarAnimationFinished("InfoBar not added.");
        whenDisplayed(withText(kInfoBarText));

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(not(View.VISIBLE)));

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabWithDescription(
                                        R.string.password_accessory_sheet_toggle)));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(not(View.VISIBLE)));

        // Reopen the keyboard, then close it.
        whenDisplayed(withId(R.id.show_keyboard)).perform(click());
        mHelper.waitForKeyboardAccessoryToBeShown();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getKeyboardDelegate()
                            .hideKeyboard(mActivityTestRule.getActivity().getCurrentFocus());
                    mActivityTestRule
                            .getInfoBarContainer()
                            .getContainerViewForTesting()
                            .requestLayout();
                });

        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
        mHelper.waitForKeyboardAccessoryToDisappear();

        whenDisplayed(withText(kInfoBarText));
    }

    @Test
    @SmallTest
    public void testMovesUpSnackbar() throws TimeoutException {
        final String kSnackbarText = "snackbar";

        mHelper.loadTestPage(false);

        // Create a simple, persistent snackbar and verify it's displayed.
        SnackbarManager manager = mActivityTestRule.getActivity().getSnackbarManager();
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () ->
                        manager.showSnackbar(
                                Snackbar.make(
                                        kSnackbarText,
                                        new SnackbarManager.SnackbarController() {},
                                        Snackbar.TYPE_PERSISTENT,
                                        Snackbar.UMA_TEST_SNACKBAR)));
        CriteriaHelper.pollUiThread(manager::isShowing);
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getWindowAndroid()::haveAnimationsEnded);

        // Click in a field to open keyboard and accessory -- this shouldn't hide the snackbar.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(withId(R.id.keyboard_accessory));
        onView(withText(kSnackbarText)).check(matches(isCompletelyDisplayed()));

        // Open a keyboard accessory sheet -- this also shouldn't hide the snackbar.
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabWithDescription(
                                        R.string.password_accessory_sheet_toggle)));
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
        onView(withText(kSnackbarText)).check(matches(isCompletelyDisplayed()));

        // Click into a field without completion to dismiss the keyboard accessory.
        mHelper.clickFieldWithoutCompletion();
        mHelper.waitForKeyboardAccessoryToDisappear();
        onView(withText(kSnackbarText)).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @SmallTest
    public void testInfobarReopensOnPressingBack() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Initialize and wait for the infobar.
        InfoBarTestAnimationListener listener = new InfoBarTestAnimationListener();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mActivityTestRule.getInfoBarContainer().addAnimationListener(listener));
        final String kInfoBarText = "SomeInfoBar";
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    SimpleConfirmInfoBarBuilder.create(
                            mActivityTestRule.getActivity().getActivityTab().getWebContents(),
                            InfoBarIdentifier.DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID,
                            kInfoBarText,
                            false);
                });
        listener.addInfoBarAnimationFinished("InfoBar not added.");
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(View.VISIBLE));

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabWithDescription(
                                        R.string.password_accessory_sheet_toggle)));
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(not(View.VISIBLE)));

        // Close the accessory using the back button. The Infobar should reappear.
        Espresso.pressBack();

        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
        mHelper.waitForKeyboardAccessoryToDisappear();

        whenDisplayed(withText(kInfoBarText));
    }
}
