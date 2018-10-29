// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.autofill;

import android.graphics.PorterDuff;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceFragment;
import android.support.v7.content.res.AppCompatResources;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.payments.AndroidPaymentAppFactory;
import org.chromium.chrome.browser.payments.ServiceWorkerPaymentAppBridge;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.MainPreferences;
import org.chromium.chrome.browser.preferences.ManagedPreferenceDelegate;
import org.chromium.chrome.browser.preferences.PreferenceUtils;

/**
 * Autofill credit cards fragment, which allows the user to edit credit cards and control
 * payment apps.
 */
public class AutofillPaymentMethodsFragment
        extends PreferenceFragment implements PersonalDataManager.PersonalDataManagerObserver {
    private static final String PREF_PAYMENT_APPS = "payment_apps";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        PreferenceUtils.addPreferencesFromResource(
                this, R.xml.autofill_and_payments_preference_fragment_screen);
        getActivity().setTitle(R.string.autofill_payment_methods);
    }

    @Override
    public void onResume() {
        super.onResume();
        // Always rebuild our list of credit cards.  Although we could detect if credit cards are
        // added or deleted, the credit card summary (number) might be different.  To be safe, we
        // update all.
        rebuildPage();
    }

    private void rebuildPage() {
        getPreferenceScreen().removeAll();
        getPreferenceScreen().setOrderingAsAdded(true);

        ChromeSwitchPreference autofillSwitch = new ChromeSwitchPreference(getActivity(), null);
        autofillSwitch.setTitle(R.string.autofill_enable_credit_cards_toggle_label);
        autofillSwitch.setSummary(
                getActivity().getString(R.string.autofill_enable_credit_cards_toggle_sublabel));
        autofillSwitch.setChecked(PersonalDataManager.isAutofillCreditCardEnabled());
        autofillSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                PersonalDataManager.setAutofillCreditCardEnabled((boolean) newValue);
                return true;
            }
        });
        autofillSwitch.setManagedPreferenceDelegate(new ManagedPreferenceDelegate() {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return PersonalDataManager.isAutofillCreditCardManaged();
            }

            @Override
            public boolean isPreferenceClickDisabledByPolicy(Preference preference) {
                return PersonalDataManager.isAutofillCreditCardManaged()
                        && !PersonalDataManager.isAutofillProfileEnabled();
            }
        });
        getPreferenceScreen().addPreference(autofillSwitch);

        for (CreditCard card : PersonalDataManager.getInstance().getCreditCardsForSettings()) {
            // Add a preference for the credit card.
            Preference card_pref = new Preference(getActivity());
            card_pref.setTitle(card.getObfuscatedNumber());
            card_pref.setSummary(card.getFormattedExpirationDate(getActivity()));
            card_pref.setIcon(
                    AppCompatResources.getDrawable(getActivity(), card.getIssuerIconDrawableId()));

            if (card.getIsLocal()) {
                card_pref.setFragment(AutofillLocalCardEditor.class.getName());
            } else {
                card_pref.setFragment(AutofillServerCardEditor.class.getName());
                card_pref.setWidgetLayoutResource(R.layout.autofill_server_data_label);
            }

            Bundle args = card_pref.getExtras();
            args.putString(MainPreferences.AUTOFILL_GUID, card.getGUID());
            getPreferenceScreen().addPreference(card_pref);
        }

        // Add 'Add credit card' button. Tap of it brings up card editor which allows users type in
        // new credit cards.
        if (PersonalDataManager.isAutofillCreditCardEnabled()) {
            Preference add_card_pref = new Preference(getActivity());
            Drawable plusIcon = ApiCompatibilityUtils.getDrawable(getResources(), R.drawable.plus);
            plusIcon.mutate();
            plusIcon.setColorFilter(
                    ApiCompatibilityUtils.getColor(getResources(), R.color.pref_accent_color),
                    PorterDuff.Mode.SRC_IN);
            add_card_pref.setIcon(plusIcon);
            add_card_pref.setTitle(R.string.autofill_create_credit_card);
            add_card_pref.setFragment(AutofillLocalCardEditor.class.getName());
            getPreferenceScreen().addPreference(add_card_pref);
        }

        // Add the link to payment apps only after the credit card list is rebuilt.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.ANDROID_PAYMENT_APPS)
                || ChromeFeatureList.isEnabled(ChromeFeatureList.SERVICE_WORKER_PAYMENT_APPS)) {
            Preference payment_apps_pref = new Preference(getActivity());
            payment_apps_pref.setTitle(getActivity().getString(R.string.payment_apps_title));
            payment_apps_pref.setFragment(AndroidPaymentAppsFragment.class.getCanonicalName());
            payment_apps_pref.setShouldDisableView(true);
            payment_apps_pref.setKey(PREF_PAYMENT_APPS);
            getPreferenceScreen().addPreference(payment_apps_pref);
            refreshPaymentAppsPrefForAndroidPaymentApps(payment_apps_pref);
        }
    }

    private void refreshPaymentAppsPrefForAndroidPaymentApps(Preference pref) {
        if (AndroidPaymentAppFactory.hasAndroidPaymentApps()) {
            setPaymentAppsPrefStatus(pref, true);
        } else {
            refreshPaymentAppsPrefForServiceWorkerPaymentApps(pref);
        }
    }

    private void refreshPaymentAppsPrefForServiceWorkerPaymentApps(Preference pref) {
        ServiceWorkerPaymentAppBridge.hasServiceWorkerPaymentApps(
                new ServiceWorkerPaymentAppBridge.HasServiceWorkerPaymentAppsCallback() {
                    @Override
                    public void onHasServiceWorkerPaymentAppsResponse(boolean hasPaymentApps) {
                        setPaymentAppsPrefStatus(pref, hasPaymentApps);
                    }
                });
    }

    private void setPaymentAppsPrefStatus(Preference pref, boolean enabled) {
        if (enabled) {
            pref.setSummary(null);
            pref.setEnabled(true);
        } else {
            pref.setSummary(getActivity().getString(R.string.payment_no_apps_summary));
            pref.setEnabled(false);
        }
    }

    @Override
    public void onPersonalDataChanged() {
        rebuildPage();
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);
        PersonalDataManager.getInstance().registerDataObserver(this);
    }

    @Override
    public void onDestroyView() {
        PersonalDataManager.getInstance().unregisterDataObserver(this);
        super.onDestroyView();
    }
}
