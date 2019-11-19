// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.form;

import android.content.Context;
import android.support.v7.content.res.AppCompatResources;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AbstractListObserver;

/**
 * A coordinator responsible for showing a form to the user.
 *
 * See http://go/aa-form-action for more information.
 */
public class AssistantFormCoordinator {
    private final AssistantFormModel mModel;
    private final LinearLayout mView;

    public AssistantFormCoordinator(Context context, AssistantFormModel model) {
        mModel = model;
        mView = new LinearLayout(context);
        mView.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mView.setOrientation(LinearLayout.VERTICAL);
        mView.setDividerDrawable(AppCompatResources.getDrawable(
                context, R.drawable.autofill_assistant_form_input_divider));
        mView.setShowDividers(LinearLayout.SHOW_DIVIDER_MIDDLE);

        updateVisibility();
        mModel.getInputsModel().addObserver(new AbstractListObserver<Void>() {
            @Override
            public void onDataSetChanged() {
                for (int i = 0; i < mView.getChildCount(); i++) {
                    mView.getChildAt(i).setVisibility(View.GONE);
                }

                for (AssistantFormInput input : mModel.getInputsModel()) {
                    View view = input.createView(context, mView);
                    mView.addView(view);
                }
                updateVisibility();
            }
        });
    }

    /** Return the view associated to this coordinator. */
    public View getView() {
        return mView;
    }

    private void updateVisibility() {
        int visibility = mModel.getInputsModel().size() > 0 ? View.VISIBLE : View.GONE;
        mView.setVisibility(visibility);
    }
}
