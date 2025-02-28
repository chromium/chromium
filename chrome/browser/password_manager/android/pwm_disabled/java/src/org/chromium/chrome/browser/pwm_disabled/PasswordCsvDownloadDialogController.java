// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwm_disabled;

import static org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadDialogProperties.CLOSE_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadDialogProperties.DETAILS_PARAGRAPH1;
import static org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadDialogProperties.DETAILS_PARAGRAPH2;
import static org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadDialogProperties.EXPORT_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.pwm_disabled.PasswordCsvDownloadDialogProperties.TITLE;

import android.net.Uri;
import android.text.SpannableString;
import android.view.LayoutInflater;
import android.view.View;

import androidx.fragment.app.FragmentActivity;

import org.chromium.base.Callback;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Controller for the dialog offering the users to download the auto-exported passwords CSV. */
class PasswordCsvDownloadDialogController {
    private final FragmentActivity mActivity;
    private final PasswordCsvDownloadDialogFragment mFragment;
    private final boolean mIsGooglePlayServicesAvailable;

    PasswordCsvDownloadDialogController(
            FragmentActivity activity,
            boolean isGooglePlayServicesAvailable,
            Runnable onExportClicked,
            Runnable onCancel) {
        mActivity = activity;
        mIsGooglePlayServicesAvailable = isGooglePlayServicesAvailable;
        View dialogView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.password_csv_download_dialog_view, null);
        mFragment = new PasswordCsvDownloadDialogFragment();
        initialize(dialogView, onExportClicked, onCancel);
    }

    /** Displays the dialog asking users if they want to export passwords. */
    void showDialog() {
        mFragment.show(mActivity.getSupportFragmentManager(), null);
    }

    /** Dismisses the dialog asking users if they want to export passwords. */
    void dismiss() {
        mFragment.dismiss();
    }

    /**
     * Starts an activity allowing users to select the download location for the exported passwords.
     *
     * @param onDownloadLocationSet called when the URI of the destination file is set.
     */
    void askForDownloadLocation(Callback<Uri> onDownloadLocationSet) {
        mFragment.runCreateFileOnDiskIntent(onDownloadLocationSet);
    }

    private void initialize(View dialogView, Runnable onExportClicked, Runnable onCancel) {
        mFragment.setView(dialogView);
        bindDialogView(dialogView, onExportClicked, onCancel);
    }

    private void bindDialogView(View dialogView, Runnable onExportClicked, Runnable onClose) {
        PropertyModel model =
                new PropertyModel.Builder(PasswordCsvDownloadDialogProperties.ALL_KEYS)
                        .with(TITLE, getDialogTitle())
                        .with(DETAILS_PARAGRAPH1, getFirstDetailsParagraph())
                        .with(DETAILS_PARAGRAPH2, getSecondDetailsParagraph())
                        .with(EXPORT_BUTTON_CALLBACK, onExportClicked)
                        .with(CLOSE_BUTTON_CALLBACK, onClose)
                        .build();

        PropertyModelChangeProcessor.create(
                model, dialogView, PasswordCsvDownloadDialogViewBinder::bind);
    }

    private String getDialogTitle() {
        return mActivity.getString(R.string.keep_access_to_your_passwords_dialog_title);
    }

    private SpannableString getFirstDetailsParagraph() {
        return mIsGooglePlayServicesAvailable
                ? getFirstDetailsParagraphWithLink(
                        mActivity.getString(R.string.csv_download_dialog_with_gms_paragraph1),
                        this::helpLinkClicked)
                : SpannableString.valueOf(
                        mActivity.getString(R.string.csv_download_dialog_no_gms_paragraph1));
    }

    private SpannableString getFirstDetailsParagraphWithLink(
            String text, Callback<View> helpLinkClickedCallback) {
        return SpanApplier.applySpans(
                text,
                new SpanApplier.SpanInfo(
                        "<link>",
                        "</link>",
                        new ChromeClickableSpan(mActivity, helpLinkClickedCallback)));
    }

    private String getSecondDetailsParagraph() {
        return mActivity.getString(R.string.csv_download_dialog_paragraph2);
    }

    private void helpLinkClicked(View unusedView) {
        // TODO(crbug.com/378653384): Open the help article.
    }
}
