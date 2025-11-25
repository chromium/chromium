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
import android.os.Build;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.autofill.mojom.FocusedFieldType;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
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
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.ref.WeakReference;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.function.Supplier;

/** Integration tests for autofill keyboard accessory. */
// TODO(crbug.com/447076444): Enable Keyboard Accessory revamp flag
// TODO(crbug.com/462636368): Turn on the dynamic positioning flag after blink bug is fixed.
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(ChromeFeatureList.AUTOFILL_ANDROID_KEYBOARD_ACCESSORY_DYNAMIC_POSITIONING)
public class AutofillKeyboardAccessoryIntegrationTest {
    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final String TEST_PAGE = "/chrome/test/data/autofill/autofill_test_form.html";

    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

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

    private WebPageStation startAtTestPage(
            ChromeWindow.KeyboardVisibilityDelegateFactory keyboardDelegate)
            throws TimeoutException {
        WebPageStation page =
                mHelper.startAtTestPage(
                        TEST_PAGE, /* isRtl= */ false, /* waitForNode= */ false, keyboardDelegate);
        ManualFillingTestHelper.createAutofillTestProfiles();
        DOMUtils.waitForNonZeroNodeBounds(mHelper.getWebContents(), "NAME_FIRST");
        return page;
    }

    /** Autofocused fields should not show a keyboard accessory. */
    @Test
    @MediumTest
    public void testAutofocusedFieldDoesNotShowKeyboardAccessory() throws TimeoutException {
        startAtTestPage(FakeKeyboard::new);
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
        startAtTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST", 1);
        mHelper.waitForKeyboardAccessoryToBeShown();
    }

    /** Switching fields should re-scroll the keyboard accessory to the left. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/377939398, crbug.com/453679696")
    public void testSwitchFieldsRescrollsKeyboardAccessory() throws TimeoutException {
        startAtTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("EMAIL_ADDRESS", 8);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        // Scroll to the tab switcher and check that the scroll offset is greater than zero.
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(scrollTo(isAssignableFrom(KeyboardAccessoryButtonGroupView.class)));
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
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testSelectSuggestionHidesKeyboardAccessory()
            throws ExecutionException, TimeoutException {
        startAtTestPage(FakeKeyboard::new);
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
        startAtTestPage(MultiWindowKeyboard::new);
        mHelper.clickNode("NAME_FIRST", 1, FocusedFieldType.FILLABLE_NON_SEARCH_FIELD);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        whenDisplayed(withId(R.id.bar_items_view)).perform(actionOnItemAtPosition(0, click()));
        mHelper.waitForKeyboardAccessoryToDisappear();
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SECURITY_TOUCH_EVENT_FILTERING_ANDROID})
    @DisableIf.Build(
            sdk_is_less_than = Build.VERSION_CODES.S,
            supported_abis_includes = "x86",
            message = "crbug.com/455491374")
    public void testClicksThroughOtherSurfaceAreAreProcessed()
            throws ExecutionException, TimeoutException, InterruptedException {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        startAtTestPage(MultiWindowKeyboard::new);
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
    @DisableIf.Build(
            sdk_is_less_than = Build.VERSION_CODES.S,
            supported_abis_includes = "x86",
            message = "crbug.com/455491374")
    public void testClicksThroughOtherSurfaceAreIgnored()
            throws ExecutionException, TimeoutException, InterruptedException {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        startAtTestPage(MultiWindowKeyboard::new);
        // The metric logs potentially filtered events as well, so it doesn't depend on the feature
        // flag being turned on of off.
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "KeyboardAccessory.TouchEventFiltered", true);
        mHelper.clickNode("NAME_FIRST", 1, FocusedFieldType.FILLABLE_NON_SEARCH_FIELD);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        performOnRecyclerViewNthItem(
                withId(R.id.bar_items_view),
                0,
                createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_OBSCURED, false));
        onView(withId(R.id.keyboard_accessory)).check(matches(isDisplayed()));
        performOnRecyclerViewNthItem(
                withId(R.id.bar_items_view),
                0,
                createClickActionWithFlags(MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED, false));
        onView(withId(R.id.keyboard_accessory)).check(matches(isDisplayed()));

        // Close the accessory by clicking on one of the suggestions.
        onView(withId(R.id.bar_items_view)).perform(actionOnItemAtPosition(0, click()));
        mHelper.waitForKeyboardAccessoryToDisappear();
        histogramExpectation.assertExpected();
    }

    @Test
    @MediumTest
    @DisableIf.Build(
            sdk_equals = Build.VERSION_CODES.UPSIDE_DOWN_CAKE,
            message = "crbug.com/377939398")
    public void testMouseClicksConsumedByAccessoryBar()
            throws ExecutionException, TimeoutException, InterruptedException {
        mHelper.startAtTestPage(/* isRtl= */ false);
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
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ANDROID_DESKTOP_KEYBOARD_ACCESSORY_REVAMP})
    @DisableIf.Build(
            sdk_equals = Build.VERSION_CODES.UPSIDE_DOWN_CAKE,
            message = "crbug.com/377939398")
    public void testPressingBackButtonHidesAccessoryWithAutofillSuggestions()
            throws TimeoutException, ExecutionException {
        startAtTestPage(MultiWindowKeyboard::new);
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
    @DisableFeatures({ChromeFeatureList.AUTOFILL_ANDROID_DESKTOP_KEYBOARD_ACCESSORY_REVAMP})
    public void testSheetHasMinimumSizeWhenTriggeredBySuggestion() throws TimeoutException {
        MultiWindowUtils.getInstance().setIsInMultiWindowModeForTesting(true);
        startAtTestPage(MultiWindowKeyboard::new);
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
