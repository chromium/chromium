// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static android.text.InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.isFocusable;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static junit.framework.Assert.assertTrue;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.CoreMatchers.is;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.password_check.PasswordCheckEditFragmentView.EXTRA_COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckEditFragmentView.EXTRA_NEW_PASSWORD;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.os.Bundle;
import android.view.View;
import android.widget.EditText;
import android.widget.ImageButton;

import androidx.annotation.StringRes;
import androidx.test.filters.MediumTest;

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

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.settings.ReauthenticationManager;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;

/**
 * View tests for the Password Check Edit screen only.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordCheckEditViewTest {
    private static final CompromisedCredential ANA = new CompromisedCredential(
            "https://some-url.com/signin", new GURL("https://some-url.com/"), "Ana", "some-url.com",
            "Ana", "password", "https://some-url.com/.well-known/change-password", "", 1, true,
            false, true, true);
    private static final String PASSWORD_CHECK_RESOLUTION_HISTOGRAM_WITH_AUTO_BUTTON =
            "PasswordManager.AutomaticChange.AcceptanceWithAutoButton";
    private static final String PASSWORD_CHECK_RESOLUTION_HISTOGRAM_WITHOUT_AUTO_BUTTON =
            "PasswordManager.AutomaticChange.AcceptanceWithoutAutoButton";
    private static final String PASSWORD_CHECK_RESOLUTION_HISTOGRAM_FOR_SCRIPTED_SITES =
            "PasswordManager.AutomaticChange.ForSitesWithScripts";
    private static final String PASSWORD_CHECK_USER_ACTION_HISTOGRAM =
            "PasswordManager.BulkCheck.UserActionAndroid";

    private PasswordCheckEditFragmentView mPasswordCheckEditView;

    @Rule
    public SettingsActivityTestRule<PasswordCheckEditFragmentView> mTestRule =
            new SettingsActivityTestRule<>(PasswordCheckEditFragmentView.class);

    @Mock
    private PasswordCheck mPasswordCheck;
    @Mock
    private SettingsLauncher mMockSettingsLauncher;

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);
        setUpUiLaunchedFromSettings();

        pollUiThread(() -> mPasswordCheckEditView != null);
        mPasswordCheckEditView.setCheckProvider(
                () -> PasswordCheckFactory.getOrCreate(mMockSettingsLauncher));
    }

    @Test
    @MediumTest
    public void testLoadsCredential() {
        EditText origin = mPasswordCheckEditView.getView().findViewById(R.id.site_edit);
        assertNotNull(origin);
        assertNotNull(origin.getText());
        assertNotNull(origin.getText().toString());
        assertThat(origin.getText().toString(), equalTo(ANA.getDisplayOrigin()));

        EditText username = mPasswordCheckEditView.getView().findViewById(R.id.username_edit);
        assertNotNull(username);
        assertNotNull(username.getText());
        assertNotNull(username.getText().toString());
        assertThat(username.getText().toString(), equalTo(ANA.getDisplayUsername()));

        EditText password = mPasswordCheckEditView.getView().findViewById(R.id.password_edit);
        assertNotNull(password);
        assertNotNull(password.getText());
        assertNotNull(password.getText().toString());
        assertThat(password.getText().toString(), equalTo(ANA.getPassword()));
        assertTrue((password.getInputType() & TYPE_TEXT_VARIATION_VISIBLE_PASSWORD) != 0);
    }

    @Test
    @MediumTest
    public void testSiteAndUsernameDisabled() {
        onView(withId(R.id.site_edit)).check(matches(allOf(not(isEnabled()), not(isFocusable()))));
        onView(withId(R.id.username_edit))
                .check(matches(allOf(not(isEnabled()), not(isFocusable()))));
    }

    @Test
    @MediumTest
    public void testSavesCredentialAndChangedPasswordInBundle() {
        // Change the password.
        final String newPassword = "NewPassword";
        EditText password = mPasswordCheckEditView.getView().findViewById(R.id.password_edit);
        assertNotNull(password);
        runOnUiThreadBlocking(() -> password.setText(newPassword));

        // Save state (e.g. like happening on destruction).
        Bundle bundle = new Bundle();
        mPasswordCheckEditView.onSaveInstanceState(bundle);

        // Verify the data that reconstructs the page contains all updated information.
        assertTrue(bundle.containsKey(EXTRA_COMPROMISED_CREDENTIAL));
        assertTrue(bundle.containsKey(EXTRA_NEW_PASSWORD));
        assertThat(bundle.getParcelable(EXTRA_COMPROMISED_CREDENTIAL), equalTo(ANA));
        assertThat(bundle.getString(EXTRA_NEW_PASSWORD), equalTo(newPassword));
    }

    @Test
    @MediumTest
    public void testTriggersSendingNewPasswordForCredential() {
        // Change the password.
        final String newPassword = "NewPassword";
        EditText password = mPasswordCheckEditView.getView().findViewById(R.id.password_edit);
        assertNotNull(password);
        runOnUiThreadBlocking(() -> password.setText(newPassword));

        onView(withId(R.id.action_save_edited_password)).perform(click());

        verify(mPasswordCheck).updateCredential(eq(ANA), eq(newPassword));

        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           PASSWORD_CHECK_USER_ACTION_HISTOGRAM,
                           PasswordCheckUserAction.EDITED_PASSWORD),
                is(1));
        assertThat(RecordHistogram.getHistogramTotalCountForTesting(
                           PASSWORD_CHECK_RESOLUTION_HISTOGRAM_WITHOUT_AUTO_BUTTON),
                is(0));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           PASSWORD_CHECK_RESOLUTION_HISTOGRAM_WITH_AUTO_BUTTON,
                           PasswordCheckResolutionAction.EDITED_PASSWORD),
                is(1));
        assertThat(RecordHistogram.getHistogramValueCountForTesting(
                           PASSWORD_CHECK_RESOLUTION_HISTOGRAM_FOR_SCRIPTED_SITES,
                           PasswordCheckResolutionAction.EDITED_PASSWORD),
                is(1));
    }

    @Test
    @MediumTest
    public void testEmptyPasswordDisablesSaveButton() {
        // Delete the password.
        EditText password = mPasswordCheckEditView.getView().findViewById(R.id.password_edit);
        runOnUiThreadBlocking(() -> password.setText(""));

        onView(withId(R.id.action_save_edited_password)).check(matches(not(isEnabled())));
        TextInputLayout passwordLabel =
                mPasswordCheckEditView.getView().findViewById(R.id.password_label);
        assertNotNull(passwordLabel.getError());
        assertThat(passwordLabel.getError().toString(),
                equalTo(getString(R.string.pref_edit_dialog_field_required_validation_message)));
    }

    @Test
    @MediumTest
    public void testMasksPasswordOnEyeIconClick() throws ExecutionException {
        EditText password = mPasswordCheckEditView.getView().findViewById(R.id.password_edit);
        ImageButton unmaskButton = mPasswordCheckEditView.getView().findViewById(
                R.id.password_entry_editor_view_password);
        ImageButton maskButton = mPasswordCheckEditView.getView().findViewById(
                R.id.password_entry_editor_mask_password);
        assertNotNull(password);
        assertNotNull(unmaskButton);
        assertNotNull(maskButton);

        // Masked by default
        assertThat(maskButton.getVisibility(), is(View.GONE));
        assertThat(unmaskButton.getVisibility(), is(View.VISIBLE));
        assertThat(password, isVisiblePasswordInput(false));

        // Clicking the unmask button shows the password.
        runOnUiThreadBlocking(unmaskButton::callOnClick);
        assertThat(maskButton.getVisibility(), is(View.VISIBLE));
        assertThat(unmaskButton.getVisibility(), is(View.GONE));
        assertThat(password, isVisiblePasswordInput(true));

        // Clicking the mask button hides the password again.
        runOnUiThreadBlocking(maskButton::callOnClick);
        assertThat(maskButton.getVisibility(), is(View.GONE));
        assertThat(unmaskButton.getVisibility(), is(View.VISIBLE));
        assertThat(password, isVisiblePasswordInput(false));
    }

    private void setUpUiLaunchedFromSettings() {
        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.BULK);

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putParcelable(EXTRA_COMPROMISED_CREDENTIAL, ANA);
        mTestRule.startSettingsActivity(fragmentArgs);
        mPasswordCheckEditView = mTestRule.getFragment();
    }

    private String getString(@StringRes int stringId) {
        return mPasswordCheckEditView.getContext().getString(stringId);
    }

    /**
     * Matches any {@link EditText} which has the content visibility matching to |shouldBeVisible|.
     * @return The matcher checking the input type.
     */
    private Matcher<EditText> isVisiblePasswordInput(boolean shouldBeVisible) {
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
