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
import org.chromium.chrome.browser.autofill_assistant.AssistantAutofillProfile;
import org.chromium.chrome.browser.autofill_assistant.AssistantEditor.AssistantContactEditor;
import org.chromium.chrome.browser.autofill_assistant.AssistantOptionModel.ContactModel;

import java.util.ArrayList;
import java.util.List;

/**
 * The contact details section of the Autofill Assistant payment request.
 */
public class AssistantContactDetailsSection extends AssistantCollectUserDataSection<ContactModel> {
    @Nullable
    private AssistantContactEditor mEditor;
    private boolean mIgnoreProfileChangeNotifications;
    private AssistantCollectUserDataModel.ContactDescriptionOptions mSummaryOptions;
    private AssistantCollectUserDataModel.ContactDescriptionOptions mFullOptions;
    private boolean mRequestReloadOnChange;

    AssistantContactDetailsSection(Context context, ViewGroup parent) {
        super(context, parent, R.layout.autofill_assistant_contact_summary,
                R.layout.autofill_assistant_contact_full,
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_payment_request_title_padding),
                context.getString(R.string.payments_add_contact),
                context.getString(R.string.payments_add_contact));
    }

    public void setEditor(@Nullable AssistantContactEditor editor) {
        mEditor = editor;
        if (mEditor == null) {
            return;
        }

        for (ContactModel item : getItems()) {
            mEditor.addContactInformationForAutocomplete(item.mOption);
        }
    }

    public void setRequestReloadOnChange(boolean requestReloadOnChange) {
        mRequestReloadOnChange = requestReloadOnChange;
    }

    @Override
    protected void createOrEditItem(@Nullable ContactModel oldItem) {
        if (mEditor == null) {
            return;
        }

        Callback<ContactModel> doneCallback = editedItem -> {
            if (mRequestReloadOnChange) {
                notifyDataChanged(editedItem,
                        oldItem == null ? AssistantUserDataEventType.ENTRY_CREATED
                                        : AssistantUserDataEventType.ENTRY_EDITED);
                return;
            }

            mIgnoreProfileChangeNotifications = true;
            addOrUpdateItem(editedItem,
                    /* select= */ true, /* notify= */ true);
            mIgnoreProfileChangeNotifications = false;
        };

        Callback<ContactModel> cancelCallback = ignoredItem -> {};

        mEditor.createOrEditItem(oldItem, doneCallback, cancelCallback);
    }

    @Override
    protected void updateFullView(View fullView, @Nullable ContactModel model) {
        if (model == null || mFullOptions == null) {
            return;
        }

        TextView fullViewText = fullView.findViewById(R.id.contact_full);
        String description = createContactDescription(mFullOptions, model.mOption);
        fullViewText.setText(description);
        hideIfEmpty(fullViewText);

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
    protected void updateSummaryView(View summaryView, @Nullable ContactModel model) {
        if (model == null || mSummaryOptions == null) {
            return;
        }

        TextView contactSummaryView = summaryView.findViewById(R.id.contact_summary);
        String description = createContactDescription(mSummaryOptions, model.mOption);
        contactSummaryView.setText(description);
        hideIfEmpty(contactSummaryView);

        summaryView.findViewById(R.id.incomplete_error)
                .setVisibility(model.mErrors.isEmpty() ? View.GONE : View.VISIBLE);
    }

    @Override
    protected boolean canEditOption(ContactModel model) {
        return model.canEdit();
    }

    @Override
    protected @DrawableRes int getEditButtonDrawable(ContactModel model) {
        return R.drawable.ic_edit_24dp;
    }

    @Override
    protected String getEditButtonContentDescription(ContactModel model) {
        return mContext.getString(R.string.payments_edit_contact_details_label);
    }

    @Override
    protected boolean areEqual(@Nullable ContactModel modelA, @Nullable ContactModel modelB) {
        if (modelA == null || modelB == null) {
            return modelA == modelB;
        }
        return TextUtils.equals(modelA.mOption.getGUID(), modelB.mOption.getGUID());
    }

    /**
     * The Chrome profiles have changed externally. This will rebuild the UI with the new/changed
     * set of contacts derived from the profiles, while keeping the selected item if possible.
     */
    void onContactsChanged(List<ContactModel> contacts) {
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
    protected void addOrUpdateItem(ContactModel model, boolean select, boolean notify) {
        super.addOrUpdateItem(model, select, notify);

        if (mEditor != null) {
            mEditor.addContactInformationForAutocomplete(model.mOption);
        }
    }

    /**
     * Creates a "\n"-separated description of {@code contact} using {@code options}.
     */
    private String createContactDescription(
            AssistantCollectUserDataModel.ContactDescriptionOptions options,
            AssistantAutofillProfile contact) {
        List<String> descriptionLines = new ArrayList<>();
        for (int i = 0;
                i < options.mFields.length && descriptionLines.size() < options.mMaxNumberLines;
                i++) {
            String line = "";
            switch (options.mFields[i]) {
                case AssistantContactField.NAME_FULL:
                    line = contact.getFullName();
                    break;
                case AssistantContactField.EMAIL_ADDRESS:
                    line = contact.getEmailAddress();
                    break;
                case AssistantContactField.PHONE_HOME_WHOLE_NUMBER:
                    line = contact.getPhoneNumber();
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