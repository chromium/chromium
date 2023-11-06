// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_edit_dialog;

import android.app.Activity;
import android.view.View;
import android.widget.Spinner;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

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
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.Arrays;

/** View tests for UsernameSelectionConfirmationView. */
@RunWith(BaseJUnit4ClassRunner.class)
@DisableFeatures(ChromeFeatureList.PASSWORD_EDIT_DIALOG_WITH_DETAILS)
@Batch(Batch.PER_CLASS)
public class UsernameSelectionConfirmationViewTest {
    private static final String[] USERNAMES = {"user1", "user2", "user3"};
    private static final int INITIAL_USERNAME_INDEX = 1;
    private static final int SELECTED_USERNAME_INDEX = 2;
    private static final String FOOTER = "Footer";

    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    UsernameSelectionConfirmationView mDialogView;
    Spinner mUsernamesView;
    TextView mFooterView;
    int mUsernameIndex;

    @BeforeClass
    public static void setupSuite() {
        sActivityTestRule.launchActivity(null);
        sActivity =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> sActivityTestRule.getActivity());
    }

    @Before
    public void setupTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialogView =
                            (UsernameSelectionConfirmationView)
                                    sActivity
                                            .getLayoutInflater()
                                            .inflate(R.layout.password_edit_dialog, null);
                    mUsernamesView = (Spinner) mDialogView.findViewById(R.id.usernames_spinner);
                    mFooterView = (TextView) mDialogView.findViewById(R.id.footer);
                    sActivity.setContentView(mDialogView);
                });
    }

    void handleUsernameSelected(int usernameIndex) {
        mUsernameIndex = usernameIndex;
    }

    PropertyModel.Builder populateDialogPropertiesBuilder() {
        return new PropertyModel.Builder(PasswordEditDialogProperties.ALL_KEYS)
                .with(PasswordEditDialogProperties.USERNAMES, Arrays.asList(USERNAMES))
                .with(PasswordEditDialogProperties.USERNAME_INDEX, INITIAL_USERNAME_INDEX)
                .with(
                        PasswordEditDialogProperties.USERNAME_SELECTED_CALLBACK,
                        this::handleUsernameSelected);
    }

    /** Tests that all the properties propagated correctly. */
    @Test
    @MediumTest
    public void testProperties() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model =
                            populateDialogPropertiesBuilder()
                                    .with(PasswordEditDialogProperties.FOOTER, FOOTER)
                                    .build();
                    PropertyModelChangeProcessor.create(
                            model, mDialogView, PasswordEditDialogViewBinder::bind);
                });
        Assert.assertEquals(
                "Initial selected username index doesn't match",
                INITIAL_USERNAME_INDEX,
                mUsernamesView.getSelectedItemPosition());
        Assert.assertEquals(
                "Username text doesn't match",
                USERNAMES[INITIAL_USERNAME_INDEX],
                mUsernamesView.getSelectedItem().toString());
        Assert.assertEquals("Footer should be visible", View.VISIBLE, mFooterView.getVisibility());
    }

    /** Tests that when the footer property is empty footer view is hidden. */
    @Test
    @MediumTest
    public void testEmptyFooter() {
        // Test with null footer property.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel nullModel = populateDialogPropertiesBuilder().build();
                    PropertyModelChangeProcessor.create(
                            nullModel, mDialogView, PasswordEditDialogViewBinder::bind);
                });
        Assert.assertEquals("Footer should not be visible", View.GONE, mFooterView.getVisibility());

        // Test with footer property containing empty string.
        TestThreadUtils.runOnUiThreadBlocking(
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
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PropertyModel model = populateDialogPropertiesBuilder().build();
                    PropertyModelChangeProcessor.create(
                            model, mDialogView, PasswordEditDialogViewBinder::bind);
                    mUsernamesView.setSelection(SELECTED_USERNAME_INDEX);
                });
        CriteriaHelper.pollUiThread(() -> SELECTED_USERNAME_INDEX == mUsernameIndex);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mUsernamesView.setSelection(INITIAL_USERNAME_INDEX);
                });
        CriteriaHelper.pollUiThread(() -> INITIAL_USERNAME_INDEX == mUsernameIndex);
    }
}
