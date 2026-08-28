// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.settings.SettingsSearchTestUtils.assertNoSearchResultsFound;
import static org.chromium.chrome.browser.settings.SettingsSearchTestUtils.clickSearchResult;
import static org.chromium.chrome.browser.settings.SettingsSearchTestUtils.highlighted;
import static org.chromium.chrome.browser.settings.SettingsSearchTestUtils.typeSearchQuery;

import android.view.View;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.autofill.settings.AutofillAndPasswordsFragment.AutofillSettingsReferrer;
import org.chromium.chrome.browser.autofill.settings.options.AutofillOptionsReferrer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.user_prefs.UserPrefs;

/** Tests for searching autofill settings. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({
    ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID,
    ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA
})
@DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
@Batch(Batch.PER_CLASS)
public class AutofillSettingsSearchTest {

    private final HistogramWatcher mSettingsSearchHistogramWatcher =
            HistogramWatcher.newSingleRecordWatcher(
                    "Autofill.YourSavedInfoSettingsPage.VisitReferrer",
                    AutofillSettingsReferrer.SETTINGS_SEARCH);

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

        clickSearchResult(R.string.autofill_and_passwords_settings_title);

        assertAutofillAndPasswordsOpened();
    }

    @Test
    @SmallTest
    public void testSearchPasswordManager() {
        searchSettings("password");

        clickSearchResult(R.string.password_manager_settings_title);

        assertAutofillAndPasswordsOpened();
        onView(highlighted(R.string.password_manager_settings_title)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchPayment() {
        searchSettings("payment");

        clickSearchResult(R.string.autofill_payments_title);

        assertAutofillAndPasswordsOpened();
        onView(highlighted(R.string.autofill_payments_title)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchSaveAndFillPaymentMethods() {
        searchSettings("payment");

        clickSearchResult(R.string.autofill_enable_credit_cards_toggle_label);

        onView(actionBarTitle(withText(R.string.autofill_payments_title)))
                .check(matches(isDisplayed()));
        onView(highlighted(R.string.autofill_enable_credit_cards_toggle_label))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchDeleteSavedCvcs() throws Exception {
        mSettingsActivityTestRule.startSettingsActivity();

        CreditCard card =
                AutofillTestHelper.createLocalCreditCard(
                        "John Doe", "1234567890123456", "12", "2025");
        card.setCvc("123");
        new AutofillTestHelper().addServerCreditCard(card);

        typeSearchQuery("delete saved security codes");

        clickSearchResult(R.string.autofill_settings_page_bulk_remove_cvc_label);

        onView(highlighted(R.string.autofill_settings_page_bulk_remove_cvc_label))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchDeleteSavedCvcs_noMatchWithoutCards() {
        searchSettings("delete saved security codes");

        assertNoSearchResultsFound();
    }

    @Test
    @SmallTest
    public void testSearchContact() {
        searchSettings("contact");

        clickSearchResult(R.string.autofill_contact_info_title);

        assertAutofillAndPasswordsOpened();
        onView(highlighted(R.string.autofill_contact_info_title)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_BUY_NOW_PAY_LATER)
    public void testSearchBuyNowPayLater() {
        mSettingsActivityTestRule.startSettingsActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.AUTOFILL_HAS_SEEN_BNPL, true);
                });

        typeSearchQuery("show pay later options");

        clickSearchResult(R.string.autofill_bnpl_settings_label);

        onView(highlighted(R.string.autofill_bnpl_settings_label)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_BUY_NOW_PAY_LATER)
    public void testSearchBuyNowPayLater_noMatchWithFlagOff() {
        mSettingsActivityTestRule.startSettingsActivity();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.AUTOFILL_HAS_SEEN_BNPL, true);
                });

        typeSearchQuery("show pay later options");

        assertNoSearchResultsFound();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_ENABLE_BUY_NOW_PAY_LATER)
    public void testSearchBuyNowPayLater_noMatchWithoutPreferenceSet() {
        searchSettings("show pay later options");

        assertNoSearchResultsFound();
    }

    @Test
    @SmallTest
    public void testSearchSaveAndFillAddresses() {
        searchSettings("save and fill address");

        clickSearchResult(R.string.autofill_enable_profiles_toggle_label);

        onView(actionBarTitle(withText(R.string.autofill_contact_info_title)))
                .check(matches(isDisplayed()));
        onView(highlighted(R.string.autofill_enable_profiles_toggle_label))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchTravel() {
        searchSettings("travel");

        clickSearchResult(R.string.autofill_travel_title);

        assertAutofillAndPasswordsOpened();
        onView(highlighted(R.string.autofill_travel_title)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchTravelOptIn() {
        searchSettings("travel");

        clickSearchResult(R.string.autofill_travel_opt_in_toggle_label);

        onView(actionBarTitle(withText(R.string.autofill_travel_title)))
                .check(matches(isDisplayed()));
        onView(highlighted(R.string.autofill_travel_opt_in_toggle_label))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchIdentity() {
        searchSettings("identity");

        clickSearchResult(R.string.autofill_identity_docs_title);

        assertAutofillAndPasswordsOpened();
        onView(highlighted(R.string.autofill_identity_docs_title)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchIdentityDocsOptIn() {
        searchSettings("identity");

        clickSearchResult(R.string.autofill_identity_docs_opt_in_toggle_label);

        onView(actionBarTitle(withText(R.string.autofill_identity_docs_title)))
                .check(matches(isDisplayed()));
        onView(highlighted(R.string.autofill_identity_docs_opt_in_toggle_label))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchAutofillSettings() {
        searchSettings("Autofill settings");

        clickSearchResult(R.string.autofill_settings_title);

        assertAutofillAndPasswordsOpened();
        onView(highlighted(R.string.autofill_settings_title)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WALLET_SHOPPING)
    public void testSearchShopping_signInPromoNotDismissed() {
        signInPromoDismissed(false);
        searchSettings("shopping");

        clickSearchResult(R.string.autofill_shopping_title);

        assertAutofillAndPasswordsOpened();
        onView(highlighted(R.string.autofill_shopping_title)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_WALLET_SHOPPING)
    public void testSearchShoppingOptIn() {
        searchSettings("fill shopping");

        clickSearchResult(R.string.autofill_shopping_opt_in_toggle_label);

        onView(highlighted(R.string.autofill_shopping_opt_in_toggle_label))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchPersonalContextToggle() {
        EntityDataManager entityDataManagerMock = Mockito.mock(EntityDataManager.class);
        when(entityDataManagerMock.isPersonalContextPreferenceVisible()).thenReturn(true);
        when(entityDataManagerMock.isPersonalContextEnabled()).thenReturn(true);
        EntityDataManagerFactory.setInstanceForTesting(entityDataManagerMock);

        searchSettings("find and fill");

        clickSearchResult(
                R.string.personal_context_autofill_settings_switch_title_android);

        onView(highlighted(R.string.personal_context_autofill_settings_switch_title_android))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSearchCardBenefitsToggle() {
        searchSettings("Show available card benefits");

        clickSearchResult(R.string.autofill_settings_page_card_benefits_label);

        onView(highlighted(R.string.autofill_settings_page_card_benefits_label))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testSearchAutofill_autofillAndPasswordsDisabled() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.Settings.AutofillOptionsReferrerAndroid",
                        AutofillOptionsReferrer.SETTINGS_SEARCH);
        searchSettings("autofill");

        clickSearchResult(R.string.autofill_settings_title);

        onView(withText(R.string.settings_autofill_service_provider)).check(matches(isDisplayed()));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA,
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID
    })
    public void testSearchAutofill_autofillAiAndAutofillAndPasswordsDisabled() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.Settings.AutofillOptionsReferrerAndroid",
                        AutofillOptionsReferrer.SETTINGS_SEARCH);
        searchSettings("autofill");

        clickSearchResult(R.string.autofill_options_title);

        onView(withText(R.string.autofill_third_party_filling_default))
                .check(matches(isDisplayed()));
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA)
    public void testSearchAutofill_autofillAiDisabled() {
        searchSettings("autofill");

        clickSearchResult(R.string.autofill_options_title);

        assertAutofillAndPasswordsOpened();
        onView(highlighted(R.string.autofill_options_title)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.AUTOFILL_AI_ONLINE_MODEL_TOGGLE_NEW_TITLE,
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID
    })
    public void testSearchAutofillSettingsChildren_yourSavedInfoSettingsPageEnabled() {
        testSearchAutofillAiSwitch();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.AUTOFILL_AI_ONLINE_MODEL_TOGGLE_NEW_TITLE)
    @DisableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testSearchAutofillSettingsChildren_yourSavedInfoSettingsPageDisabled() {
        testSearchAutofillAiSwitch();
    }

    private void testSearchAutofillAiSwitch() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.Settings.AutofillOptionsReferrerAndroid",
                        AutofillOptionsReferrer.SETTINGS_SEARCH);
        searchSettings("Smarter form understanding");

        clickSearchResult(R.string.settings_autofill_ai_page_title_v2);

        onView(actionBarTitle(withText(R.string.autofill_settings_title)))
                .check(matches(isDisplayed()));
        onView(highlighted(R.string.settings_autofill_ai_page_title_v2))
                .check(matches(isDisplayed()));
        histogramWatcher.assertExpected();
    }

    private void searchSettings(String query) {
        mSettingsActivityTestRule.startSettingsActivity();
        typeSearchQuery(query);
    }

    private void assertAutofillAndPasswordsOpened() {
        onView(actionBarTitle(withText(R.string.autofill_and_passwords_settings_title)))
                .check(matches(isDisplayed()));

        mSettingsSearchHistogramWatcher.assertExpected();
    }

    private static Matcher<View> actionBarTitle(Matcher<View> matcher) {
        return allOf(matcher, withParent(withId(R.id.action_bar)));
    }

    private static void signInPromoDismissed(boolean value) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.SIGNIN_PROMO_AUTOFILL_AND_PASSWORDS_DISMISSED, value);
    }
}
