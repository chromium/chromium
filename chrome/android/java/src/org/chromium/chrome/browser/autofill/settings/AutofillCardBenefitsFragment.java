// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.text.style.ClickableSpan;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.fragment.app.Fragment;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.ui.text.SpanApplier;

/** Preferences fragment to allow users to manage card benefits linked to their credit cards. */
public class AutofillCardBenefitsFragment extends ChromeBaseSettingsFragment
        implements PersonalDataManager.PersonalDataManagerObserver,
                Preference.OnPreferenceChangeListener {
    public static final String CARD_BENEFITS_LEARN_MORE_CLICKED_USER_ACTION =
            "CardBenefits_LearnMoreLinkClicked";
    public static final String CARD_BENEFITS_TOGGLED_OFF_USER_ACTION = "CardBenefits_ToggledOff";
    public static final String CARD_BENEFITS_TOGGLED_ON_USER_ACTION = "CardBenefits_ToggledOn";
    public static final String LEARN_MORE_URL =
            "https://support.google.com/googlepay?p=card_benefits_chrome";

    @VisibleForTesting static final String PREF_KEY_ENABLE_CARD_BENEFIT = "enable_card_benefit";

    private static Callback<Fragment> sObserverForTest;

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    private PersonalDataManager mPersonalDataManager;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mPageTitle.set(getString(R.string.autofill_card_benefits_settings_page_title));

        // Create blank preference screen.
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        setPreferenceScreen(screen);
        if (sObserverForTest != null) {
            sObserverForTest.onResult(this);
        }
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    // ChromeBaseSettingsFramgent override.
    @Override
    public void onResume() {
        super.onResume();
        // Rebuild the preference list in case any of the underlying data has been updated and if
        // any preferences need to be added/removed based on that.
        rebuildPage();
    }

    private void rebuildPage() {
        getPreferenceScreen().removeAll();
        getPreferenceScreen().setOrderingAsAdded(true);

        createCardBenefitSwitch();
        createLearnAboutCardBenefitsLink();
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }

    @VisibleForTesting
    static void setObserverForTest(Callback<Fragment> observerForTest) {
        sObserverForTest = observerForTest;
    }

    private void createCardBenefitSwitch() {
        ChromeSwitchPreference cardBenefitSwitch = new ChromeSwitchPreference(getStyledContext());
        cardBenefitSwitch.setTitle(R.string.autofill_settings_page_card_benefits_label);
        cardBenefitSwitch.setSummary(R.string.autofill_settings_page_card_benefits_toggle_summary);
        cardBenefitSwitch.setKey(PREF_KEY_ENABLE_CARD_BENEFIT);
        cardBenefitSwitch.setChecked(mPersonalDataManager.isCardBenefitEnabled());
        cardBenefitSwitch.setOnPreferenceChangeListener(this);
        getPreferenceScreen().addPreference(cardBenefitSwitch);
    }

    private void createLearnAboutCardBenefitsLink() {
        TextMessagePreference learnAboutLinkPreference =
                new TextMessagePreference(getStyledContext(), /* attrs= */ null);
        learnAboutLinkPreference.setSummary(
                SpanApplier.applySpans(
                        getString(
                                R.string
                                        .autofill_settings_page_card_benefits_learn_about_link_text),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ClickableSpan() {
                                    @Override
                                    public void onClick(View view) {
                                        openUrlInCct(LEARN_MORE_URL);
                                        RecordUserAction.record(
                                                CARD_BENEFITS_LEARN_MORE_CLICKED_USER_ACTION);
                                    }
                                })));
        learnAboutLinkPreference.setDividerAllowedAbove(false);
        learnAboutLinkPreference.setDividerAllowedBelow(false);
        getPreferenceScreen().addPreference(learnAboutLinkPreference);
    }

    private void openUrlInCct(String url) {
        new CustomTabsIntent.Builder()
                .setShowTitle(true)
                .build()
                .launchUrl(getContext(), Uri.parse(url));
    }

    @Override
    public void onPersonalDataChanged() {
        rebuildPage();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        boolean prefEnabled = (boolean) newValue;
        mPersonalDataManager.setCardBenefit(prefEnabled);
        RecordUserAction.record(
                prefEnabled
                        ? CARD_BENEFITS_TOGGLED_ON_USER_ACTION
                        : CARD_BENEFITS_TOGGLED_OFF_USER_ACTION);
        return true;
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        mPersonalDataManager = PersonalDataManagerFactory.getForProfile(getProfile());
        mPersonalDataManager.registerDataObserver(this);
    }

    @Override
    public void onDestroyView() {
        mPersonalDataManager.unregisterDataObserver(this);
        super.onDestroyView();
    }
}
