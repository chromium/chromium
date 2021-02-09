// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantTextUtils;
import org.chromium.chrome.browser.autofill_assistant.LayoutUtils;

/**
 * A coordinator responsible for showing a form to the user.
 *
 * See http://go/aa-form-action for more information.
 */
public class AssistantFormCoordinator {
    private final AssistantFormModel mModel;
    private final LinearLayout mRootView;
    private final LinearLayout mFormView;
    private final LinearLayout mInfoView;

    public AssistantFormCoordinator(Context context, AssistantFormModel model) {
        mModel = model;

        mRootView = makeLinearLayout(context);

        mFormView = makeLinearLayout(context);

        mInfoView = (LinearLayout) LayoutUtils.createInflater(context).inflate(
                R.layout.autofill_assistant_form_information, mRootView,
                /* attachToRoot= */ false);

        mRootView.addView(mFormView);
        mRootView.addView(mInfoView);

        updateVisibility();
        mModel.addObserver((source, propertyKey) -> {
            if (AssistantFormModel.INPUTS == propertyKey) {
                // TODO(b/144690738) This creates a new instance of the UI on every notification...
                clearLinearLayout(mFormView);

                for (AssistantFormInput input : model.get(AssistantFormModel.INPUTS)) {
                    View view = input.createView(context, mFormView);
                    mFormView.addView(view);
                }
                updateVisibility();
            } else if (AssistantFormModel.INFO_LABEL == propertyKey) {
                if (mModel.get(AssistantFormModel.INFO_LABEL) == null) {
                    mInfoView.setVisibility(View.GONE);
                } else {
                    mInfoView.setVisibility(View.VISIBLE);
                    TextView label = mInfoView.findViewById(R.id.text);
                    AssistantTextUtils.applyVisualAppearanceTags(
                            label, mModel.get(AssistantFormModel.INFO_LABEL), null);
                }
            } else if (AssistantFormModel.INFO_POPUP == propertyKey) {
                View infoButton = mInfoView.findViewById(R.id.info_button);
                if (mModel.get(AssistantFormModel.INFO_POPUP) == null) {
                    infoButton.setVisibility(View.GONE);
                } else {
                    infoButton.setVisibility(View.VISIBLE);
                    infoButton.setOnClickListener(
                            unusedView -> mModel.get(AssistantFormModel.INFO_POPUP).show(context));
                }
            }
        });
    }

    /** Return the view associated to this coordinator. */
    public View getView() {
        return mRootView;
    }

    private void updateVisibility() {
        int rootVisibility = mModel.get(AssistantFormModel.INPUTS) != null
                        && mModel.get(AssistantFormModel.INPUTS).size() > 0
                ? View.VISIBLE
                : View.GONE;
        mRootView.setVisibility(rootVisibility);
    }

    private LinearLayout makeLinearLayout(Context context) {
        LinearLayout view = new LinearLayout(context);
        view.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        view.setOrientation(LinearLayout.VERTICAL);
        return view;
    }

    private void clearLinearLayout(LinearLayout view) {
        for (int i = 0; i < view.getChildCount(); i++) {
            view.getChildAt(i).setVisibility(View.GONE);
        }
    }
}
