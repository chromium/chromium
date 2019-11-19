// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.contrib.RecyclerViewActions.actionOnItem;
import static android.support.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static android.support.test.espresso.matcher.ViewMatchers.withChild;
import static android.support.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.waitToBeHidden;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabTestHelper.isKeyboardAccessoryTabLayout;

import android.app.Activity;
import android.os.Build;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;
import android.view.View;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.autofill.mojom.FocusedFieldType;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.lang.ref.WeakReference;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Integration tests for autofill keyboard accessory.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.LOLLIPOP, message = "crbug.com/958631")
@RetryOnFailure
@EnableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillKeyboardAccessoryIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    /**
     * This FakeKeyboard triggers as a regular keyboard but has no measurable height. This simulates
     * being the upper half in multi-window mode.
     */
    private static class MultiWindowKeyboard extends FakeKeyboard {
        public MultiWindowKeyboard(WeakReference<Activity> activity) {
            super(activity);
        }

        @Override
        protected int getStaticKeyboardHeight() {
            return 0;
        }
    }

    private void loadTestPage(ChromeWindow.KeyboardVisibilityDelegateFactory keyboardDelegate)
            throws TimeoutException {
        mHelper.loadTestPage("/chrome/test/data/autofill/autofill_test_form.html", false, false,
                keyboardDelegate);
        ManualFillingTestHelper.createAutofillTestProfiles();
        DOMUtils.waitForNonZeroNodeBounds(mHelper.getWebContents(), "NAME_FIRST");
    }

    /**
     * Autofocused fields should not show a keyboard accessory.
     */
    @Test
    @MediumTest
    public void testAutofocusedFieldDoesNotShowKeyboardAccessory() throws TimeoutException {
        loadTestPage(FakeKeyboard::new);
        CriteriaHelper.pollUiThread(() -> {
            View accessory = mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory);
            return accessory == null || !accessory.isShown();
        });
    }

    /**
     * Tapping on an input field should show a keyboard and its keyboard accessory.
     */
    @Test
    @MediumTest
    public void testTapInputFieldShowsKeyboardAccessory() throws TimeoutException {
        loadTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST");
        mHelper.waitForKeyboardAccessoryToBeShown();
    }

    /**
     * Switching fields should re-scroll the keyboard accessory to the left.
     */
    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/984489")
    public void testSwitchFieldsRescrollsKeyboardAccessory() throws TimeoutException {
        loadTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("EMAIL_ADDRESS");
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        // Scroll to the second position and check it actually happened.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mHelper.getAccessoryBarView().scrollToPosition(2); });
        CriteriaHelper.pollUiThread(() -> {
            return mHelper.getAccessoryBarView().computeHorizontalScrollOffset() > 0;
        }, "Should keep the manual scroll position.");

        // Clicking any other node should now scroll the items back to the initial position.
        mHelper.clickNodeAndShowKeyboard("NAME_LAST");
        CriteriaHelper.pollUiThread(() -> {
            return mHelper.getAccessoryBarView().computeHorizontalScrollOffset() == 0;
        }, "Should be scrolled back to position 0.");
    }

    /**
     * Selecting a keyboard accessory suggestion should hide the keyboard and its keyboard
     * accessory.
     */
    @Test
    @MediumTest
    public void testSelectSuggestionHidesKeyboardAccessory()
            throws ExecutionException, TimeoutException {
        loadTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST");
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mHelper.getFirstAccessorySuggestion().performClick());
        mHelper.waitForKeyboardAccessoryToDisappear();
    }

    @Test
    @MediumTest
    public void testSuggestionsCloseAccessoryWhenClicked()
            throws ExecutionException, TimeoutException {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        loadTestPage(MultiWindowKeyboard::new);
        mHelper.clickNode("NAME_FIRST", FocusedFieldType.FILLABLE_NON_SEARCH_FIELD);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> mHelper.getFirstAccessorySuggestion().performClick());
        mHelper.waitForKeyboardAccessoryToDisappear();
    }

    @Test
    @SmallTest
    public void testPressingBackButtonHidesAccessoryWithAutofillSuggestions()
            throws TimeoutException, ExecutionException {
        loadTestPage(MultiWindowKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST");
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        whenDisplayed(withId(R.id.bar_items_view))
                .perform(scrollTo(isKeyboardAccessoryTabLayout()))
                .perform(actionOnItem(isKeyboardAccessoryTabLayout(), selectTabAtPosition(0)));

        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet)));

        assertTrue(TestThreadUtils.runOnUiThreadBlocking(
                () -> mHelper.getManualFillingCoordinator().handleBackPress()));

        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));
    }

    @Test
    @MediumTest
    public void testSheetHasMinimumSizeWhenTriggeredBySuggestion() throws TimeoutException {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        loadTestPage(MultiWindowKeyboard::new);
        mHelper.clickNode("NAME_FIRST", FocusedFieldType.FILLABLE_NON_SEARCH_FIELD);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        whenDisplayed(withId(R.id.bar_items_view))
                .perform(scrollTo(isKeyboardAccessoryTabLayout()),
                        actionOnItem(isKeyboardAccessoryTabLayout(), selectTabAtPosition(0)));

        CriteriaHelper.pollUiThread(() -> {
            View sheetView =
                    mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory_sheet);
            return sheetView.isShown() && sheetView.getHeight() > 0;
        });

        // Click the back arrow.
        whenDisplayed(withId(R.id.show_keyboard)).perform(click());
        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet)));

        CriteriaHelper.pollUiThread(() -> {
            View sheetView =
                    mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory_sheet);
            return sheetView.getHeight() == 0 || !sheetView.isShown();
        });
    }
}
