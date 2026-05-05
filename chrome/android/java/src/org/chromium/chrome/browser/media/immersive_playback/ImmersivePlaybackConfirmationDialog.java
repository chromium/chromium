// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import android.content.Context;
import android.content.res.Resources;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.blink.mojom.ImmersivePlaybackConfirmationStatus;
import org.chromium.blink.mojom.ImmersiveProjectionType;
import org.chromium.blink.mojom.ImmersiveStereoMode;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

/** Helper class to show a confirmation dialog for immersive mode selection. */
@NullMarked
public class ImmersivePlaybackConfirmationDialog {
    static final int[] STEREO_MODE_KEYS = {
        ImmersiveStereoMode.MONO, ImmersiveStereoMode.SIDE_BY_SIDE, ImmersiveStereoMode.TOP_BOTTOM
    };

    private static final int[] STEREO_MODE_LABEL_IDS = {
        R.string.immersive_playback_confirmation_stereo_mode_mono,
        R.string.immersive_playback_confirmation_stereo_mode_side_by_side,
        R.string.immersive_playback_confirmation_stereo_mode_top_bottom
    };

    static final int[] PROJECTION_TYPE_KEYS = {
        ImmersiveProjectionType.QUAD,
        ImmersiveProjectionType.SPHERE,
        ImmersiveProjectionType.HEMISPHERE
    };

    private static final int[] PROJECTION_TYPE_LABEL_IDS = {
        R.string.immersive_playback_confirmation_projection_type_quad,
        R.string.immersive_playback_confirmation_projection_type_sphere,
        R.string.immersive_playback_confirmation_projection_type_hemisphere
    };

    public interface ConfirmationCallback {
        void onResult(int status, int stereoMode, int projectionType);
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final ConfirmationCallback mCallback;
    private @Nullable PropertyModel mDialogModel;

    @Nullable Spinner mStereoSpinner;
    @Nullable Spinner mProjectionSpinner;

    public ImmersivePlaybackConfirmationDialog(
            Context context, ModalDialogManager modalDialogManager, ConfirmationCallback callback) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mCallback = callback;
    }

    public void show() {
        Resources resources = mContext.getResources();
        int padding =
                resources.getDimensionPixelSize(
                        R.dimen.immersive_playback_confirmation_dialog_padding);

        LinearLayout customView = new LinearLayout(mContext);
        customView.setOrientation(LinearLayout.HORIZONTAL);
        customView.setPadding(padding, 0, padding, 0);

        LinearLayout.LayoutParams columnParams =
                new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1.0f);

        LinearLayout stereoLayout = new LinearLayout(mContext);
        stereoLayout.setOrientation(LinearLayout.VERTICAL);
        stereoLayout.addView(
                createLabel(
                        resources.getString(
                                R.string.immersive_playback_confirmation_stereo_mode_label)));
        Spinner stereoSpinner = createSpinner(STEREO_MODE_LABEL_IDS);
        mStereoSpinner = stereoSpinner;
        stereoLayout.addView(stereoSpinner);
        customView.addView(stereoLayout, columnParams);

        LinearLayout projectionLayout = new LinearLayout(mContext);
        projectionLayout.setOrientation(LinearLayout.VERTICAL);
        projectionLayout.addView(
                createLabel(
                        resources.getString(
                                R.string.immersive_playback_confirmation_projection_type_label)));
        Spinner projectionSpinner = createSpinner(PROJECTION_TYPE_LABEL_IDS);
        mProjectionSpinner = projectionSpinner;
        projectionLayout.addView(projectionSpinner);
        customView.addView(projectionLayout, columnParams);

        ModalDialogProperties.Controller dialogController =
                new SimpleModalDialogController(mModalDialogManager, this::onDismiss);

        mDialogModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, dialogController)
                        .with(
                                ModalDialogProperties.TITLE,
                                resources,
                                R.string.immersive_playback_confirmation_title)
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                resources.getString(
                                        R.string.immersive_playback_confirmation_message))
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                resources,
                                R.string.immersive_playback_confirmation_confirm)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                resources,
                                R.string.immersive_playback_confirmation_decline)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .build();

        mModalDialogManager.showDialog(mDialogModel, ModalDialogManager.ModalDialogType.APP, true);
    }

    private TextView createLabel(String text) {
        TextView label = new TextView(mContext);
        label.setText(text);
        return label;
    }

    private Spinner createSpinner(int[] labelIds) {
        Spinner spinner = new Spinner(mContext);
        String[] labels = new String[labelIds.length];
        Resources res = mContext.getResources();
        for (int i = 0; i < labelIds.length; i++) {
            labels[i] = res.getString(labelIds[i]);
        }
        ArrayAdapter<String> adapter =
                new ArrayAdapter<>(mContext, android.R.layout.simple_spinner_item, labels);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(adapter);
        return spinner;
    }

    private void onDismiss(@DialogDismissalCause int dismissalCause) {
        mDialogModel = null;
        int status = ImmersivePlaybackConfirmationStatus.FAILED;
        int stereoMode = ImmersiveStereoMode.MONO;
        int projectionType = ImmersiveProjectionType.QUAD;

        if (dismissalCause == DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                && mStereoSpinner != null
                && mProjectionSpinner != null) {
            status = ImmersivePlaybackConfirmationStatus.CONFIRMED;
            int stereoPosition = mStereoSpinner.getSelectedItemPosition();
            if (stereoPosition >= 0 && stereoPosition < STEREO_MODE_KEYS.length) {
                stereoMode = STEREO_MODE_KEYS[stereoPosition];
            }
            int projectionPosition = mProjectionSpinner.getSelectedItemPosition();
            if (projectionPosition >= 0 && projectionPosition < PROJECTION_TYPE_KEYS.length) {
                projectionType = PROJECTION_TYPE_KEYS[projectionPosition];
            }
        } else if (dismissalCause == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
            status = ImmersivePlaybackConfirmationStatus.DECLINED;
        } else if (dismissalCause == DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE) {
            status = ImmersivePlaybackConfirmationStatus.CANCELED;
        }

        mStereoSpinner = null;
        mProjectionSpinner = null;
        mCallback.onResult(status, stereoMode, projectionType);
    }

    public void dismiss() {
        if (mDialogModel != null) {
            mModalDialogManager.dismissDialog(
                    mDialogModel, DialogDismissalCause.DISMISSED_BY_NATIVE);
            mDialogModel = null;
        }
    }
}
