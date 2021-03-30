// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.onboarding;

import android.content.Context;
import android.content.DialogInterface.OnDismissListener;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.LinearLayout;
import android.widget.ScrollView;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.Callback;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.LayoutUtils;

import java.util.Map;

/**
 * Coordinator responsible for showing the dialog onboarding screen when the user is using the
 * Autofill Assistant for the first time.
 */
class DialogOnboardingCoordinator extends BaseOnboardingCoordinator {
    @Nullable
    AlertDialog mDialog;

    DialogOnboardingCoordinator(
            String experimentIds, Map<String, String> parameters, Context context) {
        super(experimentIds, parameters, context);
    }

    @Override
    ScrollView createViewImpl() {
        ScrollView baseView = (ScrollView) LayoutUtils.createInflater(mContext).inflate(
                R.layout.autofill_assistant_base_onboarding, /* root= */ null);
        ViewGroup onboardingContentContainer =
                baseView.findViewById(R.id.onboarding_layout_container);

        LinearLayout buttonsLayout = new LinearLayout(mContext);
        buttonsLayout.setLayoutParams(new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        buttonsLayout.setGravity(Gravity.BOTTOM | Gravity.END);
        buttonsLayout.setOrientation(LinearLayout.HORIZONTAL);
        LayoutUtils.createInflater(mContext).inflate(
                R.layout.autofill_assistant_onboarding_no_button, /* root= */ buttonsLayout);
        LayoutUtils.createInflater(mContext).inflate(
                R.layout.autofill_assistant_onboarding_yes_button, /* root= */ buttonsLayout);

        onboardingContentContainer.addView(buttonsLayout);
        return baseView;
    }

    @Override
    void initViewImpl(Callback<Integer> callback) {
        mDialog = new AlertDialog
                          .Builder(getContext(),
                                  org.chromium.chrome.autofill_assistant.R.style
                                          .Theme_Chromium_AlertDialog)
                          .create();

        mDialog.setOnDismissListener((OnDismissListener) dialog
                -> onUserAction(
                        /* result= */ AssistantOnboardingResult.DISMISSED, callback));
        mDialog.setView(mView);
    }

    @Override
    void showViewImpl() {
        mDialog.show();
    }

    @Override
    public void hide() {
        if (mDialog != null) {
            mDialog.cancel();
            mDialog = null;
        }
        destroy();
    }

    @Override
    public boolean isInProgress() {
        return mDialog != null;
    }
}
