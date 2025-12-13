// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollToPosition;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.base.test.util.CriteriaHelper.pollInstrumentationThread;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.singleMouseClickView;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.ANA;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.BOB;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.NO_ONE;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.createAllPasswordsSheetCredential;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.createBottomSheetController;
import static org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport.waitForState;

import android.text.method.PasswordTransformationMethod;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.concurrent.ExecutionException;

/**
 * View tests for the AllPasswordsBottomSheet ensure that model changes are reflected in the sheet.
 */
@Batch(Batch.UNIT_TESTS)
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.FILLING_PASSWORDS_FROM_ANY_ORIGIN)
public class AllPasswordsBottomSheetViewTest {
    private static final boolean IS_PASSWORD_FIELD = true;
    private static final String EXAMPLE_ORIGIN = "https://m.example.com/";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Mock private Callback<Integer> mDismissHandler;
    @Mock private Callback<String> mSearchQueryCallback;
    @Mock private FaviconHelper mFaviconHelper;
    private BottomSheetController mBottomSheetController;

    private PropertyModel mModel;
    private ListModel<ListItem> mListModel;
    private AllPasswordsBottomSheetView mAllPasswordsBottomSheetView;

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.launchActivity(null);
        runOnUiThreadBlocking(
                () -> {
                    mBottomSheetController =
                            createBottomSheetController(mActivityTestRule.getActivity());
                    mModel =
                            AllPasswordsBottomSheetProperties.createDefaultModel(
                                    EXAMPLE_ORIGIN, mDismissHandler, mSearchQueryCallback);
                    mListModel = new ListModel<>();
                    mAllPasswordsBottomSheetView =
                            new AllPasswordsBottomSheetView(
                                    mActivityTestRule.getActivity(), mBottomSheetController);
                    AllPasswordsBottomSheetCoordinator.setUpView(
                            mModel, mListModel, mAllPasswordsBottomSheetView, mFaviconHelper);
                });
    }

    @Test
    @MediumTest
    public void testVisibilityChangedByModel() {
        // After setting the visibility to true, the view should exist and be visible.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        waitForState(mBottomSheetController, SheetState.FULL);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(true));

        // After hiding the view, the view should still exist but be invisible.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        waitForState(mBottomSheetController, SheetState.HIDDEN);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(false));
    }

    @Test
    @MediumTest
    public void testShowsWarningWithOriginByDefaultWithUpmEnabled() {
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        waitForState(mBottomSheetController, SheetState.FULL);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(true));
        assertEquals(
                mAllPasswordsBottomSheetView.getWarningText().toString(),
                String.format(
                        getString(R.string.all_passwords_bottom_sheet_subtitle), "m.example.com"));
    }

    @Test
    @MediumTest
    public void testCredentialsChangedByModel() {
        addDefaultCredentialsToTheModel();

        waitForState(mBottomSheetController, SheetState.FULL);
        onView(withId(R.id.sheet_item_list))
                .perform(scrollToPosition(0))
                .check(
                        (view, e) -> {
                            View child =
                                    ((RecyclerView) view)
                                            .findViewHolderForAdapterPosition(0)
                                            .itemView;
                            assertThat(getCredentialOrigin(child).getText(), is("example.com"));
                            assertThat(
                                    getCredentialName(child).getPrimaryTextView().getText(),
                                    is(ANA.getFormattedUsername()));
                            assertThat(
                                    getCredentialPassword(child).getPrimaryTextView().getText(),
                                    is(ANA.getPassword()));
                            assertThat(
                                    getCredentialPassword(child)
                                            .getPrimaryTextView()
                                            .getTransformationMethod(),
                                    instanceOf(PasswordTransformationMethod.class));
                            assertThat(getCredentialName(child).isEnabled(), is(true));
                            assertThat(getCredentialName(child).isClickable(), is(true));
                            assertThat(getCredentialPassword(child).isEnabled(), is(true));
                            assertThat(getCredentialPassword(child).isClickable(), is(true));
                        });

        onView(withId(R.id.sheet_item_list))
                .perform(scrollToPosition(1))
                .check(
                        (view, e) -> {
                            View child =
                                    ((RecyclerView) view)
                                            .findViewHolderForAdapterPosition(1)
                                            .itemView;
                            assertThat(getCredentialOrigin(child).getText(), is("m.example.xyz"));
                            assertThat(
                                    getCredentialName(child).getPrimaryTextView().getText(),
                                    is(NO_ONE.getFormattedUsername()));
                            assertThat(
                                    getCredentialPassword(child).getPrimaryTextView().getText(),
                                    is(NO_ONE.getPassword()));
                            assertThat(
                                    getCredentialPassword(child)
                                            .getPrimaryTextView()
                                            .getTransformationMethod(),
                                    instanceOf(PasswordTransformationMethod.class));
                            assertThat(getCredentialName(child).isEnabled(), is(false));
                            assertThat(getCredentialName(child).isClickable(), is(false));
                            assertThat(getCredentialPassword(child).isEnabled(), is(true));
                            assertThat(getCredentialPassword(child).isClickable(), is(true));
                        });

        onView(withId(R.id.sheet_item_list))
                .perform(scrollToPosition(2))
                .check(
                        (view, e) -> {
                            View child =
                                    ((RecyclerView) view)
                                            .findViewHolderForAdapterPosition(2)
                                            .itemView;
                            assertThat(getCredentialOrigin(child).getText(), is("facebook"));
                            assertThat(
                                    getCredentialName(child).getPrimaryTextView().getText(),
                                    is(BOB.getFormattedUsername()));
                            assertThat(
                                    getCredentialPassword(child).getPrimaryTextView().getText(),
                                    is(BOB.getPassword()));
                            assertThat(
                                    getCredentialPassword(child)
                                            .getPrimaryTextView()
                                            .getTransformationMethod(),
                                    instanceOf(PasswordTransformationMethod.class));
                            assertThat(getCredentialName(child).isEnabled(), is(true));
                            assertThat(getCredentialName(child).isClickable(), is(true));
                            assertThat(getCredentialPassword(child).isEnabled(), is(true));
                            assertThat(getCredentialPassword(child).isClickable(), is(true));
                        });
    }

    @Test
    @MediumTest
    public void testFillingPasswordInNonPasswordFieldShowsWarningDialog()
            throws ExecutionException {
        runOnUiThreadBlocking(
                () -> {
                    mAllPasswordsBottomSheetView.setVisible(true);
                    mListModel.add(createAllPasswordsSheetCredential(ANA, !IS_PASSWORD_FIELD));
                });

        waitForState(mBottomSheetController, SheetState.FULL);

        onView(allOf(withId(R.id.password_text), isDescendantOfA(withId(R.id.sheet_item_list))))
                .perform(click());

        pollInstrumentationThread(
                () -> {
                    onView(withText(R.string.passwords_not_secure_filling))
                            .inRoot(isDialog())
                            .check(matches(isDisplayed()));
                });
    }

    @Test
    @MediumTest
    public void testConsumesGenericMotionEventsToPreventMouseClicksThroughSheet() {
        // After setting the visibility to true, the view should exist and be visible.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        waitForState(mBottomSheetController, SheetState.FULL);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(true));

        assertThat(singleMouseClickView(mAllPasswordsBottomSheetView.getContentView()), is(true));
    }

    @Test
    @MediumTest
    public void testDismissesWhenHidden() {
        addDefaultCredentialsToTheModel();

        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        waitForState(mBottomSheetController, SheetState.FULL);
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        waitForState(mBottomSheetController, SheetState.HIDDEN);
        verify(mDismissHandler).onResult(BottomSheetController.StateChangeReason.NONE);
    }

    @Test
    @MediumTest
    public void testSearchIsCalledOnSearchQueryChange() {
        addDefaultCredentialsToTheModel();
        pollUiThread(() -> mAllPasswordsBottomSheetView.getSearchView().setQuery("a", false));
        verify(mSearchQueryCallback).onResult("a");
    }

    // Adds three credential items to the model.
    private void addDefaultCredentialsToTheModel() {
        runOnUiThreadBlocking(
                () -> {
                    mAllPasswordsBottomSheetView.setVisible(true);
                    mListModel.add(createAllPasswordsSheetCredential(ANA, IS_PASSWORD_FIELD));
                    mListModel.add(createAllPasswordsSheetCredential(NO_ONE, IS_PASSWORD_FIELD));
                    mListModel.add(createAllPasswordsSheetCredential(BOB, IS_PASSWORD_FIELD));
                });
    }

    private String getString(@StringRes int stringRes) {
        return mAllPasswordsBottomSheetView.getContentView().getResources().getString(stringRes);
    }

    private TextView getCredentialOrigin(View parent) {
        return parent.findViewById(R.id.password_info_title);
    }

    private ChipView getCredentialName(View parent) {
        return parent.findViewById(R.id.suggestion_text);
    }

    private ChipView getCredentialPassword(View parent) {
        return parent.findViewById(R.id.password_text);
    }
}
