// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings.options;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.IntDef;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.autofill.settings.AutofillHelpMenuProvider;
import org.chromium.chrome.browser.autofill.settings.personal_context.AutofillPersonalContextFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Autofill options fragment, which allows the user to configure autofill. */
@NullMarked
public class AutofillOptionsFragment extends ChromeBaseSettingsFragment {
    private static @Nullable Callback<Fragment> sObserverForTest;

    // Key for the argument with which the AutofillOptions fragment will be launched. The value for
    // this argument is part of the AutofillOptionsReferrer enum containing all entry points.
    public static final String AUTOFILL_OPTIONS_REFERRER = "autofill-options-referrer";
    public static final String PREF_AUTOFILL_THIRD_PARTY_FILLING = "autofill_third_party_filling";
    public static final String PREF_THIRD_PARTY_TOGGLE_HINT = "third_party_toggle_hint";
    public static final String PREF_AUTOFILL_AI_SWITCH = "autofill_ai_switch";
    public static final String PREF_AUTOFILL_AI_AUTHENTICATION_SWITCH =
            "autofill_ai_authentication_switch";
    public static final String PREF_AUTOFILL_AI_CATEGORY = "autofill_ai_category";
    public static final String PREF_AUTOFILL_SERVICE_PROVIDER_CETEGORY =
            "autofill_service_provider_category";
    public static final String PREF_AUTOFILL_PERSONAL_CONTEXT_SWITCH =
            "autofill_personal_context_switch";
    public static final String PREF_AUTOFILL_PERSONAL_CONTEXT_MANAGE_CONNECTED_APPS =
            "autofill_personal_context_manage_connected_apps";
    public static final String PREF_AUTOFILL_PERSONAL_CONTEXT_CATEGORY =
            "autofill_personal_context_category";

    private @AutofillOptionsReferrer int mReferrer;

