// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import static org.hamcrest.Matchers.contains;
import static org.mockito.ArgumentMatchers.anyInt;
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
import org.chromium.base.FeatureList;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;

/** Tests for password edit dialog. */
@RunWith(ParameterizedRobolectricTestRunner.class)
public class PasswordEditDialogTest {
    private static final long NATIVE_PTR = 1;
    private static final String[] USERNAMES = {"user1", "user2", "user3"};
    private static final int INITIAL_USERNAME_INDEX = 1;
    private static final int SELECTED_USERNAME_INDEX = 2;
    private static final String PASSWORD = "password";
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

    private boolean mIsDetailedViewFlagEnabled;
    private boolean mIsSignedIn;

    @Parameters
    public static Collection<Object[]> data() {
        return Arrays.asList(
                new Object[][] {{/*isExtendedViewFlagEnabled=*/false, /*isSignedIn=*/false},
                        {/*isExtendedViewFlagEnabled=*/false, /*isSignedIn=*/true},
                        {/*isExtendedViewFlagEnabled=*/true, /*isSignedIn=*/false},
                        {/*isExtendedViewFlagEnabled=*/true, /*isSignedIn=*/true}});
    }

    public PasswordEditDialogTest(boolean isDetailedViewFlagEnabled, boolean isSignedIn) {
        mIsDetailedViewFlagEnabled = isDetailedViewFlagEnabled;
        mIsSignedIn = isSignedIn;
    }

    @Before
    public void setUp() {
        FeatureList.setTestFeatures(Collections.singletonMap(
                ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS, mIsDetailedViewFlagEnabled));
        createAndShowDialog(mIsSignedIn);
    }

    /**
     * Tests that properties of modal dialog and custom view are set correctly based on passed
     * parameters.
     */
    @Test
    public void testDialogProperties() {
        Mockito.verify(mModalDialogManagerMock)
                .showDialog(mModalDialogModel, ModalDialogManager.ModalDialogType.TAB);
        Assert.assertThat("Usernames don't match",
                mDialogProperties.get(PasswordEditDialogProperties.USERNAMES), contains(USERNAMES));
        Assert.assertEquals("Selected username doesn't match", INITIAL_USERNAME_INDEX,
                mDialogProperties.get(PasswordEditDialogProperties.SELECTED_USERNAME_INDEX));
        Assert.assertEquals("Password doesn't match", PASSWORD,
                mDialogProperties.get(PasswordEditDialogProperties.PASSWORD));
        // Footer should be displayed only when UPM flag is on
        if (mIsDetailedViewFlagEnabled) {
            Assert.assertNotNull(
                    "Footer is empty", mDialogProperties.get(PasswordEditDialogProperties.FOOTER));
            Assert.assertNotNull("There should be a title icon",
                    mModalDialogModel.get(ModalDialogProperties.TITLE_ICON));
            if (mIsSignedIn) {
                Assert.assertTrue("Footer should contain user account name",
                        mDialogProperties.get(PasswordEditDialogProperties.FOOTER)
                                .contains(ACCOUNT_NAME));
            }
        } else {
            Assert.assertNull("Footer is not empty",
                    mDialogProperties.get(PasswordEditDialogProperties.FOOTER));
            Assert.assertNull("No title icon is expected",
                    mModalDialogModel.get(ModalDialogProperties.TITLE_ICON));
        }
    }

    /** Tests that the username selected in spinner gets reflected in the callback patameter. */
    @Test
    public void testUserSelection() {
        Callback<Integer> usernameSelectedCallback =
                mDialogProperties.get(PasswordEditDialogProperties.USERNAME_SELECTED_CALLBACK);
        usernameSelectedCallback.onResult(SELECTED_USERNAME_INDEX);
        Assert.assertEquals("Selected username doesn't match", SELECTED_USERNAME_INDEX,
                mDialogProperties.get(PasswordEditDialogProperties.SELECTED_USERNAME_INDEX));
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.POSITIVE);
        Mockito.verify(mDelegateMock).onDialogAccepted(SELECTED_USERNAME_INDEX);
        Mockito.verify(mModalDialogManagerMock)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
    }

    /**
     * Tests that the username is not saved and the dialog is dismissed when dismiss() is called
     * from native code.
     */
    @Test
    public void testDialogDismissedFromNative() {
        mDialogCoordinator.dismiss();
        Mockito.verify(mDelegateMock, never()).onDialogAccepted(anyInt());
        Mockito.verify(mModalDialogManagerMock)
                .dismissDialog(mModalDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
    }

    /**
     * Tests that the username is not saved and the dialog is dismissed when the user taps on the
     * negative button.
     */
    @Test
    public void testDialogDismissedWithNegativeButton() {
        ModalDialogProperties.Controller dialogController =
                mModalDialogModel.get(ModalDialogProperties.CONTROLLER);
        dialogController.onClick(mModalDialogModel, ModalDialogProperties.ButtonType.NEGATIVE);
        Mockito.verify(mDelegateMock, never()).onDialogAccepted(anyInt());
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
        mDialogCoordinator.show(USERNAMES, INITIAL_USERNAME_INDEX, PASSWORD, ORIGIN,
                signedIn ? ACCOUNT_NAME : null);
        mModalDialogModel = mDialogCoordinator.getDialogModelForTesting();
        mDialogProperties = mDialogCoordinator.getDialogViewModelForTesting();
    }
}
