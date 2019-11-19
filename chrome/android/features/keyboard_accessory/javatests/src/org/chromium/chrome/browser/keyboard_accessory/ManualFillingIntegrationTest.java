// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.matcher.ViewMatchers.assertThat;
import static android.support.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withChild;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertNotNull;

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.waitToBeHidden;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabTestHelper.isKeyboardAccessoryTabLayout;

import android.support.test.espresso.Espresso;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup;

import org.junit.After;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.InfoBarTestAnimationListener;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Integration tests for keyboard accessory and accessory sheet with other Chrome components.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
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

        assertNotNull("Controller for Manual filling should be available.",
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
        onView(withChild(withId(R.id.keyboard_accessory_sheet))).check(doesNotExist());
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
        onView(withChild(withId(R.id.keyboard_accessory_sheet))).check(doesNotExist());

        // Trigger the sheet and wait for it to open and the keyboard to disappear.
        onView(allOf(isDisplayed(), isKeyboardAccessoryTabLayout()))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));
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
        whenDisplayed(withId(R.id.keyboard_accessory)).check((view, e) -> {
            accessoryMargins.set((ViewGroup.MarginLayoutParams) view.getLayoutParams());
            assertThat(accessoryMargins.get().bottomMargin, is(0)); // Attached to keyboard.
        });
        onView(withChild(withId(R.id.keyboard_accessory_sheet))).check(doesNotExist());

        // Trigger the sheet and wait for it to open and the keyboard to disappear.
        onView(allOf(isDisplayed(), isKeyboardAccessoryTabLayout()))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet))).check((view, e) -> {
            accessorySheetView.set(view);
        });
        // The accessory bar is now pushed up by the accessory.
        CriteriaHelper.pollUiThread(() -> {
            return accessoryMargins.get().bottomMargin == accessorySheetView.get().getHeight();
        });

        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
        CriteriaHelper.pollUiThread(() -> accessoryMargins.get().bottomMargin == 0);
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
    public void testAccessoryHiddenAfterTappingAutoGenerationButton() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory and add the generation button.
        mHelper.focusPasswordField();
        mHelper.signalAutoGenerationStatus(true);
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(isKeyboardAccessoryTabLayout()).perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));

        // Click the generation button. This should hide the accessory sheet and bar.
        onView(withText(R.string.password_generation_accessory_button)).perform(click());
        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
        waitToBeHidden(withId(R.id.keyboard_accessory));
    }

    @Test
    @SmallTest
    @Features.DisableFeatures(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)
    public void testHidingSheetBringsBackKeyboard() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(isDisplayed(), isKeyboardAccessoryTabLayout()))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));

        // Click the tab again to hide the sheet and show the keyboard.
        onView(allOf(isDisplayed(), isKeyboardAccessoryTabLayout()))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardAccessoryToBeShown();
        onView(withId(R.id.keyboard_accessory)).check(matches(isDisplayed()));
        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
    }

    @Test
    @SmallTest
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
    public void testSelectingNonPasswordInputDismissesAccessory() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the password field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(allOf(isDisplayed(), isKeyboardAccessoryTabLayout()));

        // Clicking the email field hides the accessory again.
        mHelper.clickEmailField(false);
        mHelper.waitForKeyboardAccessoryToDisappear();
    }

    @Test
    @SmallTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    public void testInvokingTabSwitcherHidesAccessory() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(isDisplayed(), isKeyboardAccessoryTabLayout()))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getLayoutManager().showOverview(false); });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().getLayoutManager().hideOverview(false); });

        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
    }

    @Test
    @SmallTest
    public void testResumingTheAppDismissesAllInputMethods() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(isDisplayed(), isKeyboardAccessoryTabLayout()))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));

        // Simulate backgrounding the main activity.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().onPauseWithNative(); });

        // This should completely dismiss any input method.
        mHelper.waitForKeyboardToDisappear();
        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
        mHelper.waitForKeyboardAccessoryToDisappear();

        // Simulate foregrounding the main activity.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivityTestRule.getActivity().onResumeWithNative(); });

        // Clicking the field should bring the accessory back up.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(isDisplayed(), isKeyboardAccessoryTabLayout()))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));
    }

    @Test
    @SmallTest
    public void testPressingBackButtonHidesAccessorySheet() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(isDisplayed(), isKeyboardAccessoryTabLayout()))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));

        Espresso.pressBack();

        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
        mHelper.waitForKeyboardAccessoryToDisappear();
    }

    @Test
    @SmallTest
    public void testInfobarStaysHiddenWhileChangingFieldsWithOpenKeybaord()
            throws TimeoutException {
        mHelper.loadTestPage(false);

        // Initialize and wait for the infobar.
        InfoBarTestAnimationListener listener = new InfoBarTestAnimationListener();
        mActivityTestRule.getInfoBarContainer().addAnimationListener(listener);
        final String kInfoBarText = "SomeInfoBar";
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            SimpleConfirmInfoBarBuilder.create(mActivityTestRule.getActivity().getActivityTab(),
                    InfoBarIdentifier.DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID, kInfoBarText,
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
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getKeyboardDelegate().hideKeyboard(
                    mActivityTestRule.getActivity().getCurrentFocus());
            mActivityTestRule.getInfoBarContainer().getContainerViewForTesting().requestLayout();
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
        mActivityTestRule.getInfoBarContainer().addAnimationListener(listener);
        final String kInfoBarText = "SomeInfoBar";
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            SimpleConfirmInfoBarBuilder.create(mActivityTestRule.getActivity().getActivityTab(),
                    InfoBarIdentifier.DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID, kInfoBarText,
                    false);
        });
        listener.addInfoBarAnimationFinished("InfoBar not added.");
        whenDisplayed(withText(kInfoBarText));

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(not(View.VISIBLE)));

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(isDisplayed(), isKeyboardAccessoryTabLayout()))
                .perform(selectTabAtPosition(0));
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(not(View.VISIBLE)));

        // Reopen the keyboard, then close it.
        whenDisplayed(withId(R.id.show_keyboard)).perform(click());
        mHelper.waitForKeyboardAccessoryToBeShown();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getKeyboardDelegate().hideKeyboard(
                    mActivityTestRule.getActivity().getCurrentFocus());
            mActivityTestRule.getInfoBarContainer().getContainerViewForTesting().requestLayout();
        });

        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
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
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                ()
                        -> manager.showSnackbar(Snackbar.make(kSnackbarText,
                                new SnackbarManager.SnackbarController() {},
                                Snackbar.TYPE_PERSISTENT, Snackbar.UMA_TEST_SNACKBAR)));
        CriteriaHelper.pollUiThread(manager::isShowing);
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getWindowAndroid()::haveAnimationsEnded);

        // Click in a field to open keyboard and accessory -- this shouldn't hide the snackbar.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(withId(R.id.keyboard_accessory));
        onView(withText(kSnackbarText)).check(matches(isCompletelyDisplayed()));

        // Open a keyboard accessory sheet -- this also shouldn't hide the snackbar.
        whenDisplayed(allOf(isDisplayed(), isKeyboardAccessoryTabLayout()))
                .perform(selectTabAtPosition(0));
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));
        onView(withText(kSnackbarText)).check(matches(isCompletelyDisplayed()));

        // Click into the email field to dismiss the keyboard accessory.
        mHelper.clickEmailField(false);
        mHelper.waitForKeyboardAccessoryToDisappear();
        onView(withText(kSnackbarText)).check(matches(isCompletelyDisplayed()));
    }

    @Test
    @SmallTest
    public void testInfobarReopensOnPressingBack() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Initialize and wait for the infobar.
        InfoBarTestAnimationListener listener = new InfoBarTestAnimationListener();
        mActivityTestRule.getInfoBarContainer().addAnimationListener(listener);
        final String kInfoBarText = "SomeInfoBar";
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            SimpleConfirmInfoBarBuilder.create(mActivityTestRule.getActivity().getActivityTab(),
                    InfoBarIdentifier.DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID, kInfoBarText,
                    false);
        });
        listener.addInfoBarAnimationFinished("InfoBar not added.");
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(View.VISIBLE));

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();
        whenDisplayed(allOf(isDisplayed(), isKeyboardAccessoryTabLayout()))
                .perform(selectTabAtPosition(0));
        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));
        assertThat(mActivityTestRule.getInfoBarContainer().getVisibility(), is(not(View.VISIBLE)));

        // Close the accessory using the back button. The Infobar should reappear.
        Espresso.pressBack();

        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
        mHelper.waitForKeyboardAccessoryToDisappear();

        whenDisplayed(withText(kInfoBarText));
    }
}
