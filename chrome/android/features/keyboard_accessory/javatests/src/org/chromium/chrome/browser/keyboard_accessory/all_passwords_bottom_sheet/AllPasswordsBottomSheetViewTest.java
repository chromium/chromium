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

import static org.chromium.base.test.util.CriteriaHelper.pollInstrumentationThread;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.autofill.AutofillTestHelper.singleMouseClickView;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.VISIBLE;

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
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.ExecutionException;

/**
 * View tests for the AllPasswordsBottomSheet ensure that model changes are reflected in the sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.FILLING_PASSWORDS_FROM_ANY_ORIGIN)
public class AllPasswordsBottomSheetViewTest {
    private static final Credential ANA =
            new Credential(
                    /* username= */ "ana@gmail.com",
                    /* password= */ "S3cr3t",
                    /* formattedUsername= */ "ana@gmail.com",
                    /* originUrl= */ "https://example.com",
                    /* isAndroidCredential= */ false,
                    /* appDisplayName= */ "",
                    /* isPlusAddressUsername= */ true);
    private static final Credential NO_ONE =
            new Credential(
                    /* username= */ "",
                    /* password= */ "***",
                    /* formattedUsername= */ "No Username",
                    /* originUrl= */ "https://m.example.xyz",
                    /* isAndroidCredential= */ false,
                    /* appDisplayName= */ "",
                    /* isPlusAddressUsername= */ false);
    private static final Credential BOB =
            new Credential(
                    /* username= */ "Bob",
                    /* password= */ "***",
                    /* formattedUsername= */ "Bob",
                    /* originUrl= */ "android://com.facebook.org",
                    /* isAndroidCredential= */ true,
                    /* appDisplayName= */ "facebook",
                    /* isPlusAddressUsername= */ false);
    private static final boolean IS_PASSWORD_FIELD = true;
    private static final String EXAMPLE_ORIGIN = "https://m.example.com/";

    @Mock private Callback<Integer> mDismissHandler;
    @Mock private Callback<CredentialFillRequest> mCredentialFillRequestCallback;
    @Mock private Callback<String> mSearchQueryCallback;

    private PropertyModel mModel;
    private ListModel<ListItem> mListModel;
    private AllPasswordsBottomSheetView mAllPasswordsBottomSheetView;
    private BottomSheetController mBottomSheetController;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mModel =
                            AllPasswordsBottomSheetProperties.createDefaultModel(
                                    EXAMPLE_ORIGIN, mDismissHandler, mSearchQueryCallback);
                    mListModel = new ListModel<>();
                    mBottomSheetController =
                            mActivityTestRule
                                    .getActivity()
                                    .getRootUiCoordinatorForTesting()
                                    .getBottomSheetController();
                    mAllPasswordsBottomSheetView =
                            new AllPasswordsBottomSheetView(getActivity(), mBottomSheetController);
                    AllPasswordsBottomSheetViewBinder.UiConfiguration uiConfiguration =
                            new AllPasswordsBottomSheetViewBinder.UiConfiguration();
                    uiConfiguration.faviconHelper =
                            FaviconHelper.create(
                                    getActivity(), mActivityTestRule.getProfile(false));
                    AllPasswordsBottomSheetCoordinator.setUpView(
                            mModel, mListModel, mAllPasswordsBottomSheetView, uiConfiguration);
                });
    }

    @Test
    @MediumTest
    public void testVisibilityChangedByModel() {
        // After setting the visibility to true, the view should exist and be visible.
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(true));

        // After hiding the view, the view should still exist but be invisible.
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(false));
    }

    @Test
    @MediumTest
    public void testShowsWarningWithOriginByDefaultWithUpmEnabled() {
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);
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

        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAllPasswordsBottomSheetView.setVisible(true);
                    mListModel.add(
                            new ListItem(
                                    AllPasswordsBottomSheetProperties.ItemType.CREDENTIAL,
                                    AllPasswordsBottomSheetProperties.CredentialProperties
                                            .createCredentialModel(
                                                    ANA, mCredentialFillRequestCallback, false)));
                });

        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);

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
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(true));

        assertThat(singleMouseClickView(mAllPasswordsBottomSheetView.getContentView()), is(true));
    }

    @Test
    @MediumTest
    public void testDismissesWhenHidden() {
        addDefaultCredentialsToTheModel();

        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);
        ThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
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
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAllPasswordsBottomSheetView.setVisible(true);
                    mListModel.add(
                            new ListItem(
                                    AllPasswordsBottomSheetProperties.ItemType.CREDENTIAL,
                                    AllPasswordsBottomSheetProperties.CredentialProperties
                                            .createCredentialModel(
                                                    ANA,
                                                    mCredentialFillRequestCallback,
                                                    IS_PASSWORD_FIELD)));
                    mListModel.add(
                            new ListItem(
                                    AllPasswordsBottomSheetProperties.ItemType.CREDENTIAL,
                                    AllPasswordsBottomSheetProperties.CredentialProperties
                                            .createCredentialModel(
                                                    NO_ONE,
                                                    mCredentialFillRequestCallback,
                                                    IS_PASSWORD_FIELD)));
                    mListModel.add(
                            new ListItem(
                                    AllPasswordsBottomSheetProperties.ItemType.CREDENTIAL,
                                    AllPasswordsBottomSheetProperties.CredentialProperties
                                            .createCredentialModel(
                                                    BOB,
                                                    mCredentialFillRequestCallback,
                                                    IS_PASSWORD_FIELD)));
                });
    }

    private ChromeActivity getActivity() {
        return mActivityTestRule.getActivity();
    }

    private String getString(@StringRes int stringRes) {
        return mAllPasswordsBottomSheetView.getContentView().getResources().getString(stringRes);
    }

    private @SheetState int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }

    private RecyclerView getCredentials() {
        return (RecyclerView)
                mAllPasswordsBottomSheetView.getContentView().findViewById(R.id.sheet_item_list);
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
