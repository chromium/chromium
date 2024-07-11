// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_entry_edit;

import static android.text.InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.hasToString;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.ALL_KEYS;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.DUPLICATE_USERNAME_ERROR;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.EMPTY_PASSWORD_ERROR;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.FEDERATION_ORIGIN;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.PASSWORD_VISIBLE;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.URL_OR_APP;
import static org.chromium.chrome.browser.password_entry_edit.CredentialEditProperties.USERNAME;

import android.widget.EditText;
import android.widget.TextView;

import androidx.test.filters.MediumTest;

import com.google.android.material.textfield.TextInputEditText;
import com.google.android.material.textfield.TextInputLayout;

import org.hamcrest.BaseMatcher;
import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_entry_edit.CredentialEntryFragmentViewBase.ComponentStateDelegate;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageButton;

/** View tests for the credential editing UI displaying a saved credential. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CredentialEditViewTest {
    private static final String TEST_URL = "https://m.a.xyz/signin";
    private static final String TEST_USERNAME = "TestUsername";
    private static final String TEST_PASSWORD = "TestPassword";

    @Mock private ComponentStateDelegate mMockComponentStateDelegate;

    private CredentialEditFragmentView mCredentialEditView;
    private PropertyModel mModel;

    @Rule
    public SettingsActivityTestRule<CredentialEditFragmentView> mTestRule =
            new SettingsActivityTestRule<>(CredentialEditFragmentView.class);

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        CredentialEditUiFactory.setCreationStrategy(
                (fragmentView, helpLauncher) -> {
                    mCredentialEditView = (CredentialEditFragmentView) fragmentView;
                    mCredentialEditView.setComponentStateDelegate(mMockComponentStateDelegate);
                });
        mTestRule.startSettingsActivity();
        runOnUiThreadBlocking(
                () -> {
                    mModel =
                            new PropertyModel.Builder(ALL_KEYS)
                                    .with(URL_OR_APP, TEST_URL)
                                    .with(FEDERATION_ORIGIN, "")
                                    .build();
                    CredentialEditCoordinator.setupModelChangeProcessor(
                            mModel, mCredentialEditView);
                });
    }

    @Test
    @MediumTest
    public void testDisplaysUrlOrAppAndExplanation() {
        TextView urlOrAppView = mCredentialEditView.getView().findViewById(R.id.url_or_app);
        assertThat(urlOrAppView.getText(), hasToString(TEST_URL));

        TextView editInfoView = mCredentialEditView.getView().findViewById(R.id.edit_info);
        assertThat(
                editInfoView.getText(),
                hasToString(mCredentialEditView.getString(R.string.password_edit_hint, TEST_URL)));
    }

    @Test
    @MediumTest
    public void testDisplaysUsername() {
        runOnUiThreadBlocking(() -> mModel.set(USERNAME, TEST_USERNAME));
        EditText usernameView = mCredentialEditView.getView().findViewById(R.id.username);
        assertThat(usernameView.getText(), hasToString(TEST_USERNAME));
    }

    @Test
    @MediumTest
    public void testDisplaysPasswordWhenVisible() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(PASSWORD, TEST_PASSWORD);
                    mModel.set(PASSWORD_VISIBLE, true);
                });
        TextInputEditText passwordView = mCredentialEditView.getView().findViewById(R.id.password);
        assertThat(passwordView.getText(), hasToString(TEST_PASSWORD));

        assertThat(passwordView, isVisiblePasswordInput(true));

        ChromeImageButton hideButton =
                mCredentialEditView.getView().findViewById(R.id.password_visibility_button);
        assertThat(
                hideButton.getContentDescription(),
                equalTo(
                        mCredentialEditView.getString(
                                R.string.password_entry_viewer_hide_stored_password)));
    }

    @Test
    @MediumTest
    public void testContainsHiddenPassword() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(PASSWORD, TEST_PASSWORD);
                    mModel.set(PASSWORD_VISIBLE, false);
                });
        TextInputEditText passwordView = mCredentialEditView.getView().findViewById(R.id.password);
        assertThat(passwordView.getText(), hasToString(TEST_PASSWORD));
        assertThat(passwordView, isVisiblePasswordInput(false));

        ChromeImageButton showButton =
                mCredentialEditView.getView().findViewById(R.id.password_visibility_button);
        assertThat(
                showButton.getContentDescription(),
                equalTo(
                        mCredentialEditView.getString(
                                R.string.password_entry_viewer_show_stored_password)));
    }

    @Test
    @MediumTest
    public void testDisplaysUsernameErrorAndDisablesDoneButton() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(DUPLICATE_USERNAME_ERROR, true);
                });
        TextInputLayout usernameInputLayout =
                mCredentialEditView.getView().findViewById(R.id.username_text_input_layout);
        assertThat(
                usernameInputLayout.getError(),
                equalTo(
                        mCredentialEditView.getString(
                                R.string.password_entry_edit_duplicate_username_error)));

        ButtonCompat doneButton = mCredentialEditView.getView().findViewById(R.id.button_primary);
        assertFalse(doneButton.isEnabled());
        assertFalse(doneButton.isClickable());

        runOnUiThreadBlocking(
                () -> {
                    mModel.set(DUPLICATE_USERNAME_ERROR, false);
                });
        assertNull(usernameInputLayout.getError());
        assertTrue(doneButton.isEnabled());
        assertTrue(doneButton.isClickable());
    }

    @Test
    @MediumTest
    public void testDisplaysPasswordErrorAndDisablesDoneButton() {
        runOnUiThreadBlocking(
                () -> {
                    mModel.set(EMPTY_PASSWORD_ERROR, true);
                });
        TextInputLayout passwordInputLayout =
                mCredentialEditView.getView().findViewById(R.id.password_text_input_layout);
        assertThat(
                passwordInputLayout.getError(),
                equalTo(
                        mCredentialEditView.getString(
                                R.string.password_entry_edit_empty_password_error)));

        ButtonCompat doneButton = mCredentialEditView.getView().findViewById(R.id.button_primary);
        assertFalse(doneButton.isEnabled());
        assertFalse(doneButton.isClickable());

        runOnUiThreadBlocking(
                () -> {
                    mModel.set(EMPTY_PASSWORD_ERROR, false);
                });
        assertNull(passwordInputLayout.getError());
        assertTrue(doneButton.isEnabled());
        assertTrue(doneButton.isClickable());
    }

    /**
     * Matches any {@link EditText} which has the content visibility matching to |shouldBeVisible|.
     *
     * @return The matcher checking the input type.
     */
    private static Matcher<EditText> isVisiblePasswordInput(boolean shouldBeVisible) {
        return new BaseMatcher<EditText>() {
            @Override
            public boolean matches(Object o) {
                EditText editText = (EditText) o;
                return ((editText.getInputType() & TYPE_TEXT_VARIATION_VISIBLE_PASSWORD)
                                == TYPE_TEXT_VARIATION_VISIBLE_PASSWORD)
                        == shouldBeVisible;
            }

            @Override
            public void describeTo(Description description) {
                if (shouldBeVisible) {
                    description.appendText("The content should be visible.");
                } else {
                    description.appendText("The content should not be visible.");
                }
            }
        };
    }
}
