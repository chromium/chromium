// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.VISIBLE;

import android.text.method.PasswordTransformationMethod;
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
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChipView;

/**
 * View tests for the AllPasswordsBottomSheet ensure that model changes are reflected in the sheet.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures(ChromeFeatureList.FILLING_PASSWORDS_FROM_ANY_ORIGIN)
public class AllPasswordsBottomSheetViewTest {
    private static final Credential ANA =
            new Credential("Ana", "S3cr3t", "Ana", "https://example.com", false, "");
    private static final Credential NO_ONE =
            new Credential("", "***", "No Username", "https://m.example.xyz", false, "");
    private static final Credential BOB =
            new Credential("Bob", "***", "Bob", "android://com.facebook.org", true, "facebook");
    private static final boolean IS_PASSWORD_FIELD = true;
    private static final String EXAMPLE_ORIGIN = "https://m.example.com/";

    @Mock
    private Callback<Integer> mDismissHandler;
    @Mock
    private Callback<Credential> mCredentialCallback;
    @Mock
    private Callback<String> mSearchQueryCallback;

    private PropertyModel mModel;
    private AllPasswordsBottomSheetView mAllPasswordsBottomSheetView;
    private BottomSheetController mBottomSheetController;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mModel = AllPasswordsBottomSheetProperties.createDefaultModel(
                EXAMPLE_ORIGIN, mDismissHandler, mSearchQueryCallback);
        mBottomSheetController = mActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAllPasswordsBottomSheetView =
                    new AllPasswordsBottomSheetView(getActivity(), mBottomSheetController);
            AllPasswordsBottomSheetCoordinator.setUpModelChangeProcessor(
                    mModel, mAllPasswordsBottomSheetView);
        });
    }

    @Test
    @MediumTest
    public void testVisibilityChangedByModel() {
        // After setting the visibility to true, the view should exist and be visible.
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(true));

        // After hiding the view, the view should still exist but be invisible.
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(false));
    }

    @Test
    @MediumTest
    public void testShowsWarningWithOriginByDefault() {
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(true));
        assertEquals(mAllPasswordsBottomSheetView.getWarningText().toString(),
                String.format(
                        getString(R.string.all_passwords_bottom_sheet_warning_dialog_message_first),
                        "m.example.com"));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1157497")
    public void testCredentialsChangedByModel() {
        addDefaultCredentialsToTheModel();

        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);
        assertThat(getCredentials().getChildCount(), is(3));
        assertThat(getCredentialOriginAt(0).getText(), is("example.com"));
        assertThat(getCredentialNameAt(0).getPrimaryTextView().getText(),
                is(ANA.getFormattedUsername()));
        assertThat(
                getCredentialPasswordAt(0).getPrimaryTextView().getText(), is(ANA.getPassword()));
        assertThat(getCredentialPasswordAt(0).getPrimaryTextView().getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
        assertThat(getCredentialNameAt(0).isEnabled(), is(false));
        assertThat(getCredentialNameAt(0).isClickable(), is(false));
        assertThat(getCredentialPasswordAt(0).isEnabled(), is(true));
        assertThat(getCredentialPasswordAt(0).isClickable(), is(true));

        assertThat(getCredentialOriginAt(1).getText(), is("m.example.xyz"));
        assertThat(getCredentialNameAt(1).getPrimaryTextView().getText(),
                is(NO_ONE.getFormattedUsername()));
        assertThat(getCredentialPasswordAt(1).getPrimaryTextView().getText(),
                is(NO_ONE.getPassword()));
        assertThat(getCredentialPasswordAt(1).getPrimaryTextView().getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
        assertThat(getCredentialNameAt(1).isEnabled(), is(false));
        assertThat(getCredentialNameAt(1).isClickable(), is(false));
        assertThat(getCredentialPasswordAt(1).isEnabled(), is(true));
        assertThat(getCredentialPasswordAt(1).isClickable(), is(true));

        assertThat(getCredentialOriginAt(2).getText(), is("facebook"));
        assertThat(getCredentialNameAt(2).getPrimaryTextView().getText(),
                is(BOB.getFormattedUsername()));
        assertThat(
                getCredentialPasswordAt(2).getPrimaryTextView().getText(), is(BOB.getPassword()));
        assertThat(getCredentialPasswordAt(2).getPrimaryTextView().getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
        assertThat(getCredentialNameAt(2).isEnabled(), is(false));
        assertThat(getCredentialNameAt(2).isClickable(), is(false));
        assertThat(getCredentialPasswordAt(2).isEnabled(), is(true));
        assertThat(getCredentialPasswordAt(2).isClickable(), is(true));
    }

    @Test
    @MediumTest
    public void testDismissesWhenHidden() {
        addDefaultCredentialsToTheModel();

        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        pollUiThread(() -> getBottomSheetState() == SheetState.FULL);
        TestThreadUtils.runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
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

    // Adds three credentials items to the model.
    private void addDefaultCredentialsToTheModel() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mAllPasswordsBottomSheetView.setVisible(true);
            mModel.get(SHEET_ITEMS)
                    .add(new ListItem(AllPasswordsBottomSheetProperties.ItemType.CREDENTIAL,
                            AllPasswordsBottomSheetProperties.CredentialProperties
                                    .createCredentialModel(
                                            ANA, mCredentialCallback, IS_PASSWORD_FIELD)));
            mModel.get(SHEET_ITEMS)
                    .add(new ListItem(AllPasswordsBottomSheetProperties.ItemType.CREDENTIAL,
                            AllPasswordsBottomSheetProperties.CredentialProperties
                                    .createCredentialModel(
                                            NO_ONE, mCredentialCallback, IS_PASSWORD_FIELD)));
            mModel.get(SHEET_ITEMS)
                    .add(new ListItem(AllPasswordsBottomSheetProperties.ItemType.CREDENTIAL,
                            AllPasswordsBottomSheetProperties.CredentialProperties
                                    .createCredentialModel(
                                            BOB, mCredentialCallback, IS_PASSWORD_FIELD)));
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
        return (RecyclerView) mAllPasswordsBottomSheetView.getContentView().findViewById(
                R.id.sheet_item_list);
    }

    private TextView getCredentialOriginAt(int index) {
        return getCredentials().getChildAt(index).findViewById(R.id.password_info_title);
    }

    private ChipView getCredentialNameAt(int index) {
        return ((ChipView) getCredentials().getChildAt(index).findViewById(R.id.suggestion_text));
    }

    private ChipView getCredentialPasswordAt(int index) {
        return ((ChipView) getCredentials().getChildAt(index).findViewById(R.id.password_text));
    }
}
