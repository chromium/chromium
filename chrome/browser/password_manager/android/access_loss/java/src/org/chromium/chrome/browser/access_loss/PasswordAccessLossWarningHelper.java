// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.PasswordAccessLossWarningUserAction.DISMISS;
import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.PasswordAccessLossWarningUserAction.HELP_CENTER;
import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.PasswordAccessLossWarningUserAction.MAIN_ACTION;
import static org.chromium.chrome.browser.access_loss.AccessLossWarningMetricsRecorder.logAccessLossWarningSheetUserAction;
import static org.chromium.chrome.browser.access_loss.HelpUrlLauncher.GOOGLE_PLAY_SUPPORTED_DEVICES_SUPPORT_URL;
import static org.chromium.chrome.browser.access_loss.HelpUrlLauncher.KEEP_APPS_AND_DEVICES_WORKING_WITH_GMS_CORE_SUPPORT_URL;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.ALL_KEYS;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.BUTTON_ACTION;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.BUTTON_TITLE;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TEXT;
import static org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetProperties.SHEET_TITLE;

import android.app.Activity;
import android.text.SpannableString;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.bottom_sheet.SimpleNoticeSheetCoordinator;
import org.chromium.chrome.browser.password_manager.CustomTabIntentHelper;
import org.chromium.chrome.browser.password_manager.GmsUpdateLauncher;
import org.chromium.chrome.browser.password_manager.PasswordExportLauncher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Entry-point to the access loss warning UI surface. */
public class PasswordAccessLossWarningHelper {
    final Activity mActivity;
    final BottomSheetController mBottomSheetController;
    final Profile mProfile;
    final HelpUrlLauncher mHelpUrlLauncher;

    public static final String PASSWORD_ACCESS_LOSS_WARNING_USER_ACTION_PREFIX =
            "PasswordManager.PasswordAccessLossWarningSheet.";
    public static final String PASSWORD_ACCESS_LOSS_WARNING_USER_ACTION_SUFFIX = ".UserAction";

    public PasswordAccessLossWarningHelper(
            Activity activity,
            BottomSheetController bottomSheetController,
            Profile profile,
            CustomTabIntentHelper customTabIntentHelper) {
        mBottomSheetController = bottomSheetController;
        mProfile = profile;
        mActivity = activity;
        mHelpUrlLauncher = new HelpUrlLauncher(customTabIntentHelper);
    }

    public void show(@PasswordAccessLossWarningType int warningType) {
        mBottomSheetController.addObserver(
                createBottomSheetObserver(mBottomSheetController, warningType));
        SimpleNoticeSheetCoordinator coordinator =
                new SimpleNoticeSheetCoordinator(mActivity, mBottomSheetController);
        PropertyModel model = getModelForWarningType(warningType);
        if (model == null) {
            return;
        }
        coordinator.showSheet(model);
    }

    /** Creates the model that has the text and functionality appropriate for the warning type. */
    @Nullable
    PropertyModel getModelForWarningType(@PasswordAccessLossWarningType int warningType) {
        switch (warningType) {
            case PasswordAccessLossWarningType.NO_GMS_CORE:
                return buildAccessLossWarningNoGms();
            case PasswordAccessLossWarningType.NO_UPM:
                // Fallthrough, same as ONLY_ACCOUNT_UPM.
            case PasswordAccessLossWarningType.ONLY_ACCOUNT_UPM:
                return buildAccessLossWarningAboutGmsUpdate(warningType);
            case PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED:
                return buildAccessLossWarningAboutManualMigration();
        }
        assert false : "Unhandled warning type: " + warningType;
        return null;
    }

