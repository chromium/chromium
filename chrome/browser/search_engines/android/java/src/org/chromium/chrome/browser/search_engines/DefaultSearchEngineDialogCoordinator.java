// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.MainThread;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.components.browser_ui.widget.RadioButtonLayout;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.Controller;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A dialog that forces the user to choose a default search engine. It can't be dismissed by back
 * press.
 */
public class DefaultSearchEngineDialogCoordinator {
    private final ModalDialogManager mDialogManager;
    private final DefaultSearchEngineDialogHelper.Delegate mDelegate;
    private final @SearchEnginePromoType int mType;
    private final @Nullable Callback<Boolean> mOnSuccessCallback;
    private final PropertyModel mModel;
    private DefaultSearchEngineDialogHelper mHelper;

    public DefaultSearchEngineDialogCoordinator(
            Activity activity,
            DefaultSearchEngineDialogHelper.Delegate delegate,
            @SearchEnginePromoType int dialogType,
            @Nullable Callback<Boolean> onSuccessCallback) {
        mDialogManager = ((ModalDialogManagerHolder) activity).getModalDialogManager();
        mDelegate = delegate;
        mType = dialogType;
        mOnSuccessCallback = onSuccessCallback;

        View contentView =
                LayoutInflater.from(activity)
                        .inflate(R.layout.default_search_engine_dialog_view, /* root= */ null);
        Controller controller =
                new Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {}

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        assert dismissalCause == DialogDismissalCause.ACTION_ON_CONTENT
                                || dismissalCause == DialogDismissalCause.ACTIVITY_DESTROYED;
                    }
                };
        OnBackPressedCallback onBackPressedCallback =
                new OnBackPressedCallback(true) {
                    @Override
                    public void handleOnBackPressed() {}
                };
        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(
                                ModalDialogProperties.TITLE,
                                activity.getString(R.string.search_engine_dialog_title))
                        .with(ModalDialogProperties.CUSTOM_VIEW, contentView)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false)
                        .with(
                                ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                                onBackPressedCallback)
                        .build();

        RadioButtonLayout radioButtons = contentView.findViewById(R.id.radio_buttons);
        Button primaryButton = contentView.findViewById(R.id.primary_button);
        Runnable finishRunnable =
                () -> {
                    assert mHelper.getConfirmedKeyword() != null;

                    if (mOnSuccessCallback != null) {
                        mOnSuccessCallback.onResult(true);
                    }
                    mDialogManager.dismissDialog(mModel, DialogDismissalCause.ACTION_ON_CONTENT);
                };
        mHelper =
                new DefaultSearchEngineDialogHelper(
                        mType, mDelegate, radioButtons, primaryButton, finishRunnable);
    }

    @MainThread
    public void show() {
        ThreadUtils.assertOnUiThread();
        if (mType == SearchEnginePromoType.SHOW_NEW) {
            RecordUserAction.record("SearchEnginePromo.NewDevice.Shown.Dialog");
        } else {
            assert mType == SearchEnginePromoType.SHOW_EXISTING;
            RecordUserAction.record("SearchEnginePromo.ExistingDevice.Shown.Dialog");
        }
        mDialogManager.showDialog(
                mModel,
                ModalDialogManager.ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH);
    }
}
