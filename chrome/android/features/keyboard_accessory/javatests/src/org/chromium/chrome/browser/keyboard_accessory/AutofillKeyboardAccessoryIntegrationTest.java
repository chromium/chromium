// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItem;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabAtPosition;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.waitToBeHidden;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabTestHelper.isKeyboardAccessoryTabLayout;

import android.app.Activity;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.autofill.mojom.FocusedFieldType;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.Arrays;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/** Integration tests for autofill keyboard accessory. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@EnableFeatures({ChromeFeatureList.PORTALS, ChromeFeatureList.PORTALS_CROSS_ORIGIN})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AutofillKeyboardAccessoryIntegrationTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/autofill/autofill_test_form.html";
    private static final String PORTAL_TEST_PAGE =
            "/chrome/test/data/autofill/portal_wrapper.html?url=autofill_test_form.html";

    private ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    /** Parameter provider for enabling/disabling triggering-related Features. */
    public static class FeatureParamProvider implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value(EnabledFeature.NONE).name("default"),
                    new ParameterSet().value(EnabledFeature.PORTALS).name("enablePortals"));
        }
    }

    /** A WebContentsObserver for watching for web contents swaps. */
    private static class SwapWebContentsObserver extends EmptyTabObserver {
        public CallbackHelper mCallbackHelper;

        public SwapWebContentsObserver() {
            mCallbackHelper = new CallbackHelper();
        }

        @Override
        public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
            mCallbackHelper.notifyCalled();
        }
    }

    @IntDef({EnabledFeature.NONE, EnabledFeature.PORTALS})
    @Retention(RetentionPolicy.SOURCE)
    private @interface EnabledFeature {
        int NONE = 0;
        int PORTALS = 1;
    }

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
        loadTestPage(keyboardDelegate, EnabledFeature.NONE);
    }

    private void loadTestPage(
            ChromeWindow.KeyboardVisibilityDelegateFactory keyboardDelegate,
            @EnabledFeature int enabledFeature)
            throws TimeoutException {
        if (enabledFeature == EnabledFeature.PORTALS) {
            mHelper.loadTestPage(PORTAL_TEST_PAGE, false, false, keyboardDelegate);
            SwapWebContentsObserver observer = new SwapWebContentsObserver();
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mActivityTestRule.getActivity().getActivityTab().addObserver(observer);
                    });
            DOMUtils.clickNode(mHelper.getWebContents(), "ACTIVATE");
            CriteriaHelper.pollUiThread(
                    () -> {
                        return observer.mCallbackHelper.getCallCount() == 1;
                    });
            // After activation, the web contents has changed. Inform |mHelper|.
            mHelper.updateWebContentsDependentState();
        } else {
            mHelper.loadTestPage(TEST_PAGE, false, false, keyboardDelegate);
        }
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
        TestThreadUtils.runOnUiThreadBlocking(
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
     * accessory.
     */
    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(FeatureParamProvider.class)
    public void testSelectSuggestionHidesKeyboardAccessory(@EnabledFeature int enabledFeature)
            throws ExecutionException, TimeoutException {
        loadTestPage(FakeKeyboard::new, enabledFeature);
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST", 1);
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
        mHelper.clickNode("NAME_FIRST", 1, FocusedFieldType.FILLABLE_NON_SEARCH_FIELD);
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
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST", 1);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        whenDisplayed(withId(R.id.bar_items_view))
                .perform(scrollTo(isKeyboardAccessoryTabLayout()))
                .perform(actionOnItem(isKeyboardAccessoryTabLayout(), selectTabAtPosition(0)));

        whenDisplayed(withChild(withId(R.id.keyboard_accessory_sheet_frame)));

        assertTrue(
                TestThreadUtils.runOnUiThreadBlocking(
                        () ->
                                mHelper.getManualFillingCoordinator()
                                        .getHandleBackPressChangedSupplier()
                                        .get()));
        assertTrue(
                TestThreadUtils.runOnUiThreadBlocking(
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
                        scrollTo(isKeyboardAccessoryTabLayout()),
                        actionOnItem(isKeyboardAccessoryTabLayout(), selectTabAtPosition(0)));

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
