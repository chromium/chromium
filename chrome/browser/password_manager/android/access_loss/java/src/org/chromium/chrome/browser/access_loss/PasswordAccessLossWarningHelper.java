// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.ALL_KEYS;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.BUTTON_TITLE;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TEXT;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TITLE;

import android.app.Activity;
import android.content.Context;
import android.text.SpannableString;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetCoordinator;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

class PasswordAccessLossWarningHelper {
    final Context mContext;
    final BottomSheetController mBottomSheetController;
    final Profile mProfile;
    final Activity mActivity;
    static final String KEEP_APPS_AND_DEVICES_WORKING_WITH_GMS_CORE_SUPPORT_URL =
            "https://support.google.com/googleplay/answer/9037938";
    static final String GOOGLE_PLAY_SUPPORTED_DEVICES_SUPPORT_URL =
            "https://support.google.com/googleplay/answer/1727131";

    public PasswordAccessLossWarningHelper(
            Context context,
            BottomSheetController bottomSheetController,
            Profile profile,
            Activity activity) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mProfile = profile;
        mActivity = activity;
    }

    public void show(@PasswordAccessLossWarningType int warningType) {
        SimpleNoticeSheetCoordinator coordinator =
                new SimpleNoticeSheetCoordinator(mContext, mBottomSheetController);
        PropertyModel model = getModelForWarningType(warningType);
        if (model == null) {
            return;
        }
        coordinator.showSheet(model);
    }

    @Nullable
    /** Creates the model that has the text and functionality appropriate for the warning type. */
    PropertyModel getModelForWarningType(@PasswordAccessLossWarningType int warningType) {
        switch (warningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
                return buildAccessLossWarningNoGms();
            case PasswordAccessLossWarningType.NO_UPM:
                // Fallthrough, same as ONLY_ACCOUNT_UPM.
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
                return buildAccessLossWarningAboutGmsUpdate();
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                return buildAccessLossWarningAboutManualMigration();
            default:
                assert false : "Unhandled warning type.";
                return null;
        }
    }

    /** GMS Core doesn't exist on the device so the user is asked to export their passwords. */
    PropertyModel buildAccessLossWarningNoGms() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(
                        SHEET_TITLE,
                        mContext.getString(R.string.pwd_access_loss_warning_no_gms_core_title))
                .with(
                        SHEET_TEXT,
                        getBottomSheetTextWithLink(
                                mContext.getString(
                                        R.string.pwd_access_loss_warning_no_gms_core_text),
                                this::openGooglePlaySupportedDevicesHelpPage))
                .with(
                        BUTTON_TITLE,
                        mContext.getString(
                                R.string.pwd_access_loss_warning_no_gms_core_button_text))
                .build();
    }

    /** GMS Core on the device doesn't support UPM so the user is asked to update GMS Core. */
    PropertyModel buildAccessLossWarningAboutGmsUpdate() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(
                        SHEET_TITLE,
                        mContext.getString(R.string.pwd_access_loss_warning_update_gms_core_title))
                .with(
                        SHEET_TEXT,
                        getBottomSheetTextWithLink(
                                mContext.getString(
                                        R.string.pwd_access_loss_warning_update_gms_core_text),
                                this::openGmsCoreHelpPage))
                .with(
                        BUTTON_TITLE,
                        mContext.getString(
                                R.string.pwd_access_loss_warning_update_gms_core_button_text))
                .build();
    }

    /**
     * GMS Core version on the device is new enough for UPM, but the automatic migration failed, so
     * the user is asked to manually do the migration by performing export and import.
     */
    PropertyModel buildAccessLossWarningAboutManualMigration() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(
                        SHEET_TITLE,
                        mContext.getString(R.string.pwd_access_loss_warning_manual_migration_title))
                .with(
                        SHEET_TEXT,
                        SpannableString.valueOf(
                                mContext.getString(
                                        R.string.pwd_access_loss_warning_manual_migration_text)))
                .with(
                        BUTTON_TITLE,
                        mContext.getString(
                                R.string.pwd_access_loss_warning_manual_migration_button_text))
                .build();
    }

    private SpannableString getBottomSheetTextWithLink(String sheetText, Callback<View> callback) {
        return SpanApplier.applySpans(
                sheetText,
                new SpanApplier.SpanInfo(
                        "<link>", "</link>", new NoUnderlineClickableSpan(mContext, callback)));
    }

    private void openGooglePlaySupportedDevicesHelpPage(View view) {
        HelpAndFeedbackLauncherFactory.getForProfile(mProfile)
                .show(mActivity, GOOGLE_PLAY_SUPPORTED_DEVICES_SUPPORT_URL, null);
    }

    private void openGmsCoreHelpPage(View view) {
        HelpAndFeedbackLauncherFactory.getForProfile(mProfile)
                .show(mActivity, KEEP_APPS_AND_DEVICES_WORKING_WITH_GMS_CORE_SUPPORT_URL, null);
    }
}
