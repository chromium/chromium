// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import static org.hamcrest.Matchers.contains;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.never;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.Collection;

/** Tests for password edit dialog. */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class PasswordEditDialogTest {
    private static final long NATIVE_PTR = 1;
    private static final String[] USERNAMES = {"user1", "user2", "user3"};
    private static final int INITIAL_USERNAME_INDEX = 1;
    private static final String INITIAL_USERNAME = USERNAMES[INITIAL_USERNAME_INDEX];
    private static final String CHANGED_USERNAME = "user3";
    private static final String INITIAL_PASSWORD = "password";
    private static final String CHANGED_PASSWORD = "passwordChanged";
    private static final String ORIGIN = "example.com";
    private static final String ACCOUNT_NAME = "foo@bar.com";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private PasswordEditDialogCoordinator.Delegate mDelegateMock;

    @Mock
    private ModalDialogManager mModalDialogManagerMock;

    @Mock
    private PasswordEditDialogView mDialogViewMock;

    private PropertyModel mDialogProperties;
    private PropertyModel mModalDialogModel;

    private PasswordEditDialogCoordinator mDialogCoordinator;
    private boolean mIsSignedIn;

    @Parameters
    public static Collection<Object> data() {
        return Arrays.asList(new Object[] {/*isSignedIn=*/false, /*isSignedIn=*/true});
    }

    public PasswordEditDialogTest(boolean isSignedIn) {
        mIsSignedIn = isSignedIn;
    }

    @Before
    public void setUp() {
        createAndShowDialog(mIsSignedIn);
    }

    /**
     * Tests that properties of modal dialog and custom view are set correctly based on passed
     * parameters when the details feature is disabled.
     */
    @Test
    @DisableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testDialogPropertiesFeatureDisabled() {
        Mockito.verify(mModalDialogManagerMock)
                .showDialog(mModalDialogModel, ModalDialogManager.ModalDialogType.TAB);
        Assert.assertThat("Usernames don't match",
                mDialogProperties.get(PasswordEditDialogProperties.USERNAMES), contains(USERNAMES));
        Assert.assertEquals("Selected username doesn't match", INITIAL_USERNAME,
                mDialogProperties.get(PasswordEditDialogProperties.USERNAME));
        Assert.assertEquals("Password doesn't match", INITIAL_PASSWORD,
                mDialogProperties.get(PasswordEditDialogProperties.PASSWORD));
        Assert.assertNull(
                "Footer is not empty", mDialogProperties.get(PasswordEditDialogProperties.FOOTER));
        Assert.assertNull("No title icon is expected",
                mModalDialogModel.get(ModalDialogProperties.TITLE_ICON));
    }

    /** Tests that the username selected in spinner gets reflected in the callback parameter. */
    @Test
    @DisableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testUserSelection() {
        Callback<String> usernameSelectedCallback =
                mDialogProperties.get(PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK);
        usernameSelectedCallback.onResult(CHANGED_USERNAME);
        Assert.assertEquals("Selected username doesn't match", CHANGED_USERNAME,
                mDialogProperties.get(PasswordEditDialogProperties.USERNAME));
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
        Mockito.verify(mDelegateMock).onDialogAccepted(CHANGED_USERNAME, INITIAL_PASSWORD);
        Mockito.verify(mModalDialogManagerMock)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    /**
     * Tests that the username is not saved and the dialog is dismissed when dismiss() is called
     * from native code.
     */
    @Test
    @DisableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testDialogDismissedFromNative() {
        mDialogCoordinator.dismiss();
        Mockito.verify(mDelegateMock, never()).onDialogAccepted(anyString(), anyString());
        Mockito.verify(mModalDialogManagerMock)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    /**
     * Tests that the username is not saved and the dialog is dismissed when the user taps on the
     * negative button.
     */
    @Test
    @DisableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testDialogDismissedWithNegativeButton() {
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);
        Mockito.verify(mDelegateMock, never()).onDialogAccepted(anyString(), anyString());
        Mockito.verify(mModalDialogManagerMock)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    /**
     * Tests that properties of modal dialog and custom view are set correctly based on passed
     * parameters when the details feature is enabled.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testDialogPropertiesFeatureEnabled() {
        Mockito.verify(mModalDialogManagerMock)
                .showDialog(mModalDialogModel, ModalDialogManager.ModalDialogType.TAB);
        Assert.assertThat("Usernames don't match",
                mDialogProperties.get(PasswordEditDialogProperties.USERNAMES), contains(USERNAMES));
        Assert.assertEquals("Selected username doesn't match", INITIAL_USERNAME,
                mDialogProperties.get(PasswordEditDialogProperties.USERNAME));
        Assert.assertEquals("Password doesn't match", INITIAL_PASSWORD,
                mDialogProperties.get(PasswordEditDialogProperties.PASSWORD));
        Assert.assertNotNull(
                "Footer is empty", mDialogProperties.get(PasswordEditDialogProperties.FOOTER));
        Assert.assertNotNull("There should be a title icon",
                mModalDialogModel.get(ModalDialogProperties.TITLE_ICON));
        if (mIsSignedIn) {
            Assert.assertTrue("Footer should contain user account name",
                    mDialogProperties.get(PasswordEditDialogProperties.FOOTER)
                            .contains(ACCOUNT_NAME));
        }
    }

    /** Tests that password changing in editText gets reflected in the callback parameter. */
    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testPasswordChanging() {
        Callback<String> passwordChangedCallback =
                mDialogProperties.get(PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK);
        passwordChangedCallback.onResult(CHANGED_PASSWORD);
        Assert.assertEquals("Password doesn't match to the expected", CHANGED_PASSWORD,
                mDialogProperties.get(PasswordEditDialogProperties.PASSWORD));
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
        Mockito.verify(mDelegateMock).onDialogAccepted(INITIAL_USERNAME, CHANGED_PASSWORD);
        Mockito.verify(mModalDialogManagerMock)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testEmptyPasswordError() {
        Callback<String> passwordChangedCallback =
                mDialogProperties.get(PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK);
        passwordChangedCallback.onResult("");
        Assert.assertTrue("Accept button should be disabled when user enters empty password",
                mModalDialogModel.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
        Assert.assertFalse("Error should be displayed when user enters empty password",
                mDialogProperties.get(PasswordEditDialogProperties.PASSWORD_ERROR) == null
                        || mDialogProperties.get(PasswordEditDialogProperties.PASSWORD_ERROR)
                                   .isEmpty());
    }

    /** Tests that the username selected in spinner gets reflected in the callback parameter. */
    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testUserSelectionFeatureEnabled() {
        Callback<String> usernameSelectedCallback =
                mDialogProperties.get(PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK);
        usernameSelectedCallback.onResult(CHANGED_USERNAME);
        Assert.assertEquals("Selected username doesn't match", CHANGED_USERNAME,
                mDialogProperties.get(PasswordEditDialogProperties.USERNAME));
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
        Mockito.verify(mDelegateMock).onDialogAccepted(CHANGED_USERNAME, INITIAL_PASSWORD);
        Mockito.verify(mModalDialogManagerMock)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    /**
     * Tests that the username is not saved and the dialog is dismissed when dismiss() is called
     * from native code.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testDialogDismissedFromNativeFeatureEnabled() {
        mDialogCoordinator.dismiss();
        Mockito.verify(mDelegateMock, never()).onDialogAccepted(anyString(), anyString());
        Mockito.verify(mModalDialogManagerMock)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    /**
     * Tests that the username is not saved and the dialog is dismissed when the user taps on the
     * negative button.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testDialogDismissedWithNegativeButtonFeatureEnabled() {
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);
        Mockito.verify(mDelegateMock, never()).onDialogAccepted(anyString(), anyString());
        Mockito.verify(mModalDialogManagerMock)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
    }

    /**
     * Helper function that creates PasswordEditDialogCoordinator, calls show and captures property
     * models for modal dialog and custom dialog view.
     *
     * @param signedIn Simulates user's sign-in state.
     */
    private void createAndShowDialog(boolean signedIn) {
        mDialogCoordinator = new PasswordEditDialogCoordinator(RuntimeEnvironment.application,
                mModalDialogManagerMock, mDialogViewMock, mDelegateMock);
        mDialogCoordinator.show(USERNAMES, INITIAL_USERNAME_INDEX, INITIAL_PASSWORD, ORIGIN,
                signedIn ? ACCOUNT_NAME : null);
        mModalDialogModel = mDialogCoordinator.getDialogModelForTesting();
        mDialogProperties = mDialogCoordinator.getDialogViewModelForTesting();
    }
}
