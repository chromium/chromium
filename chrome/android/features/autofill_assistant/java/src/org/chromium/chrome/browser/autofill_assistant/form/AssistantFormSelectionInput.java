// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

import android.content.Context;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantChoiceList;

import java.util.List;

/** A form input that allows to choose between multiple choices. */
class AssistantFormSelectionInput extends AssistantFormInput {
    interface Delegate {
        void onChoiceSelectionChanged(int choiceIndex, boolean selected);
    }

    private final String mLabel;
    private final List<AssistantFormSelectionChoice> mChoices;
    private final boolean mAllowMultipleChoices;
    private final Delegate mDelegate;

    public AssistantFormSelectionInput(String label, List<AssistantFormSelectionChoice> choices,
            boolean allowMultipleChoices, Delegate delegate) {
        mLabel = label;
        mChoices = choices;
        mAllowMultipleChoices = allowMultipleChoices;
        mDelegate = delegate;
    }

    @Override
    public View createView(Context context, ViewGroup parent) {
        ViewGroup root = (ViewGroup) LayoutInflater.from(context).inflate(
                R.layout.autofill_assistant_form_selection_input, parent,
                /* attachToRoot= */ false);
        TextView label = root.findViewById(org.chromium.chrome.autofill_assistant.R.id.label);
        if (mLabel.isEmpty()) {
            label.setVisibility(View.GONE);
        } else {
            label.setText(mLabel);
        }

        AssistantChoiceList choiceList = root.findViewById(R.id.choice_list);
        choiceList.setAllowMultipleChoices(mAllowMultipleChoices);
        for (int i = 0; i < mChoices.size(); i++) {
            AssistantFormSelectionChoice choice = mChoices.get(i);

            TextView choiceView = new TextView(context);
            ApiCompatibilityUtils.setTextAppearance(
                    choiceView, R.style.TextAppearance_BlackCaptionDefault);
            choiceView.setGravity(Gravity.CENTER_VERTICAL);
            choiceView.setText(choice.getLabel());

            int index = i; // needed for the lambda.
            choiceList.addItem(choiceView, /* hasEditButton= */ false,
                    (isChecked)
                            -> mDelegate.onChoiceSelectionChanged(index, isChecked),
                    /* itemEditedListener= */ null);

            choiceList.setChecked(choiceView, choice.isInitiallySelected());
        }
        return root;
    }
}
