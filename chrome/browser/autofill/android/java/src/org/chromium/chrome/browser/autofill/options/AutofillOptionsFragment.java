// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.options;

import android.content.Context;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.annotation.IntDef;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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

    private @AutofillOptionsReferrer int mReferrer;

    // Represents different referrers when navigating to the Autofill Options page.
    //
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    //
    // Needs to stay in sync with AutofillOptionsReferrer in enums.xml.
    @IntDef({
        AutofillOptionsReferrer.SETTINGS,
        AutofillOptionsReferrer.DEEP_LINK_TO_SETTINGS,
        AutofillOptionsReferrer.PAYMENT_METHODS_FRAGMENT,
        AutofillOptionsReferrer.AUTOFILL_PROFILES_FRAGMENT,
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

        int COUNT = 4;
    }

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

    @Nullable Preference getAutofillServiceProviderCategory() {
        return findPreference(PREF_AUTOFILL_SERVICE_PROVIDER_CETEGORY);
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        setHasOptionsMenu(true);
        SettingsUtils.addPreferencesFromResource(this, R.xml.autofill_options_preferences);
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

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(R.drawable.ic_help_24dp);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            getHelpAndFeedbackLauncher()
                    .show(
                            getActivity(),
                            getActivity().getString(R.string.help_context_autofill),
                            null);
            return true;
        }
        return super.onOptionsItemSelected(item);
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

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    AutofillOptionsFragment.class.getName(), R.xml.autofill_options_preferences) {
                @Override
                public Bundle getExtras() {
                    return createRequiredArgs(AutofillOptionsReferrer.SETTINGS);
                }

                @Override
                public void updateDynamicPreferences(Context context, SettingsIndexData indexData) {
                    indexData.removeEntry(getUniqueId(PREF_THIRD_PARTY_TOGGLE_HINT));
                    if (!isAutofillAiEnabled()) {
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_AI_SWITCH));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_AI_AUTHENTICATION_SWITCH));
                        indexData.removeEntry(getUniqueId(PREF_AUTOFILL_SERVICE_PROVIDER_CETEGORY));
                    }
                }
            };
}
