// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import android.app.Activity;
import android.view.View;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import com.google.android.material.textfield.TextInputEditText;

import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.Arrays;

/** View tests for PasswordEditDialogView. */
@RunWith(BaseJUnit4ClassRunner.class)
public class PasswordEditDialogViewTest {
    private static final String[] USERNAMES = {"user1", "user2", "user3"};
    private static final int INITIAL_USERNAME_INDEX = 1;
    private static final int SELECTED_USERNAME_INDEX = 2;
    private static final String INITIAL_PASSWORD = "password";
    private static final String CHANGED_PASSWORD = "passwordChanged";
    private static final String FOOTER = "Footer";

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private static Activity sActivity;

    PasswordEditDialogView mDialogView;
    Spinner mUsernamesView;
    TextInputEditText mPasswordView;
    TextView mFooterView;
    int mSelectedUsernameIndex;
    String mCurrentPassword;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        sActivity = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> sActivityTestRule.getActivity());
    }

    @Before
    public void setupTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDialogView = (PasswordEditDialogView) sActivity.getLayoutInflater().inflate(
                    ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
                            ? R.layout.password_edit_dialog_with_details
                            : R.layout.password_edit_dialog,
                    null);
            mUsernamesView = (Spinner) mDialogView.findViewById(R.id.usernames_spinner);
            mFooterView = (TextView) mDialogView.findViewById(R.id.footer);
            sActivity.setContentView(mDialogView);
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)) {
                mPasswordView = (TextInputEditText) mDialogView.findViewById(R.id.password);
            }
        });
    }

    void handleUsernameSelection(int selectedUsernameIndex) {
        mSelectedUsernameIndex = selectedUsernameIndex;
    }

    void handlePasswordChanged(String password) {
        mCurrentPassword = password;
    }

    PropertyModel.Builder populateDialogPropertiesBuilder() {
        return new PropertyModel.Builder(PasswordEditDialogProperties.ALL_KEYS)
                .with(PasswordEditDialogProperties.USERNAMES, Arrays.asList(USERNAMES))
                .with(PasswordEditDialogProperties.SELECTED_USERNAME_INDEX, INITIAL_USERNAME_INDEX)
                .with(PasswordEditDialogProperties.PASSWORD, INITIAL_PASSWORD)
                .with(PasswordEditDialogProperties.USERNAME_SELECTED_CALLBACK,
                        this::handleUsernameSelection)
                .with(PasswordEditDialogProperties.PASSWORD_CHANGED_CALLBACK,
                        this::handlePasswordChanged);
    }

    /** Tests that all the properties propagated correctly. */
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testProperties() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel model = populateDialogPropertiesBuilder()
                                          .with(PasswordEditDialogProperties.FOOTER, FOOTER)
                                          .build();
            PropertyModelChangeProcessor.create(model, mDialogView, PasswordEditDialogView::bind);
        });
        Assert.assertEquals("Initial selected username index doesn't match", INITIAL_USERNAME_INDEX,
                mUsernamesView.getSelectedItemPosition());
        Assert.assertEquals("Username text doesn't match", USERNAMES[INITIAL_USERNAME_INDEX],
                mUsernamesView.getSelectedItem().toString());
        Assert.assertNull(mPasswordView);
        Assert.assertEquals("Footer should be visible", View.VISIBLE, mFooterView.getVisibility());
    }

    /** Tests that when the footer property is empty footer view is hidden. */
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testEmptyFooter() {
        // Test with null footer property.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel nullModel = populateDialogPropertiesBuilder().build();
            PropertyModelChangeProcessor.create(
                    nullModel, mDialogView, PasswordEditDialogView::bind);
        });
        Assert.assertEquals("Footer should not be visible", View.GONE, mFooterView.getVisibility());

        // Test with footer property containing empty string.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel emptyModel = populateDialogPropertiesBuilder()
                                               .with(PasswordEditDialogProperties.FOOTER, "")
                                               .build();
            PropertyModelChangeProcessor.create(
                    emptyModel, mDialogView, PasswordEditDialogView::bind);
        });
        Assert.assertEquals("Footer should not be visible", View.GONE, mFooterView.getVisibility());
    }

    /** Tests username selected callback. */
    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testUsernameSelection() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel model = populateDialogPropertiesBuilder().build();
            PropertyModelChangeProcessor.create(model, mDialogView, PasswordEditDialogView::bind);
            mUsernamesView.setSelection(SELECTED_USERNAME_INDEX);
        });
        CriteriaHelper.pollUiThread(() -> mSelectedUsernameIndex == SELECTED_USERNAME_INDEX);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUsernamesView.setSelection(INITIAL_USERNAME_INDEX); });
        CriteriaHelper.pollUiThread(() -> mSelectedUsernameIndex == INITIAL_USERNAME_INDEX);
    }

    /** Tests that all the properties propagated correctly with detailed view flag enabled. */
    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testPropertiesFeatureEnabled() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel model = populateDialogPropertiesBuilder()
                                          .with(PasswordEditDialogProperties.FOOTER, FOOTER)
                                          .build();
            PropertyModelChangeProcessor.create(model, mDialogView, PasswordEditDialogView::bind);
        });
        Assert.assertEquals("Initial selected username index doesn't match", INITIAL_USERNAME_INDEX,
                mUsernamesView.getSelectedItemPosition());
        Assert.assertEquals("Username text doesn't match", USERNAMES[INITIAL_USERNAME_INDEX],
                mUsernamesView.getSelectedItem().toString());
        Assert.assertEquals(
                "Password doesn't match", INITIAL_PASSWORD, mPasswordView.getText().toString());
        Assert.assertEquals("Footer should be visible", View.VISIBLE, mFooterView.getVisibility());
    }

    /** Tests password changed callback with detailed view flag enabled. */
    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testPasswordEditing() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel model = populateDialogPropertiesBuilder().build();
            PropertyModelChangeProcessor.create(model, mDialogView, PasswordEditDialogView::bind);
            mPasswordView.setText(INITIAL_PASSWORD);
        });
        CriteriaHelper.pollUiThread(() -> mCurrentPassword.equals(INITIAL_PASSWORD));
        TestThreadUtils.runOnUiThreadBlocking(() -> mPasswordView.setText(CHANGED_PASSWORD));
        CriteriaHelper.pollUiThread(() -> mCurrentPassword.equals(CHANGED_PASSWORD));
    }

    /**
     * Tests that when the footer property is empty footer view is hidden with detailed view flag
     * enabled.
     */
    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testEmptyFooterFeatureEnabled() {
        // Test with null footer property.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel nullModel = populateDialogPropertiesBuilder().build();
            PropertyModelChangeProcessor.create(
                    nullModel, mDialogView, PasswordEditDialogView::bind);
        });
        Assert.assertEquals("Footer should not be visible", View.GONE, mFooterView.getVisibility());

        // Test with footer property containing empty string.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel emptyModel = populateDialogPropertiesBuilder()
                                               .with(PasswordEditDialogProperties.FOOTER, "")
                                               .build();
            PropertyModelChangeProcessor.create(
                    emptyModel, mDialogView, PasswordEditDialogView::bind);
        });
        Assert.assertEquals("Footer should not be visible", View.GONE, mFooterView.getVisibility());
    }

    /** Tests username selected callback with detailed view flag enabled. */
    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
    public void testUsernameSelectionFeatureEnabled() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PropertyModel model = populateDialogPropertiesBuilder().build();
            PropertyModelChangeProcessor.create(model, mDialogView, PasswordEditDialogView::bind);
            mUsernamesView.setSelection(SELECTED_USERNAME_INDEX);
        });
        CriteriaHelper.pollUiThread(() -> mSelectedUsernameIndex == SELECTED_USERNAME_INDEX);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mUsernamesView.setSelection(INITIAL_USERNAME_INDEX); });
        CriteriaHelper.pollUiThread(() -> mSelectedUsernameIndex == INITIAL_USERNAME_INDEX);
    }
}