    // Represents different referrers when navigating to the Autofill Options page.
    //
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    //
    // Needs to stay in sync with AutofillOptionsReferrer in enums.xml.
    // LINT.IfChange(AutofillOptionsReferrer)
    @IntDef({
        AutofillOptionsReferrer.SETTINGS,
        AutofillOptionsReferrer.DEEP_LINK_TO_SETTINGS,
        AutofillOptionsReferrer.PAYMENT_METHODS_FRAGMENT,
        AutofillOptionsReferrer.AUTOFILL_PROFILES_FRAGMENT,
        AutofillOptionsReferrer.AUTOFILL_AND_PASSWORDS_FRAGMENT,
        AutofillOptionsReferrer.AUTOFILL_IDENTITY_DOCS_FRAGMENT,
        AutofillOptionsReferrer.AUTOFILL_TRAVEL_FRAGMENT,
        AutofillOptionsReferrer.AUTOFILL_SHOPPING_FRAGMENT,
        AutofillOptionsReferrer.PRIVATE_INFERENCE_NOTICE,
        AutofillOptionsReferrer.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AutofillOptionsReferrer {
        /** Corresponds to the Settings page. */
        int SETTINGS = 0;

        /** Corresponds to an external link opening Chrome. */
        int DEEP_LINK_TO_SETTINGS = 1;

        /** Payment methods fragment in Chrome settings. */
        int PAYMENT_METHODS_FRAGMENT = 2;

        /** Profiles fragment in Chrome settings. */
        int AUTOFILL_PROFILES_FRAGMENT = 3;

        /** Autofill and passwords in Chrome settings. */
        int AUTOFILL_AND_PASSWORDS_FRAGMENT = 4;

        /** Identity docs fragment in Chrome settings. */
        int AUTOFILL_IDENTITY_DOCS_FRAGMENT = 5;

        /** Travel fragment in Chrome settings. */
        int AUTOFILL_TRAVEL_FRAGMENT = 6;

        /** Shopping fragment in Chrome settings. */
        int AUTOFILL_SHOPPING_FRAGMENT = 7;

        /** Private inference notice. */
        int PRIVATE_INFERENCE_NOTICE = 8;

        int COUNT = 9;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/autofill/enums.xml:AutofillOptionsReferrer)

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    /** This default constructor is required to instantiate the fragment. */
    public AutofillOptionsFragment() {}

    RadioButtonGroupThirdPartyPreference getThirdPartyFillingOption() {
        RadioButtonGroupThirdPartyPreference thirdPartyFillingSwitch =
                findPreference(PREF_AUTOFILL_THIRD_PARTY_FILLING);
        assert thirdPartyFillingSwitch != null;
        return thirdPartyFillingSwitch;
    }

    ChromeSwitchPreference getAutofillAiSwitch() {
        ChromeSwitchPreference autofillAiSwitch = findPreference(PREF_AUTOFILL_AI_SWITCH);
        assert autofillAiSwitch != null;
        return autofillAiSwitch;
    }

    ChromeSwitchPreference getAutofillAiAuthenticationSwitch() {
        ChromeSwitchPreference autofillAiAuthenticationSwitch =
                findPreference(PREF_AUTOFILL_AI_AUTHENTICATION_SWITCH);
        assert autofillAiAuthenticationSwitch != null;
        return autofillAiAuthenticationSwitch;
    }

    TextMessagePreference getHint() {
        TextMessagePreference hint = findPreference(PREF_THIRD_PARTY_TOGGLE_HINT);
        assert hint != null;
        return hint;
    }

    @Nullable Preference getAutofillAiCategory() {
        return findPreference(PREF_AUTOFILL_AI_CATEGORY);
    }

    ChromeSwitchPreference getAutofillPersonalContextSwitch() {
        return assumeNonNull(findPreference(PREF_AUTOFILL_PERSONAL_CONTEXT_SWITCH));
    }

    Preference getAutofillPersonalContextManageConnectedApps() {
        return assumeNonNull(findPreference(PREF_AUTOFILL_PERSONAL_CONTEXT_MANAGE_CONNECTED_APPS));
    }

    Preference getAutofillPersonalContextCategory() {
        return assumeNonNull(findPreference(PREF_AUTOFILL_PERSONAL_CONTEXT_CATEGORY));
    }

    @Nullable Preference getAutofillServiceProviderCategory() {
        return findPreference(PREF_AUTOFILL_SERVICE_PROVIDER_CETEGORY);
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        requireActivity()
                .addMenuProvider(new AutofillHelpMenuProvider(this), this, Lifecycle.State.RESUMED);
        SettingsUtils.addPreferencesFromResource(this, R.xml.autofill_options_preferences);

        AutofillAiPreference autofillAiPreference = findPreference(PREF_AUTOFILL_AI_SWITCH);
        autofillAiPreference.setProfile(getProfile());
    }

    @Override
    public SettableMonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mReferrer = getReferrerFromInstanceStateOrLaunchBundle(savedInstanceState);

        if (sObserverForTest != null) {
            sObserverForTest.onResult(this);
        }
    }

    public static void setObserverForTest(Callback<Fragment> observerForTest) {
        sObserverForTest = observerForTest;
        ResettersForTesting.register(() -> sObserverForTest = null);
    }

    public static Bundle createRequiredArgs(@AutofillOptionsReferrer int referrer) {
        Bundle requiredArgs = new Bundle();
        requiredArgs.putInt(AUTOFILL_OPTIONS_REFERRER, referrer);
        return requiredArgs;
    }

    @AutofillOptionsReferrer
    int getReferrer() {
        return mReferrer;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putInt(AUTOFILL_OPTIONS_REFERRER, mReferrer);
    }

    private @AutofillOptionsReferrer int getReferrerFromInstanceStateOrLaunchBundle(
            @Nullable Bundle savedInstanceState) {
        if (savedInstanceState != null
                && savedInstanceState.containsKey(AUTOFILL_OPTIONS_REFERRER)) {
            return savedInstanceState.getInt(AUTOFILL_OPTIONS_REFERRER);
        }
        Bundle extras = getArguments();
        assert extras.containsKey(AUTOFILL_OPTIONS_REFERRER)
                : "missing autofill-options-referrer fragment";
        return extras.getInt(AUTOFILL_OPTIONS_REFERRER);
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "autofill_options";
    }

    static boolean isAutofillAiEnabled() {
        // LINT.IfChange(AutofillEnabledCheckFragment)
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_AI_WITH_DATA_SCHEMA);
        // LINT.ThenChange(:AddAddAddressButtonMediator)
    }

    static boolean isAutofillAiReauthEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_AI_REAUTH_REQUIRED);
    }

    public static boolean shouldShowPersonalContext(Profile profile) {
        return !ChromeFeatureList.isEnabled(ChromeFeatureList.YOUR_SAVED_INFO_SETTINGS_PAGE_ANDROID)
                && AutofillPersonalContextFragment.isPersonalContextEligible(profile);
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    AutofillOptionsFragment.class.getName(), R.xml.autofill_options_preferences) {
                @Override
                public Bundle getExtras() {
                    return createRequiredArgs(AutofillOptionsReferrer.SETTINGS);
                }

                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    indexData.removeEntry(getUniqueId(PREF_THIRD_PARTY_TOGGLE_HINT));
                    if (!isAutofillAiEnabled()) {
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_AI_SWITCH));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_AI_AUTHENTICATION_SWITCH));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_SERVICE_PROVIDER_CETEGORY));
                    } else {
                        if (!isAutofillAiReauthEnabled()) {
                            indexData.removeEntry(
                                    getUniqueId(PREF_AUTOFILL_AI_AUTHENTICATION_SWITCH));
                        }
                    }
                    if (!AutofillOptionsFragment.shouldShowPersonalContext(profile)) {
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_PERSONAL_CONTEXT_SWITCH));
                        indexData.removeEntry(
                                getUniqueId(PREF_AUTOFILL_PERSONAL_CONTEXT_MANAGE_CONNECTED_APPS));
                    }
                }
            };
}
