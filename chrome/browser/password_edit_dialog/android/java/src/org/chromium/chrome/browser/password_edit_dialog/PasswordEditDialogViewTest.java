// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;
import android.widget.AutoCompleteTextView;
import android.widget.TextView;

import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.Arrays;

/** View tests for PasswordEditDialogView */
@RunWith(BaseRobolectricTestRunner.class)
public class PasswordEditDialogViewTest {
    private static final String[] USERNAMES = {"user1", "user2", "user3"};
    private static final String INITIAL_USERNAME = "user2";
    private static final String CHANGED_USERNAME = "user21";
    private static final String INITIAL_PASSWORD = "password";
    private static final String CHANGED_PASSWORD = "passwordChanged";
    private static final String FOOTER = "Footer";
    private static final String PASSWORD_ERROR = "Enter password";

    private PasswordEditDialogView mDialogView;
    private AutoCompleteTextView mUsernamesView;
    private TextInputLayout mUsernameInputLayout;
    private TextInputEditText mPasswordView;
    private TextInputLayout mPasswordInputLayout;
    private TextView mFooterView;
    private String mUsername;
    private String mCurrentPassword;

    @Before
    public void setupTest() {
        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mDialogView =
                (PasswordEditDialogView)
                        activity.getLayoutInflater().inflate(R.layout.password_edit_dialog, null);
        mUsernamesView = mDialogView.findViewById(R.id.username_view);
        mUsernameInputLayout = mDialogView.findViewById(R.id.username_input_layout);
        mFooterView = mDialogView.findViewById(R.id.footer);
        activity.setContentView(mDialogView);
        mPasswordView = mDialogView.findViewById(R.id.password);
        mPasswordInputLayout = mDialogView.findViewById(R.id.password_text_input_layout);
    }

    void handleUsernameSelection(String username) {
        mUsername = username;
    }

    void handlePasswordChanged(String password) {
        mCurrentPassword = password;
    }

    PropertyModel.Builder populateDialogPropertiesBuilder() {
        return new PropertyModel.Builder(PasswordEditDialogProperties.ALL_KEYS)
                .with(PasswordEditDialogProperties.USERNAMES, Arrays.asList(USERNAMES))
                .with(PasswordEditDialogProperties.USERNAME, INITIAL_USERNAME)
                .with(PasswordEditDialogProperties.PASSWORD, INITIAL_PASSWORD)
                .with(
                        PasswordEditDialogProperties.USERNAME_CHANGED_CALLBACK,
                        this::handleUsernameSelection)
                .with(
                        PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK,
                        this::handlePasswordChanged);
    }

