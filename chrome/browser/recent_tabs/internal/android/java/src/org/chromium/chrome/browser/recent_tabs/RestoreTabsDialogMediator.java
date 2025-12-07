// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import android.content.Context;
import android.view.View;

import androidx.activity.OnBackPressedCallback;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsMetricsHelper.RestoreTabsOnFRERestoredTabsResult;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsMetricsHelper.RestoreTabsOnFREResultAction;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Contains the logic to set the state of the model and react to events like clicks. */
@NullMarked
public class RestoreTabsDialogMediator extends RestoreTabsMediator {
    private Context mContext;
    private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private PropertyModel mHostDialogModel;
    private RestoreTabsBackPressHandler mBackPressHandler;

    /**
     * Initialize mediator with required dependencies.
     *
     * @param model A {@link PropertyModel} that holds the {@RestoreTabsProperties}.
     * @param profile The {@link Profile} for the current user.
     * @param tabCreatorManager A {@link TabCreatorManager} instance to restore tabs.
     * @param modalDialogManagerSupplier A supplier for the {@link ModalDialogManager}.
     */
    @Initializer
    public void initialize(
            PropertyModel model,
            Profile profile,
            TabCreatorManager tabCreatorManager,
            Context context,
            Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        super.initialize(model, profile, tabCreatorManager, null);

        mContext = context;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mBackPressHandler = new RestoreTabsBackPressHandler(model);

        ModalDialogProperties.Controller dialogController =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {}

                    @Override
                    public void onDismiss(
                            PropertyModel model, @DialogDismissalCause int dismissalCause) {
                        dismiss();

                        switch (dismissalCause) {
                            case DialogDismissalCause.TOUCH_OUTSIDE:
                                RestoreTabsMetricsHelper.recordResultActionHistogram(
                                        RestoreTabsOnFREResultAction.DISMISSED_SCRIM);
                                RestoreTabsMetricsHelper.recordResultActionMetrics(
                                        RestoreTabsOnFREResultAction.DISMISSED_SCRIM);
                                RestoreTabsMetricsHelper.recordRestoredTabsResultHistogram(
                                        RestoreTabsOnFRERestoredTabsResult.NONE);
                                break;
                        }
                    }
                };

        OnBackPressedCallback onBackPressedCallback =
                new OnBackPressedCallback(true) {
                    @Override
                    public void handleOnBackPressed() {
                        mBackPressHandler.backPressOnCurrentScreen();
                    }
                };

        mHostDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.CONTENT_DESCRIPTION,
                                mContext.getString(R.string.restore_tabs_content_description))
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.DISABLE_SCRIM, true)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(
                                ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                                onBackPressedCallback)
                        .build();
    }

    /**
     * If set to true, requests to show the bottom sheet. Otherwise, requests to hide the sheet.
     *
     * @param isVisible A boolean indicating whether to show or hide the sheet.
     * @param content The bottom sheet content to show/hide.
     * @return True if the request was successful, false otherwise.
     */
    public boolean setVisible(boolean isVisible, View content) {
        ModalDialogManager dialogManager = mModalDialogManagerSupplier.get();
        if (dialogManager == null) return false;

        if (isVisible) {
            mHostDialogModel.set(ModalDialogProperties.CUSTOM_VIEW, content);
            dialogManager.showDialog(mHostDialogModel, ModalDialogManager.ModalDialogType.APP);
        } else {
            dialogManager.dismissDialog(
                    mHostDialogModel, DialogDismissalCause.ACTION_ON_DIALOG_COMPLETED);
        }
        return true;
    }

    PropertyModel getHostDialogModelForTesting() {
        return mHostDialogModel;
    }
}