    /** GMS Core doesn't exist on the device so the user is asked to export their passwords. */
    PropertyModel buildAccessLossWarningNoGms() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(
                        SHEET_TITLE,
                        mActivity.getString(R.string.pwd_access_loss_warning_no_gms_core_title))
                .with(
                        SHEET_TEXT,
                        getBottomSheetTextWithLink(
                                mActivity.getString(
                                        R.string.pwd_access_loss_warning_no_gms_core_text),
                                (unusedView) -> {
                                    mHelpUrlLauncher.showHelpArticle(
                                            mActivity, GOOGLE_PLAY_SUPPORTED_DEVICES_SUPPORT_URL);
                                    logAccessLossWarningSheetUserAction(
                                            PasswordAccessLossWarningType.NO_GMS_CORE, HELP_CENTER);
                                }))
                .with(
                        BUTTON_TITLE,
                        mActivity.getString(
                                R.string.pwd_access_loss_warning_no_gms_core_button_text))
                .with(
                        BUTTON_ACTION,
                        () -> {
                            PasswordExportLauncher.showMainSettingsAndStartExport(mActivity);
                            logAccessLossWarningSheetUserAction(
                                    PasswordAccessLossWarningType.NO_GMS_CORE, MAIN_ACTION);
                        })
                .build();
    }

    /**
     * GMS Core on the device doesn't support UPM so the user is asked to update GMS Core.
     *
     * @param warningType that is used to track user action metrics.
     * @return the property model for the warning sheet.
     */
    PropertyModel buildAccessLossWarningAboutGmsUpdate(
            @PasswordAccessLossWarningType int warningType) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(
                        SHEET_TITLE,
                        mActivity.getString(R.string.pwd_access_loss_warning_update_gms_core_title))
                .with(
                        SHEET_TEXT,
                        getBottomSheetTextWithLink(
                                mActivity.getString(
                                        R.string.pwd_access_loss_warning_update_gms_core_text),
                                (unusedView) -> {
                                    mHelpUrlLauncher.showHelpArticle(
                                            mActivity,
                                            KEEP_APPS_AND_DEVICES_WORKING_WITH_GMS_CORE_SUPPORT_URL);
                                    logAccessLossWarningSheetUserAction(warningType, HELP_CENTER);
                                }))
                .with(
                        BUTTON_TITLE,
                        mActivity.getString(
                                R.string.pwd_access_loss_warning_update_gms_core_button_text))
                .with(
                        BUTTON_ACTION,
                        () -> {
                            GmsUpdateLauncher.launch(mActivity);
                            logAccessLossWarningSheetUserAction(warningType, MAIN_ACTION);
                        })
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
                        mActivity.getString(
                                R.string.pwd_access_loss_warning_manual_migration_title))
                .with(
                        SHEET_TEXT,
                        SpannableString.valueOf(
                                mActivity.getString(
                                        R.string.pwd_access_loss_warning_manual_migration_text)))
                .with(
                        BUTTON_TITLE,
                        mActivity.getString(
                                R.string.pwd_access_loss_warning_manual_migration_button_text))
                .with(
                        BUTTON_ACTION,
                        () -> {
                            PasswordExportLauncher.showMainSettingsAndStartExport(mActivity);
                            logAccessLossWarningSheetUserAction(
                                    PasswordAccessLossWarningType.NEW_GMS_CORE_MIGRATION_FAILED,
                                    MAIN_ACTION);
                        })
                .build();
    }

    private SpannableString getBottomSheetTextWithLink(String sheetText, Callback<View> callback) {
        return SpanApplier.applySpans(
                sheetText,
                new SpanApplier.SpanInfo(
                        "<link>", "</link>", new NoUnderlineClickableSpan(mActivity, callback)));
    }

    private BottomSheetObserver createBottomSheetObserver(
            BottomSheetController bottomSheetController,
            @PasswordAccessLossWarningType int warningType) {
        return new EmptyBottomSheetObserver() {
            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                if (reason == StateChangeReason.SWIPE
                        || reason == StateChangeReason.BACK_PRESS
                        || reason == StateChangeReason.TAP_SCRIM
                        || reason == StateChangeReason.OMNIBOX_FOCUS) {
                    logAccessLossWarningSheetUserAction(warningType, DISMISS);
                }
                bottomSheetController.removeObserver(this);
            }
        };
    }
}
