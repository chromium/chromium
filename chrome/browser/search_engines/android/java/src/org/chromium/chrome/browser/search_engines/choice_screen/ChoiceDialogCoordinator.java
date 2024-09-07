// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.concurrent.TimeoutException;

/**
 * Entry point to show a blocking choice dialog inviting users to finish their default app & search
 * engine choice in Android settings.
 */
public class ChoiceDialogCoordinator {
    @IntDef({DialogType.CHOICE_LAUNCH, DialogType.CHOICE_CONFIRM})
    @Retention(RetentionPolicy.SOURCE)
    private @interface DialogType {
        int CHOICE_LAUNCH = 0;
        int CHOICE_CONFIRM = 1;
    }

    private final Context mContext;
    private final ModalDialogManager mModalDialogManager;
    private final View mView;
    private final PropertyModel mModel;
    private final OnBackPressedCallback mEmptyBackPressedCallback =
            new OnBackPressedCallback(true) {
                @Override
                public void handleOnBackPressed() {}
            };
    private @DialogType int mType = DialogType.CHOICE_LAUNCH;

    public static ChoiceDialogCoordinator maybeShow(
            Context context, ModalDialogManager modalDialogManager) {
        return maybeShowInternal(() -> new ChoiceDialogCoordinator(context, modalDialogManager));
    }

    @VisibleForTesting
    static ChoiceDialogCoordinator maybeShowInternal(
            Supplier<ChoiceDialogCoordinator> coordinatorSupplier) {
        var searchEngineChoiceService = SearchEngineChoiceService.getInstance();
        if (searchEngineChoiceService == null
                || !searchEngineChoiceService.isDeviceChoiceDialogEligible()) {
            return null;
        }

        var coordinator = coordinatorSupplier.get();
        withUiThreadTimeout(searchEngineChoiceService.shouldShowDeviceChoiceDialog(), 1000)
                .then(
                        shouldShow -> {
                            if (shouldShow) coordinator.show();
                        },
                        unused -> {
                            /* timeout*/
                        });

        return coordinator;
    }

    /** Constructs and shows the dialog. */
    public void show() {
        mModalDialogManager.showDialog(
                mModel,
                ModalDialogManager.ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH);
    }

    @VisibleForTesting
    ChoiceDialogCoordinator(Context context, ModalDialogManager modalDialogManager) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mView = LayoutInflater.from(context).inflate(R.layout.blocking_choice_dialog, null);
        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mView)
                        .with(ModalDialogProperties.CONTROLLER, createController())
                        .build();

        prepareView();
        ButtonCompat button = mView.findViewById(R.id.choice_dialog_button);
        button.setOnClickListener(
                view -> {
                    // TODO(b/355054464): Remove after play api is ready and add test for the logic.
                    switch (mType) {
                        case DialogType.CHOICE_LAUNCH -> advance();
                        case DialogType.CHOICE_CONFIRM -> mModalDialogManager.dismissDialog(
                                mModel, DialogDismissalCause.ACTION_ON_CONTENT);
                    }
                });
    }

    private static <T> Promise<T> withUiThreadTimeout(Promise<T> promise, long delayMillis) {
        if (!promise.isPending()) return promise;

        Promise<T> timeoutPromise = new Promise<>();
        promise.then(timeoutPromise::fulfill, timeoutPromise::reject);
        ThreadUtils.postOnUiThreadDelayed(
                () -> {
                    if (timeoutPromise.isPending()) {
                        timeoutPromise.reject(new TimeoutException());
                    }
                },
                delayMillis);
        return timeoutPromise;
    }

    private ModalDialogProperties.Controller createController() {
        return new ModalDialogProperties.Controller() {
            @Override
            public void onClick(PropertyModel model, int buttonType) {}

            @Override
            public void onDismiss(PropertyModel model, int dismissalCause) {
                assert mType == DialogType.CHOICE_CONFIRM
                        || dismissalCause == DialogDismissalCause.ACTIVITY_DESTROYED;
            }
        };
    }

    private void prepareView() {
        View illustration = mView.findViewById(R.id.illustration);
        TextView title = mView.findViewById(R.id.choice_dialog_title);
        TextView message = mView.findViewById(R.id.choice_dialog_message);
        ButtonCompat button = mView.findViewById(R.id.choice_dialog_button);
        switch (mType) {
            case DialogType.CHOICE_LAUNCH -> {
                illustration.setBackgroundResource(R.drawable.blocking_choice_dialog_illustration);
                title.setText(R.string.blocking_choice_dialog_first_title);
                message.setText(R.string.blocking_choice_dialog_first_message);
                button.setText(mContext.getString(R.string.blocking_choice_dialog_first_button));

                mModel.set(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false);
                mModel.set(
                        ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                        // Capture back navigation and suppress it. The user must complete the
                        // screen by interacting with the options presented.
                        mEmptyBackPressedCallback);
            }
            case DialogType.CHOICE_CONFIRM -> {
                illustration.setBackgroundResource(R.drawable.blocking_choice_dialog_illustration);
                title.setText(R.string.blocking_choice_dialog_second_title);
                message.setText(R.string.blocking_choice_dialog_second_message);
                button.setText(mContext.getString(R.string.blocking_choice_dialog_second_button));

                mModel.set(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true);
                mEmptyBackPressedCallback.remove();
            }
            default -> throw new IllegalArgumentException("Invalid DialogType: " + mType);
        }
    }

    public void advance() {
        if (mType == DialogType.CHOICE_CONFIRM) {
            return;
        }

        mType = DialogType.CHOICE_CONFIRM;
        prepareView();
    }
}
