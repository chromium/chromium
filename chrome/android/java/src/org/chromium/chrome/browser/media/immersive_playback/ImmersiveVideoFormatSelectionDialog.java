// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import android.content.Context;
import android.content.res.Resources;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;

import androidx.activity.ComponentDialog;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.content_public.browser.ImmersivePlaybackConfirmationStatus;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/** Helper class to show a confirmation dialog for immersive mode selection. */
@NullMarked
public class ImmersiveVideoFormatSelectionDialog {

    private final Context mContext;
    private final Resources mResources;
    private final ModalDialogManager mModalDialogManager;
    private final ImmersivePlaybackConfirmationCallback mCallback;
    private @Nullable PropertyModel mDialogModel;
    private @Nullable ImmersiveVideoFormatRadioGroup mRadioGroup;

    private final ModalDialogManager.ModalDialogManagerObserver mObserver =
            new ModalDialogManager.ModalDialogManagerObserver() {
                @Override
                public void onDialogCreated(PropertyModel model, @Nullable ComponentDialog dialog) {
                    if (model == mDialogModel && dialog != null) {
                        Window window = dialog.getWindow();
                        if (window != null) {
                            int widthPx =
                                    mResources.getDimensionPixelSize(
                                            R.dimen.immersive_playback_confirmation_dialog_width);
                            WindowManager.LayoutParams lp = window.getAttributes();
                            lp.width = widthPx;
                            window.setAttributes(lp);
                        }
                    }
                }
            };

    public ImmersiveVideoFormatSelectionDialog(
            Context context,
            ModalDialogManager modalDialogManager,
            ImmersivePlaybackConfirmationCallback callback) {
        mContext = context;
        mResources = context.getResources();
        mModalDialogManager = modalDialogManager;
        mCallback = callback;
    }

    public void show() {
        int padding =
                mResources.getDimensionPixelSize(
                        R.dimen.immersive_playback_confirmation_dialog_padding);

        mRadioGroup = new ImmersiveVideoFormatRadioGroup(mContext);
        mRadioGroup.setPadding(padding, 0, padding, 0);
        mRadioGroup.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        ModalDialogProperties.Controller dialogController =
                new SimpleModalDialogController(mModalDialogManager, this::onDismiss);

        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(
                                ModalDialogProperties.TITLE,
                                mResources,
                                R.string.immersive_playback_confirmation_format_title)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mRadioGroup)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mResources,
                                R.string.immersive_playback_confirmation_ok)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mResources,
                                R.string.immersive_playback_confirmation_cancel)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        mModalDialogManager.addObserver(mObserver);
        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP, true);
    }

    private void onDismiss(@DialogDismissalCause int dismissalCause) {
        mModalDialogManager.removeObserver(mObserver);
        mDialogModel = null;
        int status = ImmersivePlaybackConfirmationStatus.FAILED;
        int stereoMode = ImmersiveStereoMode.MONO;
        int projectionType = ImmersiveProjectionType.QUAD;

        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED && mRadioGroup != null) {
            status = ImmersivePlaybackConfirmationStatus.CONFIRMED;
            ImmersiveVideoFormatRadioGroup.FormatOption selected = mRadioGroup.getSelectedFormat();
            stereoMode = selected.stereoMode;
            projectionType = selected.projectionType;
        } else if (dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
            status = ImmersivePlaybackConfirmationStatus.DECLINED;
        } else if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
            status = ImmersivePlaybackConfirmationStatus.CANCELED;
        }

        mRadioGroup = null;
        mCallback.onResult(status, stereoMode, projectionType);
    }

    public void dismiss() {
        if (mDialogModel != null) {
            mModalDialogManager.removeObserver(mObserver);
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
            mDialogModel = null;
        }
    }
}
