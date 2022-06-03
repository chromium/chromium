// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.payments.SettingsAutofillAndPaymentsObserver;

/**
 * Server credit card settings.
 */
public class AutofillServerCardEditor extends AutofillCreditCardEditor {
    private View mLocalCopyLabel;
    private View mClearLocalCopy;

    @UsedByReflection("AutofillPaymentMethodsFragment.java")
    public AutofillServerCardEditor() {}

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final View v = super.onCreateView(inflater, container, savedInstanceState);
        if (mCard == null) {
            getActivity().finish();
            return v;
        }

        ((TextView) v.findViewById(R.id.title)).setText(mCard.getObfuscatedNumber());
        ((TextView) v.findViewById(R.id.summary))
                .setText(mCard.getFormattedExpirationDate(getActivity()));
        v.findViewById(R.id.edit_server_card).setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                CustomTabActivity.showInfoPage(
                        getActivity(), ChromeStringConstants.AUTOFILL_MANAGE_WALLET_CARD_URL);
            }
        });

        mLocalCopyLabel = v.findViewById(R.id.local_copy_label);
        mClearLocalCopy = v.findViewById(R.id.clear_local_copy);

        if (mCard.getIsCached()) {
            mClearLocalCopy.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    PersonalDataManager.getInstance().clearUnmaskedCache(mGUID);
                    removeLocalCopyViews();
                }
            });
        } else {
            removeLocalCopyViews();
        }

        initializeButtons(v);
        return v;
    }

    private void removeLocalCopyViews() {
        ViewGroup parent = (ViewGroup) mClearLocalCopy.getParent();
        if (parent == null) return;

        parent.removeView(mLocalCopyLabel);
        parent.removeView(mClearLocalCopy);
    }

    @Override
    protected int getLayoutId() {
        return R.layout.autofill_server_card_editor;
    }

    @Override
    protected int getTitleResourceId(boolean isNewEntry) {
        return R.string.autofill_edit_credit_card;
    }

    @Override
    public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        if (parent == mBillingAddress && position != mInitialBillingAddressPos) {
            ((Button) getView().findViewById(R.id.button_primary)).setEnabled(true);
        }
    }

    @Override
    protected boolean saveEntry() {
        if (mBillingAddress.getSelectedItem() != null
                && mBillingAddress.getSelectedItem() instanceof AutofillProfile) {
            mCard.setBillingAddressId(
                    ((AutofillProfile) mBillingAddress.getSelectedItem()).getGUID());
            PersonalDataManager.getInstance().updateServerCardBillingAddress(mCard);
            SettingsAutofillAndPaymentsObserver.getInstance().notifyOnCreditCardUpdated(mCard);
        }
        return true;
    }

    @Override
    protected boolean getIsDeletable() {
        return false;
    }
}
