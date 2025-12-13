// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A dialog to confirm if the user wants to turn on caret browsing. This dialog is shown when the
 * user presses F7.
 */
@NullMarked
public class CaretBrowsingDialog implements ModalDialogProperties.Controller {
    private final ModalDialogManager mModalDialogManager;
    private final PropertyModel mModel;
    private final Profile mProfile;
    private final CheckBox mDontAskAgainCheckBox;

    /**
     * Constructs the caret browsing dialog.
     *
     * @param activity The activity to show the dialog in.
     * @param modalDialogManager The modal dialog manager for showing the dialog.
     * @param profile The user profile
     */
    public CaretBrowsingDialog(
            Activity activity, ModalDialogManager modalDialogManager, Profile profile) {
        mModalDialogManager = modalDialogManager;
        mProfile = profile;

        View dialogView =
                LayoutInflater.from(activity).inflate(R.layout.caret_browsing_ask_again_view, null);
        mDontAskAgainCheckBox = (CheckBox) dialogView.findViewById(R.id.dont_ask_again);

        PropertyModel.Builder builder =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.TITLE,
                                activity.getResources(),
                                R.string.caret_browsing_dialog_title)
                        .with(ModalDialogProperties.CUSTOM_VIEW, dialogView)
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                activity.getResources()
                                        .getString(R.string.caret_browsing_dialog_message))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                activity.getResources(),
                                R.string.turn_on)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                activity.getResources(),
                                android.R.string.cancel)
                        .with(ModalDialogProperties.CONTROLLER, this)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true);
        mModel = builder.build();
    }

    /** Shows the caret browsing confirmation dialog. This is toggled via F7 keyboard shortcut. */
    public void show() {
        mModalDialogManager.showDialog(mModel, ModalDialogManager.ModalDialogType.APP);
    }

    /**
     * Handles dialog inputs
     *
     * @param model The dialog model
     * @param buttonType The input that was triggered
     */
    @Override
    public void onClick(PropertyModel model, int buttonType) {
        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
            boolean checked = mDontAskAgainCheckBox.isChecked();
            AccessibilitySettingsBridge.setShowCaretBrowsingDialogPreference(mProfile, !checked);
            AccessibilitySettingsBridge.setCaretBrowsingEnabled(mProfile, true);
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
            mModalDialogManager.dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
            RecordHistogram.recordEnumeratedHistogram(
                    AccessibilitySettingsBridge.ACCESSIBILITY_CARET_BROWING_HISTOGRAM,
                    AccessibilitySettingsBridge.AccessibilityCaretBrowsingAction.DISMISSED,
                    AccessibilitySettingsBridge.AccessibilityCaretBrowsingAction.COUNT);
        }
    }

    /**
     * Called when the dialog is dismissed. No-op in this case.
     *
     * @param model The dialog model.
     * @param dismissalCause The reason for the dismissal.
     */
    @Override
    public void onDismiss(PropertyModel model, int dismissalCause) {
        RecordHistogram.recordEnumeratedHistogram(
                AccessibilitySettingsBridge.ACCESSIBILITY_CARET_BROWING_HISTOGRAM,
                AccessibilitySettingsBridge.AccessibilityCaretBrowsingAction.DISMISSED,
                AccessibilitySettingsBridge.AccessibilityCaretBrowsingAction.COUNT);
    }

    /**
     * @return The {@link PropertyModel} for this dialog. @VisibleForTesting
     */
    PropertyModel getModelForTesting() {
        return mModel;
    }

    /**
     * @return The custom {@link View} for this dialog. @VisibleForTesting
     */
    View getCustomViewForTesting() {
        return mModel.get(ModalDialogProperties.CUSTOM_VIEW);
    }

    /**
     * @param profile The user profile
     * @return true if we should show the dialog for handling keyboard shortcut
     */
    public static boolean shouldShowDialogForKeyboardShortcut(Profile profile) {
        if (AccessibilitySettingsBridge.isCaretBrowsingEnabled(profile)) {
            // Do not show the dialog.
            // When caret browsing is already enabled, the keyboard shortcut will just toggle the
            // feature off.
            AccessibilitySettingsBridge.setCaretBrowsingEnabled(profile, false);
            return false;
        } else if (!AccessibilitySettingsBridge.isShowCaretBrowsingDialogPreference(profile)) {
            // Do not show the dialog.
            // When the user has already seen the dialog and checked the "Don't show this again"
            // checkbox, just toggle it on.
            AccessibilitySettingsBridge.setCaretBrowsingEnabled(profile, true);
            return false;
        } else {
            // Show the dialog.
            // Reach here when caret browsing is not enabled, and user did not click on "Don't show
            // this again"
            return true;
        }
    }
}
