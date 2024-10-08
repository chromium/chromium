// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.SEARCH_ENGINE_CHOICE_PENDING_OS_CHOICE_DIALOG_SHOWN_ATTEMPTS;

import android.annotation.SuppressLint;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.widget.TextView;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
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

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
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

        void updateViewForType(@DialogType int dialogType);
    }

    private final Context mContext;
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

    /**
     * Attempts to show the device defaults apps choice dialog.
     *
     * @return {@code true} if the dialog will be shown, {@code false} otherwise.
     */
    public static boolean maybeShow(
            Context context,
            ModalDialogManager modalDialogManager,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        return maybeShowInternal(
                searchEngineChoiceService ->
                        new ChoiceDialogCoordinator(
                                context,
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
            clearDialogShownCount();
            return false;
        }

        coordinatorFactory.apply(searchEngineChoiceService);

        // If the dialog is suppressed, we won't show the UI regardless of the backend response, so
        // we can let other promos get triggered after this one.
        return computeDialogSuppressionStatus() == DialogSuppressionStatus.CAN_SHOW;
    }

    @VisibleForTesting
    ChoiceDialogCoordinator(
            Context context,
            ViewHolder viewHolder,
            ModalDialogManager modalDialogManager,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            @NonNull SearchEngineChoiceService searchEngineChoiceService) {
        mContext = context;
        mViewHolder = viewHolder;
        mModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CUSTOM_VIEW, mViewHolder.getView())
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                new ModalDialogProperties.Controller() {
                                    @Override
                                    public void onClick(PropertyModel model, int buttonType) {
                                        mMediator.onActionButtonClick();
                                    }

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

        mViewHolder.updateViewForType(dialogType);

        switch (dialogType) {
            case DialogType.LOADING, DialogType.CHOICE_LAUNCH -> {
                mModel.set(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, false);
                mModel.set(
                        ModalDialogProperties.APP_MODAL_DIALOG_BACK_PRESS_HANDLER,
                        // Capture back navigation and suppress it. The user must complete the
                        // screen by interacting with the options presented.
                        mEmptyBackPressedCallback);
                mModel.set(
                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        mContext.getString(R.string.next));
                mModel.set(
                        ModalDialogProperties.POSITIVE_BUTTON_DISABLED,
                        dialogType == DialogType.LOADING);
            }
            case DialogType.CHOICE_CONFIRM -> {
                mModel.set(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true);
                mModel.set(
                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        mContext.getString(R.string.done));
                mModel.set(ModalDialogProperties.POSITIVE_BUTTON_DISABLED, false);
                mEmptyBackPressedCallback.remove();
                RecordUserAction.record("OsDefaultsChoiceDialogUnblocked");
            }
            case DialogType.UNKNOWN -> throw new IllegalStateException();
        }
    }

    @Override
    public void showDialog() {
        assert SearchEnginesFeatures.isEnabled(SearchEnginesFeatures.CLAY_BLOCKING);
        @DialogSuppressionStatus int suppressionStatus = computeDialogSuppressionStatus();
        recordShowDialogStatus(suppressionStatus);
        if (suppressionStatus != DialogSuppressionStatus.CAN_SHOW) {
            return; // Suppress the dialog.
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

    @IntDef({
        DialogSuppressionStatus.CAN_SHOW,
        DialogSuppressionStatus.SUPPRESSED_DARK_LAUNCH,
        DialogSuppressionStatus.SUPPRESSED_ESCAPE_HATCH,
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    @interface DialogSuppressionStatus {
        // These values are persisted to logs. Entries should not be renumbered and numeric values
        // should never be reused.
        // LINT.IfChange(DialogSuppressionStatus)
        int CAN_SHOW = 0;
        int SUPPRESSED_DARK_LAUNCH = 1;
        int SUPPRESSED_ESCAPE_HATCH = 2;
        int COUNT = 3;
        // LINT.ThenChange(//tools/metrics/histograms/enums.xml:OsDefaultsChoiceDialogSuppressionStatus)
    }

    @DialogSuppressionStatus
    private static int computeDialogSuppressionStatus() {
        if (SearchEnginesFeatureUtils.clayBlockingIsDarkLaunch()) {
            if (SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging()) {
                // TODO(b/355186707): Temporary log to be removed after e2e validation.
                Log.i(TAG, "The dialog is suppressed: Dark Launch mode.");
            }
            return DialogSuppressionStatus.SUPPRESSED_DARK_LAUNCH;
        }

        int blockCount =
                ChromeSharedPreferences.getInstance()
                        .readInt(SEARCH_ENGINE_CHOICE_PENDING_OS_CHOICE_DIALOG_SHOWN_ATTEMPTS);
        int blockLimit = SearchEnginesFeatureUtils.clayBlockingEscapeHatchBlockLimit();
        if (blockLimit > 0 && blockCount >= blockLimit) {
            if (SearchEnginesFeatureUtils.clayBlockingEnableVerboseLogging()) {
                // TODO(b/355186707): Temporary log to be removed after e2e validation.
                Log.i(
                        TAG,
                        "The dialog is suppressed: Escape Hatch triggered, blocked %d times"
                                + " (limit=%d).",
                        blockCount,
                        blockLimit);
            }
            return DialogSuppressionStatus.SUPPRESSED_ESCAPE_HATCH;
        }

        return DialogSuppressionStatus.CAN_SHOW;
    }

    private static void recordShowDialogStatus(@DialogSuppressionStatus int suppressionStatus) {
        RecordHistogram.recordEnumeratedHistogram(
                "Search.OsDefaultsChoice.DialogSuppressionStatus",
                suppressionStatus,
                DialogSuppressionStatus.COUNT);

        if (suppressionStatus == DialogSuppressionStatus.CAN_SHOW) {
            int newCount =
                    ChromeSharedPreferences.getInstance()
                            .incrementInt(
                                    SEARCH_ENGINE_CHOICE_PENDING_OS_CHOICE_DIALOG_SHOWN_ATTEMPTS);
            RecordHistogram.recordLinearCountHistogram(
                    "Search.OsDefaultsChoice.DialogShownAttempt",
                    newCount,
                    /* min= */ 1,
                    /* max= */ 50,
                    /* numBuckets= */ 51);
        }
    }

    private static void clearDialogShownCount() {
        ChromeSharedPreferences.getInstance()
                .removeKey(SEARCH_ENGINE_CHOICE_PENDING_OS_CHOICE_DIALOG_SHOWN_ATTEMPTS);
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
        public void updateViewForType(@DialogType int dialogType) {
            View illustration = mView.findViewById(R.id.illustration);
            TextView title = mView.findViewById(R.id.choice_dialog_title);
            TextView message = mView.findViewById(R.id.choice_dialog_message);

            switch (dialogType) {
                case DialogType.LOADING, DialogType.CHOICE_LAUNCH -> {
                    illustration.setBackgroundResource(
                            R.drawable.blocking_choice_dialog_illustration);
                    title.setText(R.string.blocking_choice_dialog_first_title);
                    message.setText(R.string.blocking_choice_dialog_first_message);
                }
                case DialogType.CHOICE_CONFIRM -> {
                    illustration.setBackgroundResource(
                            R.drawable.blocking_choice_confirmation_illustration);
                    title.setText(R.string.blocking_choice_dialog_second_title);
                    message.setText(R.string.blocking_choice_dialog_second_message);
                }
                case DialogType.UNKNOWN -> throw new IllegalStateException();
            }
            // As the dialog states change the text, focus accessibility every time the state
            // changes.
            title.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
        }
    }
}
