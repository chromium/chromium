// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.content.Context;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.browsing_data.TimePeriodUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** A controller responsible for setting up quick delete MVC. */
public class QuickDeleteController {

    private final @NonNull Context mContext;
    private final @NonNull QuickDeleteDelegate mDelegate;
    private final @NonNull QuickDeleteTabsFilter mDeleteTabsFilter;
    private final @NonNull SnackbarManager mSnackbarManager;
    private final @NonNull LayoutManager mLayoutManager;
    private final QuickDeleteBridge mQuickDeleteBridge;
    private final QuickDeleteMediator mQuickDeleteMediator;
    private final PropertyModel mPropertyModel;
    private final PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    /**
     * Constructor for the QuickDeleteController with a dialog and confirmation snackbar.
     *
     * @param context            The associated {@link Context}.
     * @param delegate           A {@link QuickDeleteDelegate} to perform the quick delete.
     * @param modalDialogManager A {@link ModalDialogManager} to show the quick delete modal dialog.
     * @param snackbarManager    A {@link SnackbarManager} to show the quick delete snackbar.
     * @param layoutManager      {@link LayoutManager} to use for showing the regular overview mode.
     * @param tabModelSelector   {@link TabModelSelector} to use for opening the links in search
     *                           history disambiguation notice.
     */
    public QuickDeleteController(
            @NonNull Context context,
            @NonNull QuickDeleteDelegate delegate,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull SnackbarManager snackbarManager,
            @NonNull LayoutManager layoutManager,
            @NonNull TabModelSelector tabModelSelector) {
        mContext = context;
        mDelegate = delegate;
        mSnackbarManager = snackbarManager;
        mLayoutManager = layoutManager;

        mDeleteTabsFilter =
                new QuickDeleteTabsFilter(tabModelSelector.getModel(/* incognito= */ false));
        Profile profile = tabModelSelector.getCurrentModel().getProfile();
        mQuickDeleteBridge = new QuickDeleteBridge(profile);

        // MVC setup.
        View quickDeleteView =
                LayoutInflater.from(context).inflate(R.layout.quick_delete_dialog, null);
        mPropertyModel =
                new PropertyModel.Builder(QuickDeleteProperties.ALL_KEYS)
                        .with(QuickDeleteProperties.CONTEXT, mContext)
                        .build();
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel, quickDeleteView, QuickDeleteViewBinder::bind);
        mQuickDeleteMediator =
                new QuickDeleteMediator(
                        mPropertyModel, profile, mQuickDeleteBridge, mDeleteTabsFilter);

        QuickDeleteDialogDelegate dialogDelegate =
                new QuickDeleteDialogDelegate(
                        context,
                        quickDeleteView,
                        modalDialogManager,
                        this::onDialogDismissed,
                        tabModelSelector,
                        mDelegate.getSettingsLauncher(),
                        mQuickDeleteMediator);
        dialogDelegate.showDialog();
    }

    void destroy() {
        mPropertyModelChangeProcessor.destroy();
    }

    /**
     * @return True, if quick delete feature flag is enabled, false otherwise
     */
    public static boolean isQuickDeleteEnabled() {
        return ChromeFeatureList.sQuickDeleteForAndroid.isEnabled();
    }

    /** A method called when the user confirms or cancels the dialog. */
    private void onDialogDismissed(@DialogDismissalCause int dismissalCause) {
        switch (dismissalCause) {
            case DialogDismissalCause.POSITIVE_BUTTON_CLICKED:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.DELETE_CLICKED);
                @TimePeriod int timePeriod = mPropertyModel.get(QuickDeleteProperties.TIME_PERIOD);
                mDeleteTabsFilter.closeTabsFilteredForQuickDelete(timePeriod);
                mDelegate.performQuickDelete(() -> onQuickDeleteFinished(timePeriod), timePeriod);
                break;
            case DialogDismissalCause.NEGATIVE_BUTTON_CLICKED:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.CANCEL_CLICKED);
                break;
            default:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.DIALOG_DISMISSED_IMPLICITLY);
                break;
        }
        destroy();
    }

    private void onQuickDeleteFinished(@TimePeriod int timePeriod) {
        navigateToTabSwitcher();
        triggerHapticFeedback();
        showSnackbar(timePeriod);
    }

    /** A method to navigate to tab switcher. */
    private void navigateToTabSwitcher() {
        if (mLayoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)) return;
        mLayoutManager.showLayout(LayoutType.TAB_SWITCHER, /* animate= */ true);
    }

    private void triggerHapticFeedback() {
        Vibrator v = (Vibrator) mContext.getSystemService(Context.VIBRATOR_SERVICE);
        final long duration = 50;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            v.vibrate(VibrationEffect.createOneShot(duration, VibrationEffect.DEFAULT_AMPLITUDE));
        } else {
            // Deprecated in API 26.
            v.vibrate(duration);
        }
    }

    /** A method to show the quick delete snack-bar. */
    private void showSnackbar(@TimePeriod int timePeriod) {
        String snackbarMessage;
        if (timePeriod == TimePeriod.ALL_TIME) {
            snackbarMessage = mContext.getString(R.string.quick_delete_snackbar_all_time_message);
        } else {
            snackbarMessage =
                    mContext.getString(
                            R.string.quick_delete_snackbar_message,
                            TimePeriodUtils.getTimePeriodString(mContext, timePeriod));
        }
        Snackbar snackbar =
                Snackbar.make(
                        snackbarMessage,
                        /* controller= */ null,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_QUICK_DELETE);
        mSnackbarManager.showSnackbar(snackbar);
    }
}
