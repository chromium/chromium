// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.payments.AddressEditor;
import org.chromium.chrome.browser.payments.AutofillAddress;

import java.util.ArrayList;
import java.util.List;

/**
 * The payment method section of the Autofill Assistant payment request.
 */
public class AssistantShippingAddressSection
        extends AssistantCollectUserDataSection<AutofillAddress> {
    private AddressEditor mEditor;
    private boolean mIgnoreProfileChangeNotifications;

    AssistantShippingAddressSection(Context context, ViewGroup parent) {
        super(context, parent, R.layout.autofill_assistant_address_summary,
                R.layout.autofill_assistant_address_full,
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_payment_request_title_padding),
                context.getString(R.string.payments_add_address),
                context.getString(R.string.payments_add_address));
        setTitle(context.getString(R.string.payments_shipping_address_label));
    }

    public void setEditor(AddressEditor editor) {
        mEditor = editor;
    }

    @Override
    protected void createOrEditItem(@Nullable AutofillAddress oldItem) {
        if (mEditor == null) {
            return;
        }
        mIgnoreProfileChangeNotifications = true;
        mEditor.edit(oldItem, newItem -> {
            assert (newItem != null && newItem.isComplete());
            addOrUpdateItem(newItem, true);
        }, cancel -> {});
        mIgnoreProfileChangeNotifications = false;
    }

    @Override
    protected void updateFullView(View fullView, AutofillAddress address) {
        if (address == null) {
            return;
        }
        TextView fullNameView = fullView.findViewById(R.id.full_name);
        fullNameView.setText(address.getProfile().getFullName());
        hideIfEmpty(fullNameView);

        TextView fullAddressView = fullView.findViewById(R.id.full_address);
        fullAddressView.setText(
                PersonalDataManager.getInstance()
                        .getShippingAddressLabelWithCountryForPaymentRequest(address.getProfile()));
        hideIfEmpty(fullAddressView);

        TextView methodIncompleteView = fullView.findViewById(R.id.incomplete_error);
        methodIncompleteView.setVisibility(address.isComplete() ? View.GONE : View.VISIBLE);
    }

    @Override
    protected void updateSummaryView(View summaryView, AutofillAddress address) {
        if (address == null) {
            return;
        }
        TextView fullNameView = summaryView.findViewById(R.id.full_name);
        fullNameView.setText(address.getProfile().getFullName());
        hideIfEmpty(fullNameView);

        TextView shortAddressView = summaryView.findViewById(R.id.short_address);
        shortAddressView.setText(PersonalDataManager.getInstance()
                                         .getShippingAddressLabelWithoutCountryForPaymentRequest(
                                                 address.getProfile()));
        hideIfEmpty(shortAddressView);

        TextView methodIncompleteView = summaryView.findViewById(R.id.incomplete_error);
        methodIncompleteView.setVisibility(address.isComplete() ? View.GONE : View.VISIBLE);
    }

    @Override
    protected boolean canEditOption(AutofillAddress address) {
        return true;
    }

    @Override
    protected @DrawableRes int getEditButtonDrawable(AutofillAddress address) {
        return R.drawable.ic_edit_24dp;
    }

    @Override
    protected String getEditButtonContentDescription(AutofillAddress address) {
        return mContext.getString(R.string.payments_edit_address);
    }

    void onProfilesChanged(List<PersonalDataManager.AutofillProfile> profiles) {
        if (mIgnoreProfileChangeNotifications) {
            return;
        }

        AutofillAddress previouslySelectedAddress = mSelectedOption;
        int selectedAddressIndex = -1;
        List<AutofillAddress> addresses = new ArrayList<>();
        for (int i = 0; i < profiles.size(); i++) {
            AutofillAddress autofillAddress = new AutofillAddress(mContext, profiles.get(i));
            if (previouslySelectedAddress != null
                    && TextUtils.equals(autofillAddress.getIdentifier(),
                            previouslySelectedAddress.getIdentifier())) {
                selectedAddressIndex = i;
            }
            addresses.add(autofillAddress);
        }

        // Replace current set of items, keep selection if possible.
        setItems(addresses, selectedAddressIndex);
    }

    @Override
    protected void addOrUpdateItem(AutofillAddress address, boolean select) {
        super.addOrUpdateItem(address, select);

        // Update autocomplete information in the editor.
        if (mEditor == null) {
            return;
        }
        mEditor.addPhoneNumberIfValid(address.getProfile().getPhoneNumber());
    }
}