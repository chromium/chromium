// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.ALL_KEYS;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.BUTTON_TITLE;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TEXT;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TITLE;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetCoordinator;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

class PasswordAccessLossWarningHelper {

    final Context mContext;
    final BottomSheetController mBottomSheetController;

    public PasswordAccessLossWarningHelper(
            Context context, BottomSheetController bottomSheetController) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
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
    /**
     * Creates the model that has the text and functionality appropriate for the warning type.
     *
     * @param warningType is the type of warning to be displayed.
     */
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
                                        R.string.pwd_access_loss_warning_no_gms_core_text)))
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
                                        R.string.pwd_access_loss_warning_update_gms_core_text)))
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
                        mContext.getString(R.string.pwd_access_loss_warning_manual_migration_text))
                .with(
                        BUTTON_TITLE,
                        mContext.getString(
                                R.string.pwd_access_loss_warning_manual_migration_button_text))
                .build();
    }

    private String getBottomSheetTextWithLink(String sheetText) {
        sheetText =
                SpanApplier.applySpans(
                                sheetText,
                                new SpanApplier.SpanInfo(
                                        "<link>",
                                        "</link>",
                                        new NoUnderlineClickableSpan(
                                                mContext, this::onLearnMoreClicked)))
                        .toString();
        return sheetText;
    }

    private void onLearnMoreClicked(View view) {
        // TODO: crbug.com/360346943 - Open the help centre article.
    }
}