    /** Tests that all the properties propagated correctly. */
    @Test
    public void testProperties() {
        PropertyModel model =
                populateDialogPropertiesBuilder()
                        .with(PasswordEditDialogProperties.FOOTER, FOOTER)
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, PasswordEditDialogViewBinder::bind);
        assertEquals(
                "Username doesn't match the initial one",
                INITIAL_USERNAME,
                mUsernamesView.getText().toString());
        assertEquals(
                "Password doesn't match", INITIAL_PASSWORD, mPasswordView.getText().toString());
        assertEquals("Footer should be visible", View.VISIBLE, mFooterView.getVisibility());
    }

    /** Tests password changed callback. */
    @Test
    public void testPasswordEditing() {
        PropertyModel model = populateDialogPropertiesBuilder().build();
        PropertyModelChangeProcessor.create(model, mDialogView, PasswordEditDialogViewBinder::bind);
        mPasswordView.setText(INITIAL_PASSWORD);
        assertEquals(INITIAL_PASSWORD, mCurrentPassword);
        mPasswordView.setText(CHANGED_PASSWORD);
        assertEquals(CHANGED_PASSWORD, mCurrentPassword);
    }

    /** Tests that when the footer property is empty footer view is hidden. */
    @Test
    public void testEmptyFooter() {
        // Test with null footer property.
        PropertyModel nullModel = populateDialogPropertiesBuilder().build();
        PropertyModelChangeProcessor.create(
                nullModel, mDialogView, PasswordEditDialogViewBinder::bind);
        assertEquals("Footer should not be visible", View.GONE, mFooterView.getVisibility());

        // Test with footer property containing empty string.
        PropertyModel emptyModel =
                populateDialogPropertiesBuilder()
                        .with(PasswordEditDialogProperties.FOOTER, "")
                        .build();
        PropertyModelChangeProcessor.create(
                emptyModel, mDialogView, PasswordEditDialogViewBinder::bind);
        assertEquals("Footer should not be visible", View.GONE, mFooterView.getVisibility());
    }

    /** Tests username selected callback. */
    @Test
    public void testUsernameSelection() {
        PropertyModel model = populateDialogPropertiesBuilder().build();
        PropertyModelChangeProcessor.create(model, mDialogView, PasswordEditDialogViewBinder::bind);
        mUsernamesView.setText(CHANGED_USERNAME);
        assertEquals(CHANGED_USERNAME, mUsername);
        mUsernamesView.setText(INITIAL_USERNAME);
        assertEquals(INITIAL_USERNAME, mUsername);
    }

    /** Tests if the password error is displayed */
    @Test
    public void testPasswordError() {
        PropertyModel model =
                populateDialogPropertiesBuilder()
                        .with(PasswordEditDialogProperties.PASSWORD_ERROR, PASSWORD_ERROR)
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, PasswordEditDialogViewBinder::bind);
        assertEquals(
                "Should display password error",
                PASSWORD_ERROR,
                mPasswordInputLayout.getError().toString());

        model =
                populateDialogPropertiesBuilder()
                        .with(PasswordEditDialogProperties.PASSWORD_ERROR, null)
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, PasswordEditDialogViewBinder::bind);
        assertNull("Password error should be reset now", mPasswordInputLayout.getError());
    }

    /**
     * Tests that: - the dropdown popup and the button are not displayed when there is only one
     * username in the list and it is the same as the initial username in the text input; - the
     * dropdown and the popup are shown after the text has changed in the input;
     */
    @Test
    public void testShouldShowDropdownWhenUsernamesDifferent() {
        PropertyModel model =
                populateDialogPropertiesBuilder()
                        .with(
                                PasswordEditDialogProperties.USERNAMES,
                                Arrays.asList(INITIAL_USERNAME))
                        .with(PasswordEditDialogProperties.USERNAME, INITIAL_USERNAME)
                        .with(PasswordEditDialogProperties.PASSWORD, INITIAL_PASSWORD)
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, PasswordEditDialogViewBinder::bind);
        assertFalse("Should not display dropdown button", mUsernameInputLayout.isEndIconVisible());

        mUsernamesView.setText(CHANGED_USERNAME);
        assertTrue(
                "Should display dropdown button when username has changed",
                mUsernameInputLayout.isEndIconVisible());
    }

    /**
     * Tests that: - the dropdown popup and the button are displayed when the username in the text
     * input is different from the one in the usernames list; - the dropdown and the popup are
     * hidden when the username is set to the same value as the one in the list;
     */
    @Test
    public void testShouldHideDropdownWhenUsernamesSame() {
        PropertyModel model =
                populateDialogPropertiesBuilder()
                        .with(
                                PasswordEditDialogProperties.USERNAMES,
                                Arrays.asList(INITIAL_USERNAME))
                        .with(PasswordEditDialogProperties.USERNAME, INITIAL_USERNAME)
                        .with(PasswordEditDialogProperties.PASSWORD, INITIAL_PASSWORD)
                        .build();
        PropertyModelChangeProcessor.create(model, mDialogView, PasswordEditDialogViewBinder::bind);
        mUsernamesView.setText(CHANGED_USERNAME);
        assertTrue("Should display dropdown button", mUsernameInputLayout.isEndIconVisible());

        mUsernamesView.setText(INITIAL_USERNAME);
        assertFalse(
                "Should not display dropdown when username is set to initial value",
                mUsernameInputLayout.isEndIconVisible());
    }
}
