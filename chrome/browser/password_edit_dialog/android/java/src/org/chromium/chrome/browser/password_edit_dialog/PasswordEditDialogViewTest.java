// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.app.Activity;
import android.view.View;
import android.widget.AutoCompleteTextView;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.Arrays;

/** View tests for PasswordEditDialogView */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class PasswordEditDialogViewTest {
    private static final String[] USERNAMES = {"user1", "user2", "user3"};
    private static final String INITIAL_USERNAME = "user2";
    private static final String CHANGED_USERNAME = "user21";
    private static final String INITIAL_PASSWORD = "password";
    private static final String CHANGED_PASSWORD = "passwordChanged";
    private static final String FOOTER = "Footer";
    private static final String PASSWORD_ERROR = "Enter password";

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    private PasswordEditDialogView mDialogView;
    private AutoCompleteTextView mUsernamesView;
    private TextInputLayout mUsernameInputLayout;
    private TextInputEditText mPasswordView;
    private TextInputLayout mPasswordInputLayout;
    private TextView mFooterView;
    private String mUsername;
    private String mCurrentPassword;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        sActivity = ThreadUtils.runOnUiThreadBlocking(() -> sActivityTestRule.getActivity());
    }

    @Before
    public void setupTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialogView =
                            (PasswordEditDialogView)
                                    sActivity
                                            .getLayoutInflater()
                                            .inflate(R.layout.password_edit_dialog, null);
                    mUsernamesView = mDialogView.findViewById(R.id.username_view);
                    mUsernameInputLayout = mDialogView.findViewById(R.id.username_input_layout);
                    mFooterView = mDialogView.findViewById(R.id.footer);
                    sActivity.setContentView(mDialogView);
                    mPasswordView = mDialogView.findViewById(R.id.password);
                    mPasswordInputLayout =
                            mDialogView.findViewById(R.id.password_text_input_layout);
                });
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
    @MediumTest
    public void testProperties() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            populateDialogPropertiesBuilder()
                                    .with(PasswordEditDialogProperties.FOOTER, FOOTER)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, mDialogView, PasswordEditDialogViewBinder::bind);
                });
        Assert.assertEquals(
                "Username doesn't match the initial one",
                INITIAL_USERNAME,
                mUsernamesView.getText().toString());
        Assert.assertEquals(
                "Password doesn't match", INITIAL_PASSWORD, mPasswordView.getText().toString());
        Assert.assertEquals("Footer should be visible", View.VISIBLE, mFooterView.getVisibility());
    }

    /** Tests password changed callback. */
    @Test
    @MediumTest
    public void testPasswordEditing() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model = populateDialogPropertiesBuilder().build();
                    PropertyModelChangeProcessor.create(
                            model, mDialogView, PasswordEditDialogViewBinder::bind);
                    mPasswordView.setText(INITIAL_PASSWORD);
                });
        CriteriaHelper.pollUiThread(() -> mCurrentPassword.equals(INITIAL_PASSWORD));
        ThreadUtils.runOnUiThreadBlocking(() -> mPasswordView.setText(CHANGED_PASSWORD));
        CriteriaHelper.pollUiThread(() -> mCurrentPassword.equals(CHANGED_PASSWORD));
    }

    /** Tests that when the footer property is empty footer view is hidden. */
    @Test
    @MediumTest
    public void testEmptyFooter() {
        // Test with null footer property.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel nullModel = populateDialogPropertiesBuilder().build();
                    PropertyModelChangeProcessor.create(
                            nullModel, mDialogView, PasswordEditDialogViewBinder::bind);
                });
        Assert.assertEquals("Footer should not be visible", View.GONE, mFooterView.getVisibility());

        // Test with footer property containing empty string.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel emptyModel =
                            populateDialogPropertiesBuilder()
                                    .with(PasswordEditDialogProperties.FOOTER, "")
                                    .build();
                    PropertyModelChangeProcessor.create(
                            emptyModel, mDialogView, PasswordEditDialogViewBinder::bind);
                });
        Assert.assertEquals("Footer should not be visible", View.GONE, mFooterView.getVisibility());
    }

    /** Tests username selected callback. */
    @Test
    @MediumTest
    public void testUsernameSelection() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model = populateDialogPropertiesBuilder().build();
                    PropertyModelChangeProcessor.create(
                            model, mDialogView, PasswordEditDialogViewBinder::bind);
                    mUsernamesView.setText(CHANGED_USERNAME);
                });
        CriteriaHelper.pollUiThread(() -> mUsername.equals(CHANGED_USERNAME));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUsernamesView.setText(INITIAL_USERNAME);
                });
        CriteriaHelper.pollUiThread(() -> mUsername.equals(INITIAL_USERNAME));
    }

    /** Tests if the password error is displayed */
    @Test
    @MediumTest
    public void testPasswordError() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            populateDialogPropertiesBuilder()
                                    .with(
                                            PasswordEditDialogProperties.PASSWORD_ERROR,
                                            PASSWORD_ERROR)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, mDialogView, PasswordEditDialogViewBinder::bind);
                });
        Assert.assertEquals(
                "Should display password error",
                mPasswordInputLayout.getError().toString(),
                PASSWORD_ERROR);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            populateDialogPropertiesBuilder()
                                    .with(PasswordEditDialogProperties.PASSWORD_ERROR, null)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, mDialogView, PasswordEditDialogViewBinder::bind);
                });
        Assert.assertTrue(
                "Password error should be reset now", mPasswordInputLayout.getError() == null);
    }

    /**
     * Tests that: - the dropdown popup and the button are not displayed when there is only one
     * username in the list and it is the same as the initial username in the text input; - the
     * dropdown and the popup are shown after the text has changed in the input;
     */
    @Test
    @MediumTest
    public void testShouldShowDropdownWhenUsernamesDifferent() {
        runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            populateDialogPropertiesBuilder()
                                    .with(
                                            PasswordEditDialogProperties.USERNAMES,
                                            Arrays.asList(new String[] {INITIAL_USERNAME}))
                                    .with(PasswordEditDialogProperties.USERNAME, INITIAL_USERNAME)
                                    .with(PasswordEditDialogProperties.PASSWORD, INITIAL_PASSWORD)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, mDialogView, PasswordEditDialogViewBinder::bind);
                });
        assertFalse("Should not display dropdown button", mUsernameInputLayout.isEndIconVisible());

        runOnUiThreadBlocking(() -> mUsernamesView.setText(CHANGED_USERNAME));
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
    @MediumTest
    public void testShouldHideDropdownWhenUsernamesSame() {
        runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            populateDialogPropertiesBuilder()
                                    .with(
                                            PasswordEditDialogProperties.USERNAMES,
                                            Arrays.asList(new String[] {INITIAL_USERNAME}))
                                    .with(PasswordEditDialogProperties.USERNAME, INITIAL_USERNAME)
                                    .with(PasswordEditDialogProperties.PASSWORD, INITIAL_PASSWORD)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, mDialogView, PasswordEditDialogViewBinder::bind);
                    mUsernamesView.setText(CHANGED_USERNAME);
                });
        assertTrue("Should display dropdown button", mUsernameInputLayout.isEndIconVisible());

        runOnUiThreadBlocking(() -> mUsernamesView.setText(INITIAL_USERNAME));
        assertFalse(
                "Should not display dropdown when username is set to initial value",
                mUsernameInputLayout.isEndIconVisible());
    }
}
