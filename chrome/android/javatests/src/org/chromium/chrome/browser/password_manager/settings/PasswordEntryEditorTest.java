// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager.settings;

import static android.text.InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.typeText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.password_manager.settings.PasswordSettingsTest.withSaveMenuIdOrText;

import android.os.Bundle;
import android.view.View;
import android.widget.EditText;

import androidx.test.espresso.matcher.BoundedMatcher;
import androidx.test.filters.SmallTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * View tests for the password entry editor screen.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordEntryEditorTest {
    private static final String URL = "https://example.com";
    private static final String USERNAME = "test user";
    private static final String PASSWORD = "passw0rd";

    @Mock
    private PasswordEditingDelegate mMockPasswordEditingDelegate;

    private PasswordEntryEditor mPasswordEntryEditor;

    @Rule
    public SettingsActivityTestRule<PasswordEntryEditor> mTestRule =
            new SettingsActivityTestRule<>(PasswordEntryEditor.class);

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        PasswordEditingDelegateProvider.getInstance().setPasswordEditingDelegate(
                mMockPasswordEditingDelegate);

        launchEditor();
        pollUiThread(() -> mPasswordEntryEditor != null);
    }
    /**
     * Check that the password editing activity displays the data received through arguments.
     */
    @Test
    @SmallTest
    public void testPasswordDataDisplayedInEditingActivity() {
        PasswordEditingDelegateProvider.getInstance().setPasswordEditingDelegate(
                mMockPasswordEditingDelegate);

        onView(withId(R.id.site_edit)).check(matches(withText(URL)));
        onView(withId(R.id.username_edit)).check(matches(withText(USERNAME)));
        onView(withId(R.id.password_edit)).check(matches(withText(PASSWORD)));
    }

    /**
     * Check that the password editing method from the PasswordEditingDelegate was called when the
     * save button in the password editing activity was clicked.
     */
    @Test
    @SmallTest
    public void testPasswordEditingMethodWasCalled() throws Exception {
        PasswordEditingDelegateProvider.getInstance().setPasswordEditingDelegate(
                mMockPasswordEditingDelegate);
        onView(withId(R.id.username_edit)).perform(typeText(" new"));

        onView(withSaveMenuIdOrText()).perform(click());

        verify(mMockPasswordEditingDelegate).editSavedPasswordEntry(USERNAME + " new", PASSWORD);
    }

    /**
     * Check that the stored password is visible after clicking the unmasking icon and invisible
     * after another click.
     */
    @Test
    @SmallTest
    public void testStoredPasswordCanBeUnmaskedAndMaskedAgain() {
        ReauthenticationManager.setApiOverride(ReauthenticationManager.OverrideState.AVAILABLE);
        ReauthenticationManager.setScreenLockSetUpOverride(
                ReauthenticationManager.OverrideState.AVAILABLE);

        ReauthenticationManager.recordLastReauth(
                System.currentTimeMillis(), ReauthenticationManager.ReauthScope.ONE_AT_A_TIME);

        // Masked by default
        onView(withId(R.id.password_edit)).check(matches(isVisiblePasswordInput(false)));

        // Clicking the unmask button shows the password.
        onView(withId(R.id.password_entry_editor_view_password)).perform(click());
        onView(withId(R.id.password_edit)).check(matches(isVisiblePasswordInput(true)));

        // Clicking the mask button hides the password again.
        onView(withId(R.id.password_entry_editor_view_password)).perform(click());
        onView(withId(R.id.password_edit)).check(matches(isVisiblePasswordInput(false)));
    }

    private void launchEditor() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_URL, URL);
        fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_NAME, USERNAME);
        fragmentArgs.putString(PasswordEntryEditor.CREDENTIAL_PASSWORD, PASSWORD);
        mTestRule.startSettingsActivity(fragmentArgs);
        mPasswordEntryEditor = mTestRule.getFragment();
    }

    /**
     * Matches any {@link EditText} which has the content visibility matching to |shouldBeVisible|.
     * @return The matcher checking the input type.
     */
    private static Matcher<View> isVisiblePasswordInput(final boolean shouldBeVisible) {
        return new BoundedMatcher<View, EditText>(EditText.class) {
            @Override
            public boolean matchesSafely(EditText editText) {
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
