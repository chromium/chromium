// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.view.KeyEvent;
import android.widget.EditText;

import androidx.fragment.app.Fragment;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.autofill.editors.EditorDialogView;
import org.chromium.chrome.browser.autofill.editors.EditorObserverForTest;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;

import java.util.List;
import java.util.concurrent.TimeoutException;

/** Custom ChromeBrowserTestRule to test Autofill. */
class AutofillTestRule extends ChromeBrowserTestRule
        implements EditorObserverForTest, Callback<Fragment> {
    final CallbackHelper mClickUpdate;
    final CallbackHelper mEditorTextUpdate;
    final CallbackHelper mPreferenceUpdate;
    final CallbackHelper mValidationUpdate;
    final CallbackHelper mConfirmationDialogUpdate;
    final CallbackHelper mFragmentShown;

    private EditorDialogView mEditorDialog;
    private Fragment mLastestShownFragment;

    AutofillTestRule() {
        mClickUpdate = new CallbackHelper();
        mEditorTextUpdate = new CallbackHelper();
        mPreferenceUpdate = new CallbackHelper();
        mValidationUpdate = new CallbackHelper();
        mConfirmationDialogUpdate = new CallbackHelper();
        mFragmentShown = new CallbackHelper();
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        Statement statement = super.apply(base, description);
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                // If the test suit is batched, the observers are reset after every test method by
                // the {@link ResettersForTesting}. Thus the observers must be set in the
                // {@link TestRule#apply()} method.
                AutofillCardBenefitsFragment.setObserverForTest(AutofillTestRule.this);
                AutofillProfilesFragment.setObserverForTest(AutofillTestRule.this);
                AutofillLocalCardEditor.setObserverForTest(AutofillTestRule.this);
                AutofillLocalIbanEditor.setObserverForTest(AutofillTestRule.this);
                FinancialAccountsManagementFragment.setObserverForTest(AutofillTestRule.this);
                statement.evaluate();
            }
        };
    }

    protected void setTextInEditorAndWait(final String[] values) throws TimeoutException {
        int callCount = mEditorTextUpdate.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<EditText> fields = mEditorDialog.getEditableTextFieldsForTest();
                    for (int i = 0; i < values.length; i++) {
                        fields.get(i).setText(values[i]);
                    }
                });
        mEditorTextUpdate.waitForCallback(callCount);
    }

    protected void waitForFragmentToBeShown() throws TimeoutException {
        int callCount = mFragmentShown.getCallCount();
        mFragmentShown.waitForCallback(callCount);
    }

    protected void clickInEditorAndWait(final int resourceId, boolean waitForPreferenceUpdate)
            throws TimeoutException {
        int callCount = mClickUpdate.getCallCount();
        int updateCallCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            int updateCallCountBeforeButtonClick = mPreferenceUpdate.getCallCount();
                            mEditorDialog.findViewById(resourceId).performClick();
                            return updateCallCountBeforeButtonClick;
                        });
        mClickUpdate.waitForCallback(callCount);
        if (waitForPreferenceUpdate) {
            mPreferenceUpdate.waitForCallback(updateCallCount);
        }
    }

    /**
     * @param button see {@link android.content.DialogInterface} for button int constants.
     */
    protected void clickInConfirmationDialogAndWait(
            final int button, boolean waitForPreferenceUpdate) throws TimeoutException {
        if (mEditorDialog.getConfirmationDialogForTest() != null) {
            int callCount = mClickUpdate.getCallCount();
            int updateCallCount =
                    ThreadUtils.runOnUiThreadBlocking(
                            () -> {
                                int updateCallCountBeforeButtonClick =
                                        mPreferenceUpdate.getCallCount();
                                mEditorDialog
                                        .getConfirmationDialogForTest()
                                        .getButton(button)
                                        .performClick();
                                return updateCallCountBeforeButtonClick;
                            });
            mClickUpdate.waitForCallback(callCount);
            if (waitForPreferenceUpdate) {
                mPreferenceUpdate.waitForCallback(updateCallCount);
            }
        }
    }

    protected void clickInEditorAndWaitForValidationError(final int resourceId)
            throws TimeoutException {
        int callCount = mValidationUpdate.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mEditorDialog.findViewById(resourceId).performClick());
        mValidationUpdate.waitForCallback(callCount);
    }

    protected void clickInEditorAndWaitForConfirmationDialog(final int resourceId)
            throws TimeoutException {
        int callCount = mConfirmationDialogUpdate.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> mEditorDialog.findViewById(resourceId).performClick());
        mConfirmationDialogUpdate.waitForCallback(callCount);
    }

    protected void sendKeycodeToTextFieldInEditorAndWait(
            final int keycode, final int textFieldIndex) throws TimeoutException {
        int callCount = mClickUpdate.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    List<EditText> fields = mEditorDialog.getEditableTextFieldsForTest();
                    fields.get(textFieldIndex)
                            .dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, keycode));
                    fields.get(textFieldIndex)
                            .dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, keycode));
                });
        mClickUpdate.waitForCallback(callCount);
    }

    protected void clickOnPreferenceAndWait(ChromeSwitchPreference preference)
            throws TimeoutException {
        int callCount = mFragmentShown.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    preference.performClick();
                });
        mFragmentShown.waitForCallback(callCount);
    }

    protected void setEditorDialogAndWait(EditorDialogView editorDialog) throws TimeoutException {
        int callCount = mClickUpdate.getCallCount();
        mEditorDialog = editorDialog;
        mClickUpdate.waitForCallback(callCount);
    }

    protected Fragment getLastestShownFragment() {
        return mLastestShownFragment;
    }

    @Override
    public void onEditorDismiss() {
        ThreadUtils.assertOnUiThread();
        mPreferenceUpdate.notifyCalled();
    }

    @Override
    public void onEditorTextUpdate() {
        ThreadUtils.assertOnUiThread();
        mEditorTextUpdate.notifyCalled();
    }

    @Override
    public void onEditorReadyToEdit() {
        ThreadUtils.assertOnUiThread();
        mClickUpdate.notifyCalled();
    }

    @Override
    public void onEditorValidationError() {
        ThreadUtils.assertOnUiThread();
        mValidationUpdate.notifyCalled();
    }

    @Override
    public void onEditorConfirmationDialogShown() {
        ThreadUtils.assertOnUiThread();
        mConfirmationDialogUpdate.notifyCalled();
    }

    // Callback<Fragment>
    @Override
    public void onResult(Fragment fragment) {
        ThreadUtils.assertOnUiThread();
        mLastestShownFragment = fragment;
        mFragmentShown.notifyCalled();
    }
}
