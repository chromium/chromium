// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.download.DownloadDialogBridge;
import org.chromium.chrome.browser.download.DownloadLaterMetrics;
import org.chromium.chrome.browser.download.DownloadLaterMetrics.DownloadLaterUiEvent;
import org.chromium.components.offline_items_collection.OfflineItemSchedule;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A wrapper of {@link DownloadLaterDialogCoordinator}, always use the same Android {@link Context}
 * and relevant dependencies, and supports to show dialog based on a {@link
 * org.chromium.components.offline_items_collection.OfflineItem}.
 */
public class DownloadLaterDialogHelper implements DownloadLaterDialogController {
    /**
     * Defines the caller of {@link DownloadLaterDialogHelper}. Used in metrics recording.
     */
    @IntDef({Source.DOWNLOAD_HOME, Source.DOWNLOAD_INFOBAR})
    public @interface Source {
        int DOWNLOAD_HOME = 0;
        int DOWNLOAD_INFOBAR = 1;
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final PrefService mPrefService;
    private final DownloadLaterDialogCoordinator mDownloadLaterDialog;

    private Callback<OfflineItemSchedule> mCallback;
    private @Source int mSource;

    /**
     * Creates the download later dialog helper.
     * @param context The {@link Context} associated with the dialog.
     * @param manager The {@link ModalDialogManager} to show the modal dialog.
     * @param prefService Used to update user preferences.
     * @return The download later dialog helper.
     */
    public static DownloadLaterDialogHelper create(
            Context context, ModalDialogManager manager, PrefService prefService) {
        DownloadDateTimePickerDialog dateTimePicker = new DownloadDateTimePickerDialogImpl();
        DownloadLaterDialogCoordinator dialog = new DownloadLaterDialogCoordinator(dateTimePicker);
        dateTimePicker.initialize(dialog);
        return new DownloadLaterDialogHelper(context, manager, prefService, dialog);
    }

    DownloadLaterDialogHelper(Context context, ModalDialogManager manager, PrefService prefService,
            DownloadLaterDialogCoordinator downloadLaterDialog) {
        mContext = context;
        mModalDialogManager = manager;
        mPrefService = prefService;
        mDownloadLaterDialog = downloadLaterDialog;
        mDownloadLaterDialog.initialize(this);
    }

    /**
     * Shows a download later dialog when the user wants to change the {@link OfflineItemSchedule}.
     * @param currentSchedule The current {@link OfflineItemSchedule}.
     * @param source The caller of this function, used to collect metrics.
     * @param callback The callback to reply the new schedule selected by the user. May reply null
     *                 if the user cancels the dialog.
     */
    public void showChangeScheduleDialog(@NonNull final OfflineItemSchedule currentSchedule,
            @Source int source, Callback<OfflineItemSchedule> callback) {
        @DownloadLaterDialogChoice
        int initialChoice = DownloadLaterDialogChoice.DOWNLOAD_NOW;
        initialChoice = currentSchedule.onlyOnWifi ? DownloadLaterDialogChoice.ON_WIFI
                                                   : DownloadLaterDialogChoice.DOWNLOAD_LATER;

        mCallback = callback;
        mSource = source;
        boolean shouldShowDateTimePicker = DownloadDialogBridge.shouldShowDateTimePicker();
        if (!shouldShowDateTimePicker
                && initialChoice == DownloadLaterDialogChoice.DOWNLOAD_LATER) {
            initialChoice = DownloadLaterDialogChoice.DOWNLOAD_NOW;
        }
        PropertyModel.Builder builder =
                new PropertyModel.Builder(DownloadLaterDialogProperties.ALL_KEYS)
                        .with(DownloadLaterDialogProperties.CONTROLLER, mDownloadLaterDialog)
                        .with(DownloadLaterDialogProperties.INITIAL_CHOICE, initialChoice)
                        .with(DownloadLaterDialogProperties.SHOW_DATE_TIME_PICKER_OPTION,
                                shouldShowDateTimePicker);

        // Set the previously selected time to the date time picker UI.
        if (currentSchedule.startTimeMs > 0) {
            builder.with(DownloadDateTimePickerDialogProperties.INITIAL_TIME,
                    currentSchedule.startTimeMs);
        }

        mDownloadLaterDialog.showDialog(
                mContext, mModalDialogManager, mPrefService, builder.build());
    }

    /**
     * Destroys the download later dialog.
     */
    public void destroy() {
        mDownloadLaterDialog.destroy();
    }

    // DownloadLaterDialogController implementation.
    @Override
    public void onDownloadLaterDialogComplete(int choice, long startTime) {
        if (mCallback == null) return;

        recordDialogMetrics(true /*complete*/, choice);
        OfflineItemSchedule schedule =
                new OfflineItemSchedule(choice == DownloadLaterDialogChoice.ON_WIFI, startTime);
        mCallback.onResult(schedule);
        mCallback = null;
    }

    @Override
    public void onDownloadLaterDialogCanceled() {
        if (mCallback == null) return;

        recordDialogMetrics(false /*complete*/, DownloadLaterDialogChoice.CANCELLED);
        mCallback.onResult(null);
        mCallback = null;
    }

    @Override
    public void onEditLocationClicked() {
        // Do nothing, no edit location text for the change schedule dialog.
    }

    // Collect complete or cancel metrics based on the source of the dialog.
    private void recordDialogMetrics(boolean complete, @DownloadLaterDialogChoice int choice) {
        switch (mSource) {
            case Source.DOWNLOAD_HOME:
                if (complete) {
                    DownloadLaterMetrics.recordDownloadHomeChangeScheduleChoice(choice);
                } else {
                    DownloadLaterMetrics.recordDownloadLaterUiEvent(
                            DownloadLaterUiEvent.DOWNLOAD_HOME_CHANGE_SCHEDULE_CANCEL);
                }
                break;
            case Source.DOWNLOAD_INFOBAR:
                if (complete) {
                    DownloadLaterMetrics.recordInfobarChangeScheduleChoice(choice);
                } else {
                    DownloadLaterMetrics.recordDownloadLaterUiEvent(
                            DownloadLaterUiEvent.DOWNLOAD_INFOBAR_CHANGE_SCHEDULE_CANCEL);
                }
                break;
            default:
                assert false : "Unknown source, can't collect metrics.";
                return;
        }
    }
}
