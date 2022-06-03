// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.os.Build;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Spinner;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;

import java.util.List;

/**
 * The base class for credit card settings.
 */
abstract class AutofillCreditCardEditor extends AutofillEditorBase {
    protected CreditCard mCard;
    protected Spinner mBillingAddress;
    protected int mInitialBillingAddressPos;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View v = super.onCreateView(inflater, container, savedInstanceState);

        // Do not use autofill for the fields.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            getActivity().getWindow().getDecorView().setImportantForAutofill(
                    View.IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS);
        }

        // Populate the billing address dropdown.
        ArrayAdapter<AutofillProfile> profilesAdapter = new ArrayAdapter<AutofillProfile>(
                getActivity(), android.R.layout.simple_spinner_item);
        profilesAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);

        AutofillProfile noSelection = new AutofillProfile();
        noSelection.setLabel(getActivity().getString(R.string.select));
        profilesAdapter.add(noSelection);

        List<AutofillProfile> profiles = PersonalDataManager.getInstance().getProfilesForSettings();
        for (int i = 0; i < profiles.size(); i++) {
            AutofillProfile profile = profiles.get(i);
            if (profile.getIsLocal() && !TextUtils.isEmpty(profile.getStreetAddress())) {
                profilesAdapter.add(profile);
            }
        }

        mBillingAddress =
                (Spinner) v.findViewById(R.id.autofill_credit_card_editor_billing_address_spinner);
        mBillingAddress.setAdapter(profilesAdapter);

        // TODO(rouslan): Use an [+ ADD ADDRESS] button instead of disabling the dropdown.
        // http://crbug.com/623629
        if (profilesAdapter.getCount() == 1) mBillingAddress.setEnabled(false);

        mCard = PersonalDataManager.getInstance().getCreditCard(mGUID);
        if (mCard != null) {
            if (!TextUtils.isEmpty(mCard.getBillingAddressId())) {
                for (int i = 0; i < mBillingAddress.getAdapter().getCount(); i++) {
                    AutofillProfile profile =
                            (AutofillProfile) mBillingAddress.getAdapter().getItem(i);
                    if (TextUtils.equals(profile.getGUID(), mCard.getBillingAddressId())) {
                        mInitialBillingAddressPos = i;
                        mBillingAddress.setSelection(i);
                        break;
                    }
                }
            }
        }

        return v;
    }

    @Override
    protected void initializeButtons(View v) {
        super.initializeButtons(v);

        mBillingAddress.setOnItemSelectedListener(this);

        // Listen for touch events on billing address field. We clear the keyboard when user touches
        // the billing address field because it is a drop down menu.
        mBillingAddress.setOnTouchListener(this);
    }
}
