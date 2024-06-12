// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.contains;
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.when;

import android.content.res.Resources;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.modaldialog.FakeModalDialogManager;

import java.util.Arrays;
import java.util.Collection;

/** Tests for password update dialog. */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class PasswordEditDialogControllerTest {
    private static final String[] USERNAMES = {"user1", "user2", "user3"};
    private static final int INITIAL_USERNAME_INDEX = 1;
    private static final String INITIAL_USERNAME = USERNAMES[INITIAL_USERNAME_INDEX];
    private static final int CHANGED_USERNAME_INDEX = 2;
    private static final String CHANGED_USERNAME = USERNAMES[CHANGED_USERNAME_INDEX];
    private static final String INITIAL_PASSWORD = "password";
    private static final String CHANGED_PASSWORD = "passwordChanged";
    private static final String ACCOUNT_NAME = "foo@bar.com";

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PasswordEditDialogCoordinator.Delegate mDelegateMock;

    private FakeModalDialogManager mModalDialogManager = new FakeModalDialogManager(0);

    @Mock private PasswordEditDialogView mDialogViewMock;

    private PropertyModel mCustomViewModel;
    private PropertyModel mModalDialogModel;

    private PasswordEditDialogCoordinator mDialogCoordinator;
    private boolean mIsSignedIn;

    @Parameters
    public static Collection<Object> data() {
        return Arrays.asList(new Object[] {/* isSignedIn= */ false, /* isSignedIn= */ true});
    }

    public PasswordEditDialogControllerTest(boolean isSignedIn) {
        mIsSignedIn = isSignedIn;
    }

    /**
     * Tests that properties of update modal dialog and custom view are set correctly based on
     * passed parameters when the details feature is enabled.
     */
    @Test
    public void testUpdatePasswordDialogWithMultipleCredentialsPropertiesFeatureEnabled() {
        createAndShowDialog(USERNAMES);
        Resources r = RuntimeEnvironment.getApplication().getResources();

        Assert.assertEquals(
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE),
                r.getString(R.string.confirm_username_dialog_title));
        assertThat(
                "Usernames don't match",
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAMES),
                contains(USERNAMES));
        Assert.assertEquals(
                "Selected username doesn't match",
                INITIAL_USERNAME,
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME));
        Assert.assertEquals(
                "Password doesn't match",
                INITIAL_PASSWORD,
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD));
        Assert.assertNotNull(
                "Footer is empty", mCustomViewModel.get(PasswordEditDialogProperties.FOOTER));
        Assert.assertNotNull(
                "There should be a title icon",
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE_ICON));
        Assert.assertEquals(
                mModalDialogManager
                        .getShownDialogModel()
                        .get(ModalDialogProperties.POSITIVE_BUTTON_TEXT),
                r.getString(R.string.password_manager_update_button));
        if (mIsSignedIn) {
            assertTrue(
                    "Footer should contain user account name",
                    mCustomViewModel
                            .get(PasswordEditDialogProperties.FOOTER)
                            .contains(ACCOUNT_NAME));
        }
    }

    /**
     * Tests that properties of update modal dialog and custom view are set correctly based on
     * passed parameters when the details feature is enabled.
     */
    @Test
    public void testUpdatePasswordDialogWithSingleCredentialPropertiesFeatureEnabled() {
        createAndShowDialog(new String[] {INITIAL_USERNAME});
        Resources r = RuntimeEnvironment.getApplication().getResources();

        Assert.assertEquals(
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE),
                r.getString(R.string.password_update_dialog_title));
        assertThat(
                "Usernames don't match",
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAMES),
                contains(INITIAL_USERNAME));
        Assert.assertEquals(
                "Selected username doesn't match",
                INITIAL_USERNAME,
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME));
        Assert.assertEquals(
                "Password doesn't match",
                INITIAL_PASSWORD,
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD));
        Assert.assertNotNull(
                "Footer is empty", mCustomViewModel.get(PasswordEditDialogProperties.FOOTER));
        Assert.assertNotNull(
                "There should be a title icon",
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE_ICON));
        Assert.assertEquals(
                mModalDialogManager
                        .getShownDialogModel()
                        .get(ModalDialogProperties.POSITIVE_BUTTON_TEXT),
                r.getString(R.string.password_manager_update_button));
        if (mIsSignedIn) {
            assertTrue(
                    "Footer should contain user account name",
                    mCustomViewModel
                            .get(PasswordEditDialogProperties.FOOTER)
                            .contains(ACCOUNT_NAME));
        }
    }

    /**
     * Tests that properties of save modal dialog and custom view are set correctly based on passed
     * parameters when the details feature is enabled.
     */
    @Test
    public void testSavePasswordDialogPropertiesFeatureEnabled() {
        createAndShowDialog(new String[0]);
        Resources r = RuntimeEnvironment.getApplication().getResources();

        Assert.assertEquals(
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE),
                r.getString(R.string.save_password));
        // Save dialog has only one username in usernames list - the one the user's just entered
        assertThat(
                "Usernames don't match",
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAMES),
                contains(new String[] {INITIAL_USERNAME}));
        Assert.assertEquals(
                "Selected username doesn't match",
                INITIAL_USERNAME,
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME));
        Assert.assertEquals(
                "Password doesn't match",
                INITIAL_PASSWORD,
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD));
        Assert.assertNotNull(
                "Footer is empty", mCustomViewModel.get(PasswordEditDialogProperties.FOOTER));
        Assert.assertNotNull(
                "There should be a title icon",
                mModalDialogManager.getShownDialogModel().get(ModalDialogProperties.TITLE_ICON));
        Assert.assertEquals(
                mModalDialogManager
                        .getShownDialogModel()
                        .get(ModalDialogProperties.POSITIVE_BUTTON_TEXT),
                r.getString(R.string.password_manager_save_button));
        if (mIsSignedIn) {
            assertTrue(
                    "Footer should contain user account name",
                    mCustomViewModel
                            .get(PasswordEditDialogProperties.FOOTER)
                            .contains(ACCOUNT_NAME));
        }
    }

    /** Tests that the username entered in the layout propagates to the model. */
    @Test
    public void testUsernameChanged() {
        createAndShowDialog(USERNAMES);

        Callback<String> usernameChangedCallback =
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK);
        usernameChangedCallback.onResult(CHANGED_USERNAME);
        Assert.assertEquals(
                "Selected username doesn't match",
                CHANGED_USERNAME,
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME));
    }

    /** Tests that the description is changed when the username is changed appropriately. */
    @Test
    public void testDescriptionWhenUsernameChanged() {
        createAndShowDialog(USERNAMES);
        when(mDelegateMock.isUsingAccountStorage(CHANGED_USERNAME)).thenReturn(false);

        Callback<String> usernameChangedCallback =
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK);
        assertEquals(
                mCustomViewModel.get(PasswordEditDialogProperties.FOOTER).contains(ACCOUNT_NAME),
                mIsSignedIn);

        usernameChangedCallback.onResult(CHANGED_USERNAME);
        assertFalse(
                mCustomViewModel.get(PasswordEditDialogProperties.FOOTER).contains(ACCOUNT_NAME));
    }

    /** Tests that correct username and password are propagated to the dialog accepted delegate */
    @Test
    public void testDialogIsAcceptedWithCorrectUsernameAndPassword() {
        createAndShowDialog(USERNAMES);
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);

        assertThat(
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME), is(INITIAL_USERNAME));
        assertThat(
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD), is(INITIAL_PASSWORD));

        mCustomViewModel.set(PasswordEditDialogProperties.USERNAME, CHANGED_USERNAME);
        mCustomViewModel.set(PasswordEditDialogProperties.PASSWORD, CHANGED_PASSWORD);
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.POSITIVE);

        Mockito.verify(mDelegateMock).onDialogAccepted(CHANGED_USERNAME, CHANGED_PASSWORD);
        Mockito.verify(mDelegateMock).onDialogDismissed(true);
    }

    @Test
    public void testEmptyPasswordError() {
        createAndShowDialog(USERNAMES);

        Callback<String> passwordChangedCallback =
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK);
        passwordChangedCallback.onResult("");
        assertTrue(
                "Accept button should be disabled when user enters empty password",
                mModalDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        assertTrue(
                "Error should be displayed when user enters empty password",
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD_ERROR) != null
                        && !mCustomViewModel
                                .get(PasswordEditDialogProperties.PASSWORD_ERROR)
                                .isEmpty());
    }

    /** Tests that changing password in editText gets reflected in the model. */
    @Test
    public void testPasswordChanging() {
        createAndShowDialog(USERNAMES);

        Callback<String> passwordChangedCallback =
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK);
        passwordChangedCallback.onResult(CHANGED_PASSWORD);
        Assert.assertEquals(
                "Password doesn't match to the expected",
                CHANGED_PASSWORD,
                mCustomViewModel.get(PasswordEditDialogProperties.PASSWORD));
    }

    /**
     * Tests that button caption changes from "Update" to "Save" when the user changes the username
     * so that the new one is not known to Password Manager.
     */
    @Test
    public void testChangesFromUpdateToSave() {
        createAndShowDialog(new String[] {INITIAL_USERNAME});
        Resources r = RuntimeEnvironment.getApplication().getResources();

        Assert.assertEquals(
                r.getString(R.string.password_manager_update_button),
                mModalDialogManager
                        .getShownDialogModel()
                        .get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));

        Callback<String> usernameChangedCallback =
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK);
        usernameChangedCallback.onResult(CHANGED_USERNAME);

        Assert.assertEquals(
                r.getString(R.string.password_manager_save_button),
                mModalDialogManager
                        .getShownDialogModel()
                        .get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
    }

    /**
     * Tests that title changes from "Save password?" to "Update password?" when the user changes
     * the username to the one already stored in the Password Manager.
     */
    @Test
    public void testChangesFromSaveToUpdate() {
        createAndShowDialog(new String[] {CHANGED_USERNAME});
        Resources r = RuntimeEnvironment.getApplication().getResources();

        Assert.assertEquals(
                r.getString(R.string.password_manager_save_button),
                mModalDialogManager
                        .getShownDialogModel()
                        .get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));

        Callback<String> usernameChangedCallback =
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK);
        usernameChangedCallback.onResult(CHANGED_USERNAME);

        Assert.assertEquals(
                r.getString(R.string.password_manager_update_button),
                mModalDialogManager
                        .getShownDialogModel()
                        .get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
    }

    /** Tests that the dialog is dismissed when dismiss() is called from native code. */
    @Test
    public void testDialogDismissedFromNative() {
        createAndShowDialog(USERNAMES);

        mDialogCoordinator.dismiss();
        Mockito.verify(mDelegateMock, never()).onDialogAccepted(anyString(), anyString());
        Mockito.verify(mDelegateMock).onDialogDismissed(false);
    }

    /** Tests that the dialog is dismissed when negative button callback is triggered. */
    @Test
    public void testDialogDismissedWithNegativeButton() {
        createAndShowDialog(USERNAMES);

        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);
        Mockito.verify(mDelegateMock, never()).onDialogAccepted(anyString(), anyString());
        Mockito.verify(mDelegateMock).onDialogDismissed(false);
    }

    /** Tests that empty username is not listed in the model's list. */
    @Test
    public void testEmptyUsernameNotListed() {
        mDialogCoordinator =
                new PasswordEditDialogCoordinator(
                        RuntimeEnvironment.getApplication(),
                        mModalDialogManager,
                        mDialogViewMock,
                        mDelegateMock);
        mDialogCoordinator.showPasswordEditDialog(
                new String[] {INITIAL_USERNAME, ""},
                INITIAL_USERNAME,
                INITIAL_PASSWORD,
                ACCOUNT_NAME);

        mCustomViewModel = mDialogCoordinator.getDialogViewModelForTesting();

        assertThat(
                mCustomViewModel.get(PasswordEditDialogProperties.USERNAMES),
                contains(INITIAL_USERNAME));
    }

    /**
     * Helper function that creates {@link PasswordEditDialogCoordinator}, and captures property
     * models for modal dialog and custom dialog view.
     */
    private void createAndShowDialog(String[] savedUserNames) {
        when(mDelegateMock.isUsingAccountStorage(any())).thenReturn(mIsSignedIn);
        mDialogCoordinator =
                new PasswordEditDialogCoordinator(
                        RuntimeEnvironment.getApplication(),
                        mModalDialogManager,
                        mDialogViewMock,
                        mDelegateMock);
        mDialogCoordinator.showPasswordEditDialog(
                savedUserNames,
                INITIAL_USERNAME,
                INITIAL_PASSWORD,
                mIsSignedIn ? ACCOUNT_NAME : null);

        mModalDialogModel = mDialogCoordinator.getDialogModelForTesting();
        mCustomViewModel = mDialogCoordinator.getDialogViewModelForTesting();
    }
}
