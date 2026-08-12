// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighterTestUtils;

/** Tests for searching autofill settings. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({
    ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID,
    ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA
})
@DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
@Batch(Batch.PER_CLASS)
public class AutofillSettingsSearchTest {

    @Rule
    public SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(null); // null sets up the search bar to be displayed.

    @Before
    public void setUp() {
        // Dismiss the sign-in promo by default to not interfere with the search tests.
        signInPromoDismissed(true);
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SigninPromoAccessPointId
                                        .AUTOFILL_AND_PASSWORDS));
    }

    @Test
    @SmallTest
    public void testSearchAutofillAndPasswords() {
        searchSettings("autofill");

        onViewWaiting( // Wait for debounce and Search results to appear.
                        allOf(
                                withId(android.R.id.title),
                                withText(R.string.autofill_and_passwords_settings_title)))
                .perform(click());

        assertAutofillAndPasswordsOpened();
    }

    @Test
    @SmallTest
    public void testSearchPasswordManager() {
        searchSettings("password");

        onViewWaiting( // Wait for debounce and Search results to appear.
                        allOf(
                                withId(android.R.id.title),
                                withText(R.string.password_manager_settings_title)))
                .perform(click());

        assertAutofillAndPasswordsOpened();
        onView(
                        allOf(
                                hasDescendant(withText(R.string.password_manager_settings_title)),
                                isHighlighted()))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchPayment() {
        searchSettings("payment");

        onViewWaiting( // Wait for debounce and Search results to appear.
                        allOf(
                                withId(android.R.id.title),
                                withText(R.string.autofill_payments_title)))
                .perform(click());

        assertAutofillAndPasswordsOpened();
        onView(allOf(hasDescendant(withText(R.string.autofill_payments_title)), isHighlighted()))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchContact() {
        searchSettings("contact");

        onViewWaiting( // Wait for debounce and Search results to appear.
                        allOf(
                                withId(android.R.id.title),
                                withText(R.string.autofill_contact_info_title)))
                .perform(click());

        assertAutofillAndPasswordsOpened();
        onView(
                        allOf(
                                hasDescendant(withText(R.string.autofill_contact_info_title)),
                                isHighlighted()))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchTravel() {
        searchSettings("travel");

        onViewWaiting( // Wait for debounce and Search results to appear.
                        allOf(withId(android.R.id.title), withText(R.string.autofill_travel_title)))
                .perform(click());

        assertAutofillAndPasswordsOpened();
        onView(allOf(hasDescendant(withText(R.string.autofill_travel_title)), isHighlighted()))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchIdentity() {
        searchSettings("identity");

        onViewWaiting( // Wait for debounce and Search results to appear.
                        allOf(
                                withId(android.R.id.title),
                                withText(R.string.autofill_identity_docs_title)))
                .perform(click());

        assertAutofillAndPasswordsOpened();
        onView(
                        allOf(
                                hasDescendant(withText(R.string.autofill_identity_docs_title)),
                                isHighlighted()))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchAutofillSettings() {
        searchSettings("Autofill settings");

        onViewWaiting( // Wait for debounce and Search results to appear.
                        allOf(
                                withId(android.R.id.title),
                                withText(R.string.autofill_settings_title)))
                .perform(click());

        assertAutofillAndPasswordsOpened();
        onView(
                        allOf(
                                hasDescendant(withText(R.string.autofill_settings_title)),
                                isHighlighted()))
                .check(matches(isDisplayed()));
    }

    private void searchSettings(String query) {
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withId(R.id.search_box)).perform(click());
        onView(withId(R.id.search_query)).perform(replaceText(query));
    }

    private void assertAutofillAndPasswordsOpened() {
        onView(
                        allOf(
                                withText(R.string.autofill_and_passwords_settings_title),
                                withParent(withId(R.id.action_bar))))
                .check(matches(isDisplayed()));
    }

    private static Matcher<View> isHighlighted() {
        return new TypeSafeMatcher<View>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("is highlighted by ViewHighlighter");
            }

            @Override
            protected boolean matchesSafely(View view) {
                return ViewHighlighterTestUtils.checkHighlightOn(view);
            }
        };
    }

    private static void signInPromoDismissed(boolean value) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.SIGNIN_PROMO_AUTOFILL_AND_PASSWORDS_DISMISSED, value);
    }
}
