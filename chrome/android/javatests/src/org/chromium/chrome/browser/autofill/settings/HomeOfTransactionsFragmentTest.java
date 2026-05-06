// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

import androidx.preference.Preference;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncherFactory;
import org.chromium.chrome.browser.password_manager.FakeCredentialManagerLauncherFactoryImpl;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.policy.test.annotations.Policies;

/** Tests for {@link HomeOfTransactionsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class HomeOfTransactionsFragmentTest {
    @Rule
    public SettingsActivityTestRule<HomeOfTransactionsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(HomeOfTransactionsFragment.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsIndexData mSearchIndexDataMock;
    @Mock private Profile mProfileMock;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;
    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;

    private final FakeCredentialManagerLauncherFactoryImpl mFakeLauncherFactory =
            new FakeCredentialManagerLauncherFactoryImpl();
    private final PayloadCallbackHelper<PendingIntent> mSuccessCallbackHelper =
            new PayloadCallbackHelper<>();

    @Before
    public void setUp() {
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJniMock);
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(anyBoolean()))
                .thenReturn(true);

        CredentialManagerLauncherFactory.setFactoryForTesting(mFakeLauncherFactory);
        mFakeLauncherFactory.setSuccessCallback(mSuccessCallbackHelper::notifyCalled);
        Context context = ApplicationProvider.getApplicationContext();
        mFakeLauncherFactory.setIntent(
                PendingIntent.getActivity(
                        context,
                        123,
                        new Intent(context, MainSettings.class),
                        PendingIntent.FLAG_IMMUTABLE));

        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mHelpAndFeedbackLauncher);
    }

    @Test
    @SmallTest
    public void testHelpMenuTriggersAutofillHelp() {
        SettingsActivity settingsActivity = mSettingsActivityTestRule.startSettingsActivity();

        onView(withId(R.id.menu_id_targeted_help)).perform(click());

        verify(mHelpAndFeedbackLauncher)
                .show(
                        settingsActivity,
                        ContextUtils.getApplicationContext()
                                .getString(R.string.help_context_autofill),
                        /* url= */ null);
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID})
    @Policies.Add({@Policies.Item(key = "PasswordManagerEnabled", string = "false")})
    public void testPasswordsItemManagedByOrganizationWhenDisabledByPolicy() {
        mSettingsActivityTestRule.startSettingsActivity();

        onView(
                        allOf(
                                withText(R.string.password_saving_off_by_administrator),
                                withEffectiveVisibility(Visibility.VISIBLE)))
                .check(matches(isDisplayed()));
        onView(withText(R.string.password_manager_settings_title))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()));
        onView(withText(R.string.password_manager_settings_title)).perform(click());

        assertNotNull(mSuccessCallbackHelper.getOnlyPayloadBlocking());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID})
    public void testPasswordsItemWhenNotManaged() {
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.password_manager_settings_title))
                .check(matches(isDisplayed()))
                .check(matches(isEnabled()));
        onView(withText(R.string.password_manager_settings_title)).perform(click());

        assertNotNull(mSuccessCallbackHelper.getOnlyPayloadBlocking());
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID})
    public void testPasswordsPreferenceErrorState() {
        when(mPasswordManagerUtilBridgeJniMock.isPasswordManagerAvailable(anyBoolean()))
                .thenReturn(false);

        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.gpm_stopped_working_subtitle)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID})
    public void testHomeOfTransactionsFormsAiPreferencesVisible() {
        mSettingsActivityTestRule.startSettingsActivity();
        HomeOfTransactionsFragment fragment = mSettingsActivityTestRule.getFragment();

        Preference identityDocsPref =
                fragment.findPreference(HomeOfTransactionsFragment.PREF_AUTOFILL_IDENTITY_DOCS);
        assertTrue(identityDocsPref.isVisible());

        Preference travelPref =
                fragment.findPreference(HomeOfTransactionsFragment.PREF_AUTOFILL_TRAVEL);
        assertTrue(travelPref.isVisible());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testSearchIndexWhenAllEnabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                            mSettingsActivityTestRule.getActivity(),
                            mSearchIndexDataMock,
                            mProfileMock);
                });

        verifyNoInteractions(mSearchIndexDataMock);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testSearchIndexEmptyWhenFeatureDisabled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                            mSettingsActivityTestRule.getActivity(),
                            mSearchIndexDataMock,
                            mProfileMock);
                });

        verify(mSearchIndexDataMock)
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_PASSWORDS));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_AUTOFILL_PAYMENTS));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_AUTOFILL_ADDRESSES));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_AUTOFILL_SETTINGS));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_AUTOFILL_IDENTITY_DOCS));
        verify(mSearchIndexDataMock)
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_AUTOFILL_TRAVEL));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testClickPaymentsLaunchesPayments() {
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_payments_title)).perform(click());

        onView(withText(R.string.autofill_enable_credit_cards_toggle_label))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testClickContactInfoLaunchesContactInfo() {
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_contact_info_title)).perform(click());

        onView(withText(R.string.autofill_enable_profiles_toggle_label))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testClickAutofillSettingsLaunchesAutofillOptions() {
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_settings_title)).perform(click());

        onView(withText(R.string.autofill_third_party_filling_default))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID,
        ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA
    })
    public void testClickIdentityDocsLaunchesIdentityDocs() {
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_identity_docs_title)).perform(click());

        onView(withText(R.string.autofill_identity_docs_opt_in_toggle_label))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testClickTravelLaunchesTravel() {
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_travel_title)).perform(click());

        onView(withText(R.string.autofill_travel_opt_in_toggle_label))
                .check(matches(isDisplayed()));
    }
}
