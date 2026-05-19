// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.only;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;

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
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManager;
import org.chromium.chrome.browser.autofill.autofill_ai.EntityDataManagerFactory;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.password_manager.CredentialManagerLauncherFactory;
import org.chromium.chrome.browser.password_manager.FakeCredentialManagerLauncherFactoryImpl;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.SigninPreferencesManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.signin_promo.AutofillAndPasswordsPromoDelegate;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;

/** Tests for {@link HomeOfTransactionsFragment}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class HomeOfTransactionsFragmentTest {
    @Rule(order = 0)
    public SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule(order = 1)
    public SettingsActivityTestRule<HomeOfTransactionsFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(HomeOfTransactionsFragment.class);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsIndexData mSearchIndexDataMock;
    @Mock private Profile mProfileMock;
    @Mock private EntityDataManager mEntityDataManager;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;
    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private SigninAndHistorySyncActivityLauncher mSigninLauncher;
    @Mock private BottomSheetSigninAndHistorySyncCoordinator mSettingsSigninCoordinator;
    @Mock private BottomSheetSigninAndHistorySyncCoordinator mAutofillAndPasswordsSigninCoordinator;

    private final FakeCredentialManagerLauncherFactoryImpl mFakeLauncherFactory =
            new FakeCredentialManagerLauncherFactoryImpl();
    private final PayloadCallbackHelper<PendingIntent> mSuccessCallbackHelper =
            new PayloadCallbackHelper<>();

    @Before
    public void setUp() {
        EntityDataManagerFactory.setInstanceForTesting(mEntityDataManager);
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

        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(mSigninLauncher);

        when(mSigninLauncher.createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        eq(SigninAccessPoint.SETTINGS_AUTOFILL_AND_PASSWORDS)))
                .thenReturn(mAutofillAndPasswordsSigninCoordinator);

        // Required for multi-pane tests involving MainSettings.
        when(mSigninLauncher.createBottomSheetSigninCoordinatorAndObserveAddAccountResult(
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        any(),
                        eq(SigninAccessPoint.SETTINGS)))
                .thenReturn(mSettingsSigninCoordinator);

        // Dismiss the promo by default.
        signInPromoDeclined(true);
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SigninPromoAccessPointId
                                        .AUTOFILL_AND_PASSWORDS));
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
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/continueButton",
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID
    })
    public void testSignInPromoVisible_noAccount() {
        signInPromoDeclined(false);

        mSettingsActivityTestRule.startSettingsActivity();

        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_promo_title))
                .check(matches(withText(R.string.signin_account_picker_bottom_sheet_title)));
        onView(withId(R.id.signin_promo_description))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .signin_promo_description_autofill_and_passwords_seamless)));
        onView(withId(R.id.signin_promo_primary_button)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_promo_secondary_button)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/continueButton",
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID
    })
    public void testSignInPromoVisible_withAccount() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        signInPromoDeclined(false);

        mSettingsActivityTestRule.startSettingsActivity();

        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_promo_title))
                .check(matches(withText(R.string.signin_account_picker_bottom_sheet_title)));
        onView(withId(R.id.signin_promo_description))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .signin_promo_description_autofill_and_passwords_seamless)));
        onView(withId(R.id.signin_promo_primary_button)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_promo_secondary_button)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID})
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testSignInPromoVisible_seamlessDisabled() {
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        signInPromoDeclined(false);

        mSettingsActivityTestRule.startSettingsActivity();

        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_title))
                .check(matches(withText(R.string.signin_promo_title_autofill_and_passwords)));
        onView(withId(R.id.sync_promo_description))
                .check(matches(withText(R.string.signin_promo_description_autofill_and_passwords)));
        onView(withId(R.id.sync_promo_choose_account_button)).check(matches(isDisplayed()));
        onView(withId(R.id.sync_promo_signin_button)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/continueButton",
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID
    })
    public void testSignInPromoDismiss() {
        signInPromoDeclined(false);

        mSettingsActivityTestRule.startSettingsActivity();

        onView(withId(R.id.signin_promo_view_container)).check(matches(isDisplayed()));
        onView(withId(R.id.signin_promo_dismiss_button)).perform(click());

        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.SIGNIN_PROMO_AUTOFILL_AND_PASSWORDS_DISMISSED,
                                false));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/continueButton",
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID
    })
    public void testSignInPromoClick() {
        signInPromoDeclined(false);

        mSettingsActivityTestRule.startSettingsActivity();

        onView(withId(R.id.signin_promo_primary_button)).perform(click());

        verify(mAutofillAndPasswordsSigninCoordinator).startSigninFlow(any());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        SigninFeatures.ENABLE_SEAMLESS_SIGNIN
                + ":seamless-signin-promo-type/compact"
                + "/seamless-signin-string-type/continueButton",
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID
    })
    public void testSignInPromoMaxImpressions() {
        signInPromoDeclined(false);
        ChromeSharedPreferences.getInstance()
                .writeInt(
                        ChromePreferenceKeys.SYNC_PROMO_SHOW_COUNT.createKey(
                                SigninPreferencesManager.SigninPromoAccessPointId
                                        .AUTOFILL_AND_PASSWORDS),
                        AutofillAndPasswordsPromoDelegate.MAX_IMPRESSIONS);

        mSettingsActivityTestRule.startSettingsActivity();

        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
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
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testSearchIndexWhenAllEnabled() {
        when(mEntityDataManager.canListEntityInstancesInSettings()).thenReturn(true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.updateDynamicPreferences(
                            mSettingsActivityTestRule.getActivity(),
                            mSearchIndexDataMock,
                            mProfileMock);
                });

        verify(mSearchIndexDataMock, only())
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_SIGNIN_PROMO));
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

        verify(mSearchIndexDataMock)
                .removeEntry(
                        HomeOfTransactionsFragment.SEARCH_INDEX_DATA_PROVIDER.getUniqueId(
                                HomeOfTransactionsFragment.PREF_SIGNIN_PROMO));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testClickPaymentsLaunchesPayments() {
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_payments_title)).perform(click());

        onView(withText(R.string.autofill_enable_credit_cards_toggle_label))
                .check(matches(isDisplayed()));
        assertHasBackstack();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testClickContactInfoLaunchesContactInfo() {
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_contact_info_title)).perform(click());

        onView(withText(R.string.autofill_enable_profiles_toggle_label))
                .check(matches(isDisplayed()));
        assertHasBackstack();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
    public void testClickAutofillSettingsLaunchesAutofillOptions() {
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_settings_title)).perform(click());

        onView(withText(R.string.autofill_third_party_filling_default))
                .check(matches(isDisplayed()));
        assertHasBackstack();
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID,
        ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA
    })
    public void testClickIdentityDocsLaunchesIdentityDocs() {
        when(mEntityDataManager.canListEntityInstancesInSettings()).thenReturn(true);
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_identity_docs_title)).perform(click());

        onView(withText(R.string.autofill_identity_docs_opt_in_toggle_label))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testIdentityDocsNotVisibleWhenCannotListEntities() {
        when(mEntityDataManager.canListEntityInstancesInSettings()).thenReturn(false);
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_identity_docs_title)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID,
        ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA
    })
    public void testClickTravelLaunchesTravel() {
        when(mEntityDataManager.canListEntityInstancesInSettings()).thenReturn(true);
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_travel_title)).perform(click());

        onView(withText(R.string.autofill_travel_opt_in_toggle_label))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testTravelNotVisibleWhenCannotListEntities() {
        when(mEntityDataManager.canListEntityInstancesInSettings()).thenReturn(false);
        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(R.string.autofill_travel_title)).check(doesNotExist());
    }

    private static void signInPromoDeclined(boolean value) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(
                        ChromePreferenceKeys.SIGNIN_PROMO_AUTOFILL_AND_PASSWORDS_DISMISSED, value);
    }
}
