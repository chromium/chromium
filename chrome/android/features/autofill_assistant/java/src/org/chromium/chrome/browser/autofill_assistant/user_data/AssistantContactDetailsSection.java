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
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.ContactEditor;

import java.util.ArrayList;
import java.util.List;

/**
 * The contact details section of the Autofill Assistant payment request.
 */
public class AssistantContactDetailsSection
        extends AssistantCollectUserDataSection<AutofillContact> {
    private ContactEditor mEditor;
    private boolean mIgnoreProfileChangeNotifications;
    private AssistantCollectUserDataModel.ContactDescriptionOptions mSummaryOptions;
    private AssistantCollectUserDataModel.ContactDescriptionOptions mFullOptions;

    AssistantContactDetailsSection(Context context, ViewGroup parent) {
        super(context, parent, R.layout.autofill_assistant_contact_summary,
                R.layout.autofill_assistant_contact_full,
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_payment_request_title_padding),
                context.getString(R.string.payments_add_contact),
                context.getString(R.string.payments_add_contact));
    }

    public void setEditor(ContactEditor editor) {
        mEditor = editor;
        if (mEditor == null) {
            return;
        }

        for (AutofillContact contact : getItems()) {
            addAutocompleteInformationToEditor(contact);
        }
    }

    @Override
    protected void createOrEditItem(@Nullable AutofillContact oldItem) {
        if (mEditor != null) {
            mEditor.edit(oldItem, newItem -> {
                assert (newItem != null && newItem.isComplete());
                mIgnoreProfileChangeNotifications = true;
                addOrUpdateItem(newItem, /* select= */ true, /* notify= */ true);
                mIgnoreProfileChangeNotifications = false;
            }, cancel -> {});
        }
    }

    @Override
    protected void updateFullView(View fullView, AutofillContact contact) {
        if (contact == null || mFullOptions == null) {
            return;
        }

        TextView fullViewText = fullView.findViewById(R.id.contact_full);
        String description = createContactDescription(mFullOptions, contact);
        fullViewText.setText(description);
        hideIfEmpty(fullViewText);

        fullView.findViewById(R.id.incomplete_error)
                .setVisibility(isComplete(contact) ? View.GONE : View.VISIBLE);
    }

    @Override
    protected void updateSummaryView(View summaryView, AutofillContact contact) {
        if (contact == null || mSummaryOptions == null) {
            return;
        }

        TextView contactSummaryView = summaryView.findViewById(R.id.contact_summary);
        String description = createContactDescription(mSummaryOptions, contact);
        contactSummaryView.setText(description);
        hideIfEmpty(contactSummaryView);

        summaryView.findViewById(R.id.incomplete_error)
                .setVisibility(isComplete(contact) ? View.GONE : View.VISIBLE);
    }

    @Override
    protected boolean canEditOption(AutofillContact contact) {
        return true;
    }

    @Override
    protected @DrawableRes int getEditButtonDrawable(AutofillContact contact) {
        return R.drawable.ic_edit_24dp;
    }

    @Override
    protected String getEditButtonContentDescription(AutofillContact contact) {
        return mContext.getString(R.string.payments_edit_contact_details_label);
    }

    @Override
    protected boolean areEqual(
            @Nullable AutofillContact optionA, @Nullable AutofillContact optionB) {
        if (optionA == null || optionB == null) {
            return optionA == optionB;
        }
        if (TextUtils.equals(optionA.getIdentifier(), optionB.getIdentifier())) {
            return true;
        }
        if (optionA.getProfile() == null || optionB.getProfile() == null) {
            return optionA.getProfile() == optionB.getProfile();
        }
        if (TextUtils.equals(optionA.getProfile().getGUID(), optionB.getProfile().getGUID())) {
            return true;
        }
        return optionA.isEqualOrSupersetOf(optionB) && optionB.isEqualOrSupersetOf(optionA);
    }

    /**
     * The Chrome profiles have changed externally. This will rebuild the UI with the new/changed
     * set of contacts derived from the profiles, while keeping the selected item if possible.
     */
    void onContactsChanged(List<AutofillContact> contacts) {
        if (mIgnoreProfileChangeNotifications) {
            return;
        }

        int selectedContactIndex = -1;
        if (mSelectedOption != null) {
            for (int i = 0; i < contacts.size(); i++) {
                if (areEqual(contacts.get(i), mSelectedOption)) {
                    selectedContactIndex = i;
                    break;
                }
            }
        }

        // Replace current set of items, keep selection if possible.
        setItems(contacts, selectedContactIndex);
    }

    void setContactSummaryOptions(AssistantCollectUserDataModel.ContactDescriptionOptions options) {
        mSummaryOptions = options;
        updateViews();
    }

    void setContactFullOptions(AssistantCollectUserDataModel.ContactDescriptionOptions options) {
        mFullOptions = options;
        updateViews();
    }

    @Override
    protected void addOrUpdateItem(AutofillContact contact, boolean select, boolean notify) {
        super.addOrUpdateItem(contact, select, notify);
        addAutocompleteInformationToEditor(contact);
    }

    private void addAutocompleteInformationToEditor(AutofillContact contact) {
        if (mEditor == null || contact == null) {
            return;
        }
        mEditor.addEmailAddressIfValid(contact.getPayerEmail());
        mEditor.addPayerNameIfValid(contact.getPayerName());
        mEditor.addPhoneNumberIfValid(contact.getPayerPhone());
    }

    /**
     * Creates a "\n"-separated description of {@code contact} using {@code options}.
     */
    private String createContactDescription(
            AssistantCollectUserDataModel.ContactDescriptionOptions options,
            AutofillContact contact) {
        List<String> descriptionLines = new ArrayList<>();
        for (int i = 0;
                i < options.mFields.length && descriptionLines.size() < options.mMaxNumberLines;
                i++) {
            String line = "";
            switch (options.mFields[i]) {
                case AssistantContactField.NAME_FULL:
                    line = contact.getPayerName();
                    break;
                case AssistantContactField.EMAIL_ADDRESS:
                    line = contact.getPayerEmail();
                    break;
                case AssistantContactField.PHONE_HOME_WHOLE_NUMBER:
                    line = contact.getPayerPhone();
                    break;
                default:
                    assert false : "profile field not handled";
                    break;
            }
            if (!TextUtils.isEmpty(line)) {
                descriptionLines.add(line);
            }
        }
        return TextUtils.join("\n", descriptionLines);
    }
}