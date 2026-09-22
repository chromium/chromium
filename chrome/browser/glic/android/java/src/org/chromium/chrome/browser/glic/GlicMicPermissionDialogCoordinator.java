// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import android.app.Activity;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonStyles;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/**
 * Shows a Chrome-owned modal dialog asking the user for microphone permission for Gemini.
 *
 * <p>This is a pre-prompt shown before the Android OS permission dialog so that the user
 * understands why Gemini needs the microphone. The body text contains a link to the Chrome "App
 * info" settings screen, where the permission can also be managed.
 */
@NullMarked
public class GlicMicPermissionDialogCoordinator {
    private final Activity mActivity;
    private final ModalDialogManager mModalDialogManager;

    /**
     * @param activity The activity used to inflate the dialog's custom view.
     * @param modalDialogManager The manager the dialog is shown with.
     */
    public GlicMicPermissionDialogCoordinator(
            Activity activity, ModalDialogManager modalDialogManager) {
        mActivity = activity;
        mModalDialogManager = modalDialogManager;
    }

    /**
     * Shows the dialog.
     *
     * @param callback Invoked exactly once with true if the user taps "Allow", or false if the
     *     dialog is rejected or dismissed for any other reason. Note that this may run after the
     *     caller's owner has gone away (e.g. the dialog is dismissed while the activity is being
     *     destroyed), so callers must not assume their dependencies are still alive.
     */
    public void show(Callback<Boolean> callback) {
        // SimpleModalDialogController dismisses the dialog on button clicks and guarantees the
        // action callback runs exactly once, on dismissal.
        SimpleModalDialogController controller =
                new SimpleModalDialogController(
                        mModalDialogManager,
                        dismissalCause ->
                                callback.onResult(
                                        dismissalCause
                                                == DialogDismissalCause.POSITIVE_BUTTON_CLICKED));

        View customView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.glic_mic_permission_dialog, /* root= */ null);

        PropertyModel dialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mActivity.getString(R.string.glic_mic_permission_dialog_allow))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mActivity.getString(R.string.no_thanks))
                        .build();

        // Linkifies "Settings" in the body text. This happens after the model is built so that the
        // span can dismiss the dialog it belongs to.
        ChromeClickableSpan settingsSpan =
                new ChromeClickableSpan(
                        mActivity,
                        v -> {
                            GlicUiUtils.openAppDetailsSettings(mActivity);
                            mModalDialogManager.dismissDialog(
                                    dialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
                        });
        TextView messageView = customView.findViewById(R.id.mic_dialog_message);
        messageView.setText(
                SpanApplier.applySpans(
                        mActivity.getString(R.string.glic_mic_permission_dialog_message),
                        new SpanInfo("<link>", "</link>", settingsSpan)));
        messageView.setMovementMethod(LinkMovementMethod.getInstance());

        mModalDialogManager.showDialog(dialogModel, ModalDialogType.APP);
    }
}
