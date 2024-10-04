// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItem;
import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.ViewActionOnDescendant.performOnRecyclerViewNthItem;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.createClickActionWithFlags;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.singleMouseClickView;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.waitToBeHidden;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;

import android.app.Activity;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.autofill.mojom.FocusedFieldType;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.button_group_component.KeyboardAccessoryButtonGroupView;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.ref.WeakReference;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Integration tests for autofill keyboard accessory. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillKeyboardAccessoryIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/autofill/autofill_test_form.html";

    private ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    /**
     * This FakeKeyboard triggers as a regular keyboard but has no measurable height. This simulates
     * being the upper half in multi-window mode.
     */
    private static class MultiWindowKeyboard extends FakeKeyboard {
        public MultiWindowKeyboard(
                WeakReference<Activity> activity,
                Supplier<ManualFillingComponent> manualFillingComponentSupplier) {
            super(activity, manualFillingComponentSupplier);
        }

        @Override
        protected int getStaticKeyboardHeight() {
            return 0;
        }
    }

    private void loadTestPage(ChromeWindow.KeyboardVisibilityDelegateFactory keyboardDelegate)
            throws TimeoutException {
        mHelper.loadTestPage(TEST_PAGE, false, false, keyboardDelegate);
        ManualFillingTestHelper.createAutofillTestProfiles();
        DOMUtils.waitForNonZeroNodeBounds(mHelper.getWebContents(), "NAME_FIRST");
    }

    /** Autofocused fields should not show a keyboard accessory. */
    @Test
    @MediumTest
    public void testAutofocusedFieldDoesNotShowKeyboardAccessory() throws TimeoutException {
        loadTestPage(FakeKeyboard::new);
        CriteriaHelper.pollUiThread(
                () -> {
                    View accessory =
                            mActivityTestRule.getActivity().findViewById(R.id.keyboard_accessory);
                    return accessory == null || !accessory.isShown();
                });
    }

    /** Tapping on an input field should show a keyboard and its keyboard accessory. */
    @Test
    @MediumTest
    public void testTapInputFieldShowsKeyboardAccessory() throws TimeoutException {
        loadTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST", 1);
        mHelper.waitForKeyboardAccessoryToBeShown();
    }

    /** Switching fields should re-scroll the keyboard accessory to the left. */
    @Test
    @MediumTest
    public void testSwitchFieldsRescrollsKeyboardAccessory() throws TimeoutException {
        loadTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("EMAIL_ADDRESS", 8);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        // Scroll to the second position and check it actually happened.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mHelper.getAccessoryBarView().scrollToPosition(2);
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    return mHelper.getAccessoryBarView().computeHorizontalScrollOffset() > 0;
                },
                "Should keep the manual scroll position.");

        // Clicking any other node should now scroll the items back to the initial position.
        mHelper.clickNodeAndShowKeyboard("NAME_LAST", 2);
        CriteriaHelper.pollUiThread(
                () -> {
                    return mHelper.getAccessoryBarView().computeHorizontalScrollOffset() == 0;
                },
                "Should be scrolled back to position 0.");
    }

    /**
     * Selecting a keyboard accessory suggestion should hide the keyboard and its keyboard
     * accessory. TODO(336780543): Remove restriction once the test is not failing on the old phone
     * bots.
     */
    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET)
    public void testSelectSuggestionHidesKeyboardAccessory()
            throws ExecutionException, TimeoutException {
        loadTestPage(FakeKeyboard::new);
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "KeyboardAccessory.TouchEventFiltered", false);
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST", 1);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        whenDisplayed(withId(R.id.bar_items_view)).perform(actionOnItemAtPosition(0, click()));
        mHelper.waitForKeyboardAccessoryToDisappear();
        histogramExpectation.assertExpected();
    }

    @Test
    @MediumTest
    public void testSuggestionsCloseAccessoryWhenClicked()
            throws ExecutionException, TimeoutException {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        loadTestPage(MultiWindowKeyboard::new);
        mHelper.clickNode("NAME_FIRST", 1, FocusedFieldType.FILLABLE_NON_SEARCH_FIELD);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        whenDisplayed(withId(R.id.bar_items_view)).perform(actionOnItemAtPosition(0, click()));
        mHelper.waitForKeyboardAccessoryToDisappear();
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
    public void testClicksThroughOtherSurfaceAreAreProcessed()
            throws ExecutionException, TimeoutException, InterruptedException {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        loadTestPage(MultiWindowKeyboard::new);
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "KeyboardAccessory.TouchEventFiltered", true);
        mHelper.clickNode("NAME_FIRST", 1, FocusedFieldType.FILLABLE_NON_SEARCH_FIELD);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        assertTrue(mHelper.getAccessoryBarView().getAdapter().getItemCount() > 0);
        performOnRecyclerViewNthItem(
                withId(R.id.bar_items_view),
                0,
                createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
        mHelper.waitForKeyboardAccessoryToDisappear();
        histogramExpectation.assertExpected();
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
    public void testClicksThroughOtherSurfaceAreIgnored()
            throws ExecutionException, TimeoutException, InterruptedException {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        loadTestPage(MultiWindowKeyboard::new);
        // The metric logs potentially filtered events as well, so it doesn't depend on the feature
        // flag being turned on of off.
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "KeyboardAccessory.TouchEventFiltered", true);
        mHelper.clickNode("NAME_FIRST", 1, FocusedFieldType.FILLABLE_NON_SEARCH_FIELD);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        for (int i = 0; i < mHelper.getAccessoryBarView().getAdapter().getItemCount(); i++) {
            performOnRecyclerViewNthItem(
                    withId(R.id.bar_items_view),
                    i,
                    createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED));
            onView(withId(R.id.keyboard_accessory)).check(matches(isDisplayed()));
            performOnRecyclerViewNthItem(
                    withId(R.id.bar_items_view),
                    i,
                    createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED));
            onView(withId(R.id.keyboard_accessory)).check(matches(isDisplayed()));
        }

        // Close the accessory by clicking on one of the suggestions.
        onView(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)).perform(click());
        mHelper.waitForKeyboardAccessoryToDisappear();
        histogramExpectation.assertExpected();
    }

    @Test
    @MediumTest
    public void testMouseClicksConsumedByAccessoryBar()
            throws ExecutionException, TimeoutException, InterruptedException {
        mHelper.loadTestPage(false);
        mHelper.registerSheetDataProvider(AccessoryTabType.CREDIT_CARDS);
        // Register a sheet data provider so that sheet is available when needed.

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        whenDisplayed(isAssignableFrom(KeyboardAccessoryButtonGroupView.class))
                .check((v, e) -> assertTrue("Didn't catch the click!", singleMouseClickView(v)));
    }

    @Test
    @SmallTest
    public void testPressingBackButtonHidesAccessoryWithAutofillSuggestions()
            throws TimeoutException, ExecutionException {
        loadTestPage(MultiWindowKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST", 1);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        whenDisplayed(withId(R.id.bar_items_view))
                .perform(scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)))
                .perform(
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabAtPosition(0)));

        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)));

        whenDisplayed(withId(R.id.keyboard_accessory_sheet_frame))
                .check((v, e) -> assertTrue("Catch click to stay open!", singleMouseClickView(v)));

        assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mHelper.getManualFillingCoordinator()
                                        .getHandleBackPressChangedSupplier()
                                        .get()));
        assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mHelper.getManualFillingCoordinator().onBackPressed()));

        waitToBeHidden(withChild(withId(R.id.keyboard_accessory_sheet_frame)));
    }

    @Test
    @MediumTest
    public void testSheetHasMinimumSizeWhenTriggeredBySuggestion() throws TimeoutException {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        loadTestPage(MultiWindowKeyboard::new);
        mHelper.clickNode("NAME_FIRST", 1, FocusedFieldType.FILLABLE_NON_SEARCH_FIELD);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        whenDisplayed(withId(R.id.bar_items_view))
                .perform(
                        scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)),
                        actionOnItem(
                                isAssignableFrom(KeyboardAccessoryButtonGroupView.class),
                                selectTabAtPosition(0)));

        whenDisplayed(withId(R.id.keyboard_accessory_sheet_frame))
                .check(
                        (sheetView, exception) -> {
                            assertTrue(sheetView.isShown() && sheetView.getHeight() > 0);
                        });

        // Click the back arrow.
        whenDisplayed(withId(R.id.show_keyboard)).perform(click());
        waitToBeHidden(withId(R.id.keyboard_accessory_sheet_container));

        CriteriaHelper.pollUiThread(
                () -> {
                    View sheetView =
                            mActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.keyboard_accessory_sheet_frame);
                    return sheetView.getHeight() == 0 || !sheetView.isShown();
                });
    }
}
