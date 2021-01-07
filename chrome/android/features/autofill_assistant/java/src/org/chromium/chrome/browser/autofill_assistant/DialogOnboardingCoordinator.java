// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnDismissListener;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;
import org.chromium.ui.UiUtils;

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
    void initViewImpl(Callback<Boolean> callback) {
        mDialog = new UiUtils
                          .CompatibleAlertDialogBuilder(getContext(),
                                  org.chromium.chrome.autofill_assistant.R.style
                                          .Theme_Chromium_AlertDialog)
                          .create();

        mDialog.setOnDismissListener(new OnDismissListener() {
            @Override
            public void onDismiss(DialogInterface dialog) {
                onUserAction(
                        /* accept= */ false, callback, OnBoarding.OB_NO_ANSWER,
                        DropOutReason.ONBOARDING_DIALOG_DISMISSED);
            }
        });

        mDialog.setView(mView);
    }

    @Override
    void showViewImpl() {
        mDialog.show();
    }

    @Override
    void hide() {
        if (mDialog != null) {
            mDialog.cancel();
            mDialog = null;
        }
        destroy();
    }
}