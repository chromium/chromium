// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.device_reauth;

import static org.chromium.chrome.browser.device_reauth.BiometricErrorDialogProperties.DESCRIPTION;
import static org.chromium.chrome.browser.device_reauth.BiometricErrorDialogProperties.MORE_DETAILS;
import static org.chromium.chrome.browser.device_reauth.BiometricErrorDialogProperties.TITLE;

import android.app.Activity;
import android.content.res.Resources;
import android.text.SpannableString;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * Shows a modal dialog describing the error encountered during mandatory biometric auth and steps
 * to solve it.
 */
public class BiometricErrorDialogController {
    private final Activity mActivity;
    private final ModalDialogManager mModalDialogManager;

    BiometricErrorDialogController(Activity activity, ModalDialogManager modalDialogManager) {
        mActivity = activity;
        mModalDialogManager = modalDialogManager;
    }

    void showLockoutErrorDialog() {
        View dialogCustomView =
                LayoutInflater.from(mActivity).inflate(R.layout.biometric_auth_error_dialog, null);
        Resources resources = mActivity.getResources();
        // TODO(crbug.com/3367923668): Add icon at the top of the title. This is the only reason to
        // use a custom view.
        SpannableString moreDetails =
                getLockoutErrorDetails(
                        resources.getString(R.string.identity_check_lockout_error_more_details));
        PropertyModel customViewModel =
                new PropertyModel.Builder(BiometricErrorDialogProperties.ALL_KEYS)
                        .with(
                                TITLE,
                                resources.getString(R.string.identity_check_lockout_error_title))
                        .with(
                                DESCRIPTION,
                                resources.getString(
                                        R.string.identity_check_lockout_error_description))
                        .with(MORE_DETAILS, moreDetails)
                        .build();

        // TODO(crbug.com/367922864): Add controller that handles the click on the "Lock screen"
        // button.
        PropertyModelChangeProcessor.create(
                customViewModel, dialogCustomView, BiometricErrorDialogViewBinder::bind);
        PropertyModel modalDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                new SimpleModalDialogController(
                                        mModalDialogManager, (Integer dismissalCause) -> {}))
                        .with(ModalDialogProperties.CUSTOM_VIEW, dialogCustomView)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.lock_screen)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.cancel)
                        .build();

        mModalDialogManager.showDialog(modalDialogModel, ModalDialogType.TAB);
    }

    private SpannableString getLockoutErrorDetails(String text) {
        // TODO(crbug.com/367922864): Link this to the Identity Check settings.
        return SpanApplier.applySpans(
                text,
                new SpanApplier.SpanInfo(
                        "<link>",
                        "</link>",
                        new NoUnderlineClickableSpan(mActivity, (View unused) -> {})));
    }
}
