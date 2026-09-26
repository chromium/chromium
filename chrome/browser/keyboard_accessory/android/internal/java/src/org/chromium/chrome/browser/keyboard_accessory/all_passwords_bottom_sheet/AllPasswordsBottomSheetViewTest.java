// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.AutofillTestHelper.singleMouseClickView;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetProperties.VISIBLE;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.ANA;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.BOB;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.NO_ONE;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.createAllPasswordsSheetCredential;
import static org.chromium.chrome.browser.keyboard_accessory.all_passwords_bottom_sheet.AllPasswordsBottomSheetTestHelper.createBottomSheetController;

import android.app.Activity;
import android.app.Dialog;
import android.text.method.PasswordTransformationMethod;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.recyclerview.widget.RecyclerView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.shadows.ShadowDialog;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.browser.autofill.helpers.FaviconHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * View tests for the AllPasswordsBottomSheet ensure that model changes are reflected in the sheet.
 */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.FILLING_PASSWORDS_FROM_ANY_ORIGIN)
public class AllPasswordsBottomSheetViewTest {
    private static final boolean IS_PASSWORD_FIELD = true;
    private static final String EXAMPLE_ORIGIN = "https://m.example.com/";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private final PayloadCallbackHelper<Integer> mDismissHandler = new PayloadCallbackHelper<>();
    private final PayloadCallbackHelper<String> mSearchQueryCallback =
            new PayloadCallbackHelper<>();

    private @Mock FaviconHelper mFaviconHelper;
    private @Mock UrlFormatter.Natives mUrlFormatterJniMock;

    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mBottomSheetSupport;
    private PropertyModel mModel;
    private ListModel<ListItem> mListModel;
    private AllPasswordsBottomSheetView mAllPasswordsBottomSheetView;

    @Before
    public void setUp() {
        UrlFormatterJni.setInstanceForTesting(mUrlFormatterJniMock);
        when(mUrlFormatterJniMock.formatUrlForSecurityDisplay(any(), anyInt()))
                .thenAnswer(inv -> inv.<GURL>getArgument(0).getHost());

        ActivityController<TestActivity> controller = Robolectric.buildActivity(TestActivity.class);
        controller.get().setTheme(R.style.Theme_BrowserUI_DayNight);
        Activity activity = controller.setup().get();
        mBottomSheetController = createBottomSheetController(activity);
        mBottomSheetSupport = new BottomSheetTestSupport(mBottomSheetController);
        mModel =
                AllPasswordsBottomSheetProperties.createDefaultModel(
                        EXAMPLE_ORIGIN,
                        mDismissHandler::notifyCalled,
                        mSearchQueryCallback::notifyCalled);
        mListModel = new ListModel<>();
        mAllPasswordsBottomSheetView =
                new AllPasswordsBottomSheetView(activity, mBottomSheetController);
        AllPasswordsBottomSheetCoordinator.setUpView(
                mModel, mListModel, mAllPasswordsBottomSheetView, mFaviconHelper);
    }

    @After
    public void tearDown() {
        mAllPasswordsBottomSheetView.destroy();
        UrlFormatterJni.setInstanceForTesting(null);
    }

    @Test
    public void testVisibilityChangedByModel() {
        // After setting the visibility to true, the view should exist and be visible.
        mModel.set(VISIBLE, true);
        waitForSheetState(SheetState.FULL);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(true));

        // After hiding the view, the view should still exist but be invisible.
        mModel.set(VISIBLE, false);
        waitForSheetState(SheetState.HIDDEN);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(false));
    }

    @Test
    public void testShowsWarningWithOriginByDefaultWithUpmEnabled() {
        mModel.set(VISIBLE, true);
        waitForSheetState(SheetState.FULL);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(true));
        assertEquals(
                String.format(
                        getString(R.string.all_passwords_bottom_sheet_subtitle), "m.example.com"),
                mAllPasswordsBottomSheetView.getWarningText().toString());
    }

    @Test
    public void testCredentialsChangedByModel() {
        addDefaultCredentialsToTheModel();

        waitForSheetState(SheetState.FULL);
        RecyclerView recyclerView = layoutCredentialsList();

        View child0 = recyclerView.findViewHolderForAdapterPosition(0).itemView;
        assertThat(getCredentialOrigin(child0).getText(), is("example.com"));
        assertThat(
                getCredentialName(child0).getPrimaryTextView().getText(),
                is(ANA.getFormattedUsername()));
        assertThat(
                getCredentialPassword(child0).getPrimaryTextView().getText(),
                is(ANA.getPassword()));
        assertThat(
                getCredentialPassword(child0).getPrimaryTextView().getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
        assertThat(getCredentialName(child0).isEnabled(), is(true));
        assertThat(getCredentialName(child0).isClickable(), is(true));
        assertThat(getCredentialPassword(child0).isEnabled(), is(true));
        assertThat(getCredentialPassword(child0).isClickable(), is(true));

        View child1 = recyclerView.findViewHolderForAdapterPosition(1).itemView;
        assertThat(getCredentialOrigin(child1).getText(), is("m.example.xyz"));
        assertThat(
                getCredentialName(child1).getPrimaryTextView().getText(),
                is(NO_ONE.getFormattedUsername()));
        assertThat(
                getCredentialPassword(child1).getPrimaryTextView().getText(),
                is(NO_ONE.getPassword()));
        assertThat(
                getCredentialPassword(child1).getPrimaryTextView().getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
        assertThat(getCredentialName(child1).isEnabled(), is(false));
        assertThat(getCredentialName(child1).isClickable(), is(false));
        assertThat(getCredentialPassword(child1).isEnabled(), is(true));
        assertThat(getCredentialPassword(child1).isClickable(), is(true));

        View child2 = recyclerView.findViewHolderForAdapterPosition(2).itemView;
        assertThat(getCredentialOrigin(child2).getText(), is("facebook"));
        assertThat(
                getCredentialName(child2).getPrimaryTextView().getText(),
                is(BOB.getFormattedUsername()));
        assertThat(
                getCredentialPassword(child2).getPrimaryTextView().getText(),
                is(BOB.getPassword()));
        assertThat(
                getCredentialPassword(child2).getPrimaryTextView().getTransformationMethod(),
                instanceOf(PasswordTransformationMethod.class));
        assertThat(getCredentialName(child2).isEnabled(), is(true));
        assertThat(getCredentialName(child2).isClickable(), is(true));
        assertThat(getCredentialPassword(child2).isEnabled(), is(true));
        assertThat(getCredentialPassword(child2).isClickable(), is(true));
    }

    @Test
    public void testFillingPasswordInNonPasswordFieldShowsWarningDialog() {
        mAllPasswordsBottomSheetView.setVisible(true);
        mListModel.add(createAllPasswordsSheetCredential(ANA, !IS_PASSWORD_FIELD));

        waitForSheetState(SheetState.FULL);
        RecyclerView recyclerView = layoutCredentialsList();
        View child0 = recyclerView.findViewHolderForAdapterPosition(0).itemView;
        getCredentialPassword(child0).performClick();

        Dialog dialog = ShadowDialog.getLatestDialog();
        assertNotNull(dialog);
        TextView title = dialog.findViewById(R.id.confirmation_dialog_title);
        assertEquals(getString(R.string.passwords_not_secure_filling), title.getText().toString());
        dialog.dismiss();
    }

    @Test
    public void testConsumesGenericMotionEventsToPreventMouseClicksThroughSheet() {
        // After setting the visibility to true, the view should exist and be visible.
        mModel.set(VISIBLE, true);
        waitForSheetState(SheetState.FULL);
        assertThat(mAllPasswordsBottomSheetView.getContentView().isShown(), is(true));

        assertThat(singleMouseClickView(mAllPasswordsBottomSheetView.getContentView()), is(true));
    }

    @Test
    public void testDismissesWhenHidden() {
        addDefaultCredentialsToTheModel();

        mModel.set(VISIBLE, true);
        waitForSheetState(SheetState.FULL);
        mModel.set(VISIBLE, false);
        waitForSheetState(SheetState.HIDDEN);
        assertEquals(
                Integer.valueOf(BottomSheetController.StateChangeReason.NONE),
                mDismissHandler.getOnlyPayloadBlocking());
    }

    @Test
    public void testSearchIsCalledOnSearchQueryChange() {
        addDefaultCredentialsToTheModel();
        mAllPasswordsBottomSheetView.getSearchView().setQuery("a", false);
        assertEquals("a", mSearchQueryCallback.getOnlyPayloadBlocking());
    }

    private void waitForSheetState(@SheetState int state) {
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        mBottomSheetSupport.endAllAnimations();
        assertEquals(state, mBottomSheetController.getSheetState());
    }

    private RecyclerView layoutCredentialsList() {
        RecyclerView recyclerView =
                mAllPasswordsBottomSheetView.getContentView().findViewById(R.id.sheet_item_list);
        recyclerView.measure(
                View.MeasureSpec.makeMeasureSpec(1080, View.MeasureSpec.EXACTLY),
                View.MeasureSpec.makeMeasureSpec(1920, View.MeasureSpec.EXACTLY));
        recyclerView.layout(0, 0, 1080, 1920);
        return recyclerView;
    }

    // Adds three credential items to the model.
    private void addDefaultCredentialsToTheModel() {
        mAllPasswordsBottomSheetView.setVisible(true);
        mListModel.add(createAllPasswordsSheetCredential(ANA, IS_PASSWORD_FIELD));
        mListModel.add(createAllPasswordsSheetCredential(NO_ONE, IS_PASSWORD_FIELD));
        mListModel.add(createAllPasswordsSheetCredential(BOB, IS_PASSWORD_FIELD));
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
