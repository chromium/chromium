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

import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantAutofillUtilChrome;
import org.chromium.chrome.browser.autofill_assistant.AssistantEditor.AssistantAddressEditor;
import org.chromium.chrome.browser.autofill_assistant.AssistantOptionModel.AddressModel;

import java.util.List;

/**
 * The shipping address section of the Autofill Assistant payment request.
 */
public class AssistantShippingAddressSection extends AssistantCollectUserDataSection<AddressModel> {
    @Nullable
    private AssistantAddressEditor mEditor;
    private boolean mIgnoreProfileChangeNotifications;

    AssistantShippingAddressSection(Context context, ViewGroup parent) {
        super(context, parent, R.layout.autofill_assistant_address_summary,
                R.layout.autofill_assistant_address_full,
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_payment_request_title_padding),
                context.getString(R.string.payments_add_address),
                context.getString(R.string.payments_add_address));
    }

    public void setEditor(@Nullable AssistantAddressEditor editor) {
        mEditor = editor;
    }

    @Override
    protected void createOrEditItem(@Nullable AddressModel oldItem) {
        if (mEditor == null) {
            return;
        }

        Callback<AddressModel> doneCallback = editedItem -> {
            mIgnoreProfileChangeNotifications = true;
            addOrUpdateItem(editedItem,
                    /* select= */ true, /* notify= */ true);
            mIgnoreProfileChangeNotifications = false;
        };

        Callback<AddressModel> cancelCallback = ignoredItem -> {};

        mEditor.createOrEditItem(oldItem, doneCallback, cancelCallback);
    }

    @Override
    protected void updateFullView(View fullView, @Nullable AddressModel model) {
        if (model == null) {
            return;
        }
        TextView fullNameView = fullView.findViewById(R.id.full_name);
        fullNameView.setText(model.mOption.getFullName());
        hideIfEmpty(fullNameView);

        TextView fullAddressView = fullView.findViewById(R.id.full_address);
        // TODO(b/211748133): Remove dependency to AutofillUtilChrome.
        fullAddressView.setText(AssistantAutofillUtilChrome.getShippingAddressLabel(
                model.mOption, /* withCountry= */ true));
        hideIfEmpty(fullAddressView);

        TextView errorView = fullView.findViewById(R.id.incomplete_error);
        if (model.mErrors.isEmpty()) {
            errorView.setText("");
            errorView.setVisibility(View.GONE);
        } else {
            errorView.setText(TextUtils.join("\n", model.mErrors));
            errorView.setVisibility(View.VISIBLE);
        }
    }

    @Override
    protected void updateSummaryView(View summaryView, @Nullable AddressModel model) {
        if (model == null) {
            return;
        }
        TextView fullNameView = summaryView.findViewById(R.id.full_name);
        fullNameView.setText(model.mOption.getFullName());
        hideIfEmpty(fullNameView);

        TextView shortAddressView = summaryView.findViewById(R.id.short_address);
        // TODO(b/211748133): Remove dependency to AutofillUtilChrome.
        shortAddressView.setText(AssistantAutofillUtilChrome.getShippingAddressLabel(
                model.mOption, /* withCountry= */ false));
        hideIfEmpty(shortAddressView);

        TextView errorView = summaryView.findViewById(R.id.incomplete_error);
        errorView.setVisibility(model.mErrors.isEmpty() ? View.GONE : View.VISIBLE);
    }

    @Override
    protected boolean canEditOption(AddressModel model) {
        return true;
    }

    @Override
    protected @DrawableRes int getEditButtonDrawable(AddressModel model) {
        return R.drawable.ic_edit_24dp;
    }

    @Override
    protected String getEditButtonContentDescription(AddressModel model) {
        return mContext.getString(R.string.payments_edit_address);
    }

    @Override
    protected boolean areEqual(AddressModel modelA, AddressModel modelB) {
        if (modelA == null || modelB == null) {
            return modelA == modelB;
        }
        return TextUtils.equals(modelA.mOption.getGUID(), modelB.mOption.getGUID());
    }

    /**
     * The Chrome profiles have changed externally. This will rebuild the UI with the new/changed
     * set of addresses derived from the profiles, while keeping the selected item if possible.
     */
    void onAddressesChanged(List<AddressModel> addresses) {
        if (mIgnoreProfileChangeNotifications) {
            return;
        }

        int selectedAddressIndex = -1;
        if (mSelectedOption != null) {
            for (int i = 0; i < addresses.size(); i++) {
                if (areEqual(addresses.get(i), mSelectedOption)) {
                    selectedAddressIndex = i;
                    break;
                }
            }
        }

        // Replace current set of items, keep selection if possible.
        setItems(addresses, selectedAddressIndex);
    }
}