// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.choice_screen.ChoiceDialogMediator.DialogType;
import org.chromium.components.search_engines.SearchEngineChoiceService;
import org.chromium.components.search_engines.SearchEnginesFeatureUtils;
import org.chromium.components.search_engines.SearchEnginesFeatures;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

import java.util.function.Function;

/**
 * Entry point to show a blocking choice dialog inviting users to finish their default app & search
 * engine choice in Android settings.
 */
public class ChoiceDialogCoordinator implements ChoiceDialogMediator.Delegate {
    private static final String TAG = "ChoiceDialogCoordntr";

    // TODO(b/365100489): Refactor this coordinator to implement the dialog's custom view fully
    // using the standard chromium MVC patterns. This class is a temporary shortcut.
    interface ViewHolder {
        View getView();

        void updateViewForType(
                @DialogType int dialogType, @Nullable Callback<Integer> actionButtonClickCallback);
    }

    private final ViewHolder mViewHolder;
    private final ChoiceDialogMediator mMediator;
    private final PropertyModel mModel;
    private final ModalDialogManager mModalDialogManager;

    private final ModalDialogManagerObserver mDialogAddedObserver =
            new ModalDialogManagerObserver() {
                @Override
                public void onDialogAdded(PropertyModel model) {
                    if (model != mModel) return;

                    mMediator.onDialogAdded();
                    RecordUserAction.record("OsDefaultsChoiceDialogShown");
                }

                @Override
                public void onDialogDismissed(PropertyModel model) {
                    if (model != mModel) return;

                    // TODO(b/365100489): Look into moving this (and maybe action button click?) to
                    // the `ModalDialogProperties.CONTROLLER` instead.
                    mMediator.onDialogDismissed();
                    RecordUserAction.record("OsDefaultsChoiceDialogClosed");
                }
            };

    private final OnBackPressedCallback mEmptyBackPressedCallback =
            new OnBackPressedCallback(true) {
                @Override
                public void handleOnBackPressed() {}
            };

    public static boolean maybeShow(
            Context context,
            ModalDialogManager modalDialogManager,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        return maybeShowInternal(
                searchEngineChoiceService ->
                        new ChoiceDialogCoordinator(
                                new ViewHolderImpl(context),
                                modalDialogManager,
                                lifecycleDispatcher,
                                searchEngineChoiceService));
    }

    @VisibleForTesting
    static boolean maybeShowInternal(
            Function<SearchEngineChoiceService, ChoiceDialogCoordinator> coordinatorFactory) {
        var searchEngineChoiceService = SearchEngineChoiceService.getInstance();
        final boolean canShow =
                searchEngineChoiceService != null
                        && searchEngineChoiceService.isDeviceChoiceDialogEligible();

        if (SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging()) {
            // TODO(b/355186707): Temporary log to be removed after e2e validation.
            Log.i(TAG, "maybeShow() - Client eligible for the device choice dialog: %b", canShow);
        }

        if (!canShow) {
            return false;
        }

        coordinatorFactory.apply(searchEngineChoiceService);

        // In dark launch mode, we won't show the UI regardless of the backend response, so we can
        // let other promos get triggered after this one.
        return !SearchEnginesFeatureUtils.clayBlockingIsDarkLaunch();
    }

    @VisibleForTesting
    ChoiceDialogCoordinator(
            ViewHolder viewHolder,
            ModalDialogManager modalDialogManager,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull SearchEngineChoiceService searchEngineChoiceService) {
        mViewHolder = viewHolder;
        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mViewHolder.getView())
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                new ModalDialogProperties.Controller() {
                                    @Override
                                    public void onClick(PropertyModel model, int buttonType) {}

                                    @Override
                                    public void onDismiss(
                                            PropertyModel model, int dismissalCause) {}
                                })
                        .build();
        mModalDialogManager = modalDialogManager;
        mMediator = new ChoiceDialogMediator(lifecycleDispatcher, searchEngineChoiceService);

        mMediator.startObserving(/* delegate= */ this);
    }

    @Override
    public void updateDialogType(@DialogType int dialogType) {
        if (SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging()) {
            // TODO(b/355186707): Temporary log to be removed after e2e validation.
            Log.i(TAG, "updateDialogType(%d)", dialogType);
        }

        mViewHolder.updateViewForType(
                dialogType,
                dialogType == DialogType.LOADING ? null : mMediator::onActionButtonClick);

        switch (dialogType) {
            case DialogType.LOADING, DialogType.CHOICE_LAUNCH -> {
                mModel.set(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false);
                mModel.set(
                        ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                        // Capture back navigation and suppress it. The user must complete the
                        // screen by interacting with the options presented.
                        mEmptyBackPressedCallback);
            }
            case DialogType.CHOICE_CONFIRM -> {
                mModel.set(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true);
                mEmptyBackPressedCallback.remove();
                RecordUserAction.record("OsDefaultsChoiceDialogUnblocked");
            }
            case DialogType.UNKNOWN -> throw new IllegalStateException();
        }
    }

    @Override
    public void showDialog() {
        assert SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING);
        if (SearchEnginesFeatureUtils.clayBlockingIsDarkLaunch()) {
            if (SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging()) {
                // TODO(b/355186707): Temporary log to be removed after e2e validation.
                Log.i(TAG, "[DarkLaunch] showDialog() suppressed");
            }
            return; // Ensure that we never show the dialog when the feature is in dark launch mode.
        }

        mModalDialogManager.addObserver(mDialogAddedObserver);
        mModalDialogManager.showDialog(
                mModel,
                ModalDialogManager.ModalDialogType.APP,
                ModalDialogManager.ModalDialogPriority.VERY_HIGH);
    }

    @Override
    public void dismissDialog() {
        mModalDialogManager.dismissDialog(mModel, DialogDismissalCause.UNKNOWN);
    }

    @Override
    public void onMediatorDestroyed() {
        mModalDialogManager.removeObserver(mDialogAddedObserver);
    }

    private static class ViewHolderImpl implements ViewHolder {
        private final View mView;

        @SuppressLint("InflateParams")
        ViewHolderImpl(Context context) {
            mView = LayoutInflater.from(context).inflate(R.layout.blocking_choice_dialog, null);
        }

        @Override
        public View getView() {
            return mView;
        }

        @Override
        public void updateViewForType(
                @DialogType int dialogType, @Nullable Callback<Integer> actionButtonClickCallback) {
            View illustration = mView.findViewById(R.id.illustration);
            TextView title = mView.findViewById(R.id.choice_dialog_title);
            TextView message = mView.findViewById(R.id.choice_dialog_message);
            ButtonCompat button = mView.findViewById(R.id.choice_dialog_button);

            switch (dialogType) {
                case DialogType.LOADING, DialogType.CHOICE_LAUNCH -> {
                    illustration.setBackgroundResource(
                            R.drawable.blocking_choice_dialog_illustration);
                    title.setText(R.string.blocking_choice_dialog_first_title);
                    message.setText(R.string.blocking_choice_dialog_first_message);
                    button.setText(mView.getContext().getString(R.string.next));
                }
                case DialogType.CHOICE_CONFIRM -> {
                    illustration.setBackgroundResource(
                            R.drawable.blocking_choice_confirmation_illustration);
                    title.setText(R.string.blocking_choice_dialog_second_title);
                    message.setText(R.string.blocking_choice_dialog_second_message);
                    button.setText(mView.getContext().getString(R.string.done));
                }
                case DialogType.UNKNOWN -> throw new IllegalStateException();
            }

            button.setOnClickListener(
                    actionButtonClickCallback == null
                            ? null
                            : ignored -> actionButtonClickCallback.onResult(dialogType));
            button.setEnabled(actionButtonClickCallback != null);
        }
    }
}
