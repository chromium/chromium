// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.SparseArray;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.VisibleForTesting;
import org.chromium.ui.UiUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Manager for managing the display of a queue of {@link ModalDialogView}s.
 */
public class ModalDialogManager {
    /**
     * Present a {@link ModalDialogView} in a container.
     */
    public static abstract class Presenter {
        private Callback<Integer> mDismissCallback;
        private ModalDialogView mModalDialog;
        private View mCurrentView;

        /**
         * @param dialog The dialog that's currently showing in this presenter. If null, no dialog
         *               is currently showing.
         */
        private void setModalDialog(
                @Nullable ModalDialogView dialog, @Nullable Callback<Integer> dismissCallback) {
            if (dialog == null) {
                removeDialogView(mCurrentView);
                mModalDialog = null;
                mDismissCallback = null;
            } else {
                assert mModalDialog
                        == null : "Should call setModalDialog(null) before setting a modal dialog.";
                mModalDialog = dialog;
                mCurrentView = dialog.getView();
                mDismissCallback = dismissCallback;
                // Make sure the view is detached from any parent before adding it to the container.
                // This is not detached after removeDialogView() because there could be animation
                // running on removing the dialog view.
                UiUtils.removeViewFromParent(mCurrentView);
                addDialogView(mCurrentView);
            }
        }

        /**
         * Run the cached cancel callback and reset the cached callback.
         */
        protected final void dismissCurrentDialog(@DialogDismissalCause int dismissalCause) {
            if (mDismissCallback == null) return;

            // Set #mCancelCallback to null before calling the callback to avoid it being
            // updated during the callback.
            Callback<Integer> callback = mDismissCallback;
            mDismissCallback = null;
            callback.onResult(dismissalCause);
        }

        /**
         * @return The modal dialog that this presenter is showing.
         */
        protected final ModalDialogView getModalDialog() {
            return mModalDialog;
        }

        /**
         * Add the specified {@link ModalDialogView} in a container.
         * @param dialogView The {@link ModalDialogView} that needs to be shown.
         */
        protected abstract void addDialogView(View dialogView);

        /**
         * Remove the specified {@link ModalDialogView} from a container.
         * @param dialogView The {@link ModalDialogView} that needs to be removed.
         */
        protected abstract void removeDialogView(View dialogView);
    }

    @IntDef({ModalDialogType.APP, ModalDialogType.TAB})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ModalDialogType {
        // The integer assigned to each type represents its priority. A smaller number represents a
        // higher priority type of dialog.
        int APP = 0;
        int TAB = 1;
    }

    /** Mapping of the {@link Presenter}s and the type of dialogs they are showing. */
    private final SparseArray<Presenter> mPresenters = new SparseArray<>();

    /** Mapping of the lists of pending dialogs and the type of the dialogs. */
    private final SparseArray<List<ModalDialogView>> mPendingDialogs = new SparseArray<>();

    /**
     * The list of suspended types of dialogs. The dialogs of types in the list will be suspended
     * from showing and will only be shown after {@link #resumeType(int)} is called.
     */
    private final Set<Integer> mSuspendedTypes = new HashSet<>();

    /** The default presenter to be used if a specified type is not supported. */
    private final Presenter mDefaultPresenter;

    /**
     * The presenter of the type of the dialog that is currently showing. Note that if there is no
     * matching {@link Presenter} for {@link #mCurrentType}, this will be the default presenter.
     */
    private Presenter mCurrentPresenter;

    /**
     * The type of the current dialog. This can be different from the type of the current
     * {@link Presenter} if there is no registered presenter for this type.
     */
    private @ModalDialogType int mCurrentType;

    /**
     * True if the current dialog is in the process of being dismissed.
     */
    private boolean mDismissingCurrentDialog;

    /**
     * Constructor for initializing default {@link Presenter}.
     * @param defaultPresenter The default presenter to be used when no presenter specified.
     * @param defaultType The dialog type of the default presenter.
     */
    public ModalDialogManager(
            @NonNull Presenter defaultPresenter, @ModalDialogType int defaultType) {
        mDefaultPresenter = defaultPresenter;
        registerPresenter(defaultPresenter, defaultType);
    }

    /** Clears any dependencies on the showing or pending dialogs. */
    public void destroy() {
        dismissAllDialogs(DialogDismissalCause.ACTIVITY_DESTROYED);
    }

    /**
     * Register a {@link Presenter} that shows a specific type of dialog. Note that only one
     * presenter of each type can be registered.
     * @param presenter The {@link Presenter} to be registered.
     * @param dialogType The type of the dialog shown by the specified presenter.
     */
    public void registerPresenter(Presenter presenter, @ModalDialogType int dialogType) {
        assert mPresenters.get(dialogType)
                == null : "Only one presenter can be registered for each type.";
        mPresenters.put(dialogType, presenter);
    }

    /**
     * @return Whether a dialog is currently showing.
     */
    public boolean isShowing() {
        return mCurrentPresenter != null;
    }

    /**
     * Show the specified dialog. If another dialog is currently showing, the specified dialog will
     * be added to the end of the pending dialog list of the specified type.
     * @param dialog The dialog to be shown or added to pending list.
     * @param dialogType The type of the dialog to be shown.
     */
    public void showDialog(ModalDialogView dialog, @ModalDialogType int dialogType) {
        showDialog(dialog, dialogType, false);
    }

    /**
     * Show the specified dialog. If another dialog is currently showing, the specified dialog will
     * be added to the pending dialog list. If showNext is set to true, the dialog will be added
     * to the top of the pending list of its type, otherwise it will be added to the end.
     * @param dialog The dialog to be shown or added to pending list.
     * @param dialogType The type of the dialog to be shown.
     * @param showAsNext Whether the specified dialog should be set highest priority of its type.
     */
    public void showDialog(
            ModalDialogView dialog, @ModalDialogType int dialogType, boolean showAsNext) {
        List<ModalDialogView> dialogs = mPendingDialogs.get(dialogType);
        if (dialogs == null) mPendingDialogs.put(dialogType, dialogs = new ArrayList<>());

        // Put the new dialog in pending list if the dialog type is suspended or the current dialog
        // is of higher priority.
        if (mSuspendedTypes.contains(dialogType) || (isShowing() && mCurrentType <= dialogType)) {
            dialogs.add(showAsNext ? 0 : dialogs.size(), dialog);
            return;
        }

        if (isShowing()) suspendCurrentDialog();

        assert !isShowing();
        dialog.prepareBeforeShow();
        mCurrentType = dialogType;
        mCurrentPresenter = mPresenters.get(dialogType, mDefaultPresenter);
        mCurrentPresenter.setModalDialog(
                dialog, (dismissalCause) -> dismissDialog(dialog, dismissalCause));
    }

    /**
     * Dismiss the specified dialog. If the dialog is not currently showing, it will be removed from
     * the pending dialog list. If the dialog is currently being dismissed this function does
     * nothing.
     * @param dialog The dialog to be dismissed or removed from pending list.
     *
     * TODO(huayinz): Remove this method and use the one with dismissal cause.
     */
    public void dismissDialog(ModalDialogView dialog) {
        dismissDialog(dialog, DialogDismissalCause.UNKNOWN);
    }

    /**
     * Dismiss the specified dialog. If the dialog is not currently showing, it will be removed from
     * the pending dialog list. If the dialog is currently being dismissed this function does
     * nothing.
     * @param dialog The dialog to be dismissed or removed from pending list.
     * @param dismissalCause The {@link DialogDismissalCause} that describes why the dialog is
     *                       dismissed.
     */
    public void dismissDialog(ModalDialogView dialog, @DialogDismissalCause int dismissalCause) {
        if (dialog == null) return;
        if (mCurrentPresenter == null || dialog != mCurrentPresenter.getModalDialog()) {
            for (int i = 0; i < mPendingDialogs.size(); ++i) {
                List<ModalDialogView> dialogs = mPendingDialogs.valueAt(i);
                for (int j = 0; j < dialogs.size(); ++j) {
                    if (dialogs.get(j) == dialog) {
                        dialogs.remove(j).getController().onDismiss(dismissalCause);
                        return;
                    }
                }
            }
            // If the specified dialog is not found, return without any callbacks.
            return;
        }

        if (!isShowing()) return;
        assert dialog == mCurrentPresenter.getModalDialog();
        if (mDismissingCurrentDialog) return;
        mDismissingCurrentDialog = true;
        dialog.getController().onDismiss(dismissalCause);
        mCurrentPresenter.setModalDialog(null, null);
        mCurrentPresenter = null;
        mDismissingCurrentDialog = false;
        showNextDialog();
    }

    /**
     * Dismiss the dialog currently shown and remove all pending dialogs.
     * @param dismissalCause The {@link DialogDismissalCause} that describes why the dialogs are
     *                       dismissed.
     */
    public void dismissAllDialogs(@DialogDismissalCause int dismissalCause) {
        for (int i = 0; i < mPendingDialogs.size(); ++i) {
            dismissPendingDialogsOfType(mPendingDialogs.keyAt(i), dismissalCause);
        }
        if (isShowing()) dismissDialog(mCurrentPresenter.getModalDialog());
    }

    /**
     * Dismiss the dialog currently shown and remove all pending dialogs of the specified type.
     * @param dialogType The specified type of dialog.
     * @param dismissalCause The {@link DialogDismissalCause} that describes why the dialogs are
     *                       dismissed.
     */
    protected void dismissDialogsOfType(
            @ModalDialogType int dialogType, @DialogDismissalCause int dismissalCause) {
        dismissPendingDialogsOfType(dialogType, dismissalCause);
        if (isShowing() && dialogType == mCurrentType) {
            dismissDialog(mCurrentPresenter.getModalDialog(), dismissalCause);
        }
    }

    /** Helper method to dismiss pending dialogs of the specified type. */
    private void dismissPendingDialogsOfType(
            @ModalDialogType int dialogType, @DialogDismissalCause int dismissalCause) {
        List<ModalDialogView> dialogs = mPendingDialogs.get(dialogType);
        if (dialogs == null) return;

        while (!dialogs.isEmpty()) {
            ModalDialogView.Controller controller = dialogs.remove(0).getController();
            controller.onDismiss(dismissalCause);
        }
    }

    /**
     * Suspend all dialogs of the specified type, including the one currently shown. These dialogs
     * will be prevented from showing unless {@link #resumeType(int)} is called after the
     * suspension. If the current dialog is suspended, it will be moved back to the first dialog
     * in the pending list. Any dialogs of the specified type in the pending list will be skipped.
     * @param dialogType The specified type of dialogs to be suspended.
     */
    protected void suspendType(@ModalDialogType int dialogType) {
        mSuspendedTypes.add(dialogType);
        if (isShowing() && dialogType == mCurrentType) {
            suspendCurrentDialog();
            showNextDialog();
        }
    }

    /**
     * Resume the specified type of dialogs after suspension.
     * @param dialogType The specified type of dialogs to be resumed.
     */
    protected void resumeType(@ModalDialogType int dialogType) {
        mSuspendedTypes.remove(dialogType);
        if (!isShowing()) showNextDialog();
    }

    /** Hide the current dialog and put it back to the front of the pending list. */
    private void suspendCurrentDialog() {
        assert isShowing();
        ModalDialogView dialogView = mCurrentPresenter.getModalDialog();
        mCurrentPresenter.setModalDialog(null, null);
        mCurrentPresenter = null;
        mPendingDialogs.get(mCurrentType).add(0, dialogView);
    }

    /** Helper method for showing the next available dialog in the pending dialog list. */
    private void showNextDialog() {
        assert !isShowing();
        // Show the next dialog of highest priority that its type is not suspended.
        for (int i = 0; i < mPendingDialogs.size(); ++i) {
            int dialogType = mPendingDialogs.keyAt(i);
            if (mSuspendedTypes.contains(dialogType)) continue;

            List<ModalDialogView> dialogs = mPendingDialogs.valueAt(i);
            if (!dialogs.isEmpty()) {
                showDialog(dialogs.remove(0), dialogType);
                return;
            }
        }
    }

    @VisibleForTesting
    public ModalDialogView getCurrentDialogForTest() {
        return mCurrentPresenter == null ? null : mCurrentPresenter.getModalDialog();
    }

    @VisibleForTesting
    List<ModalDialogView> getPendingDialogsForTest(@ModalDialogType int dialogType) {
        return mPendingDialogs.get(dialogType);
    }

    @VisibleForTesting
    Presenter getPresenterForTest(@ModalDialogType int dialogType) {
        return mPresenters.get(dialogType);
    }

    @VisibleForTesting
    Presenter getCurrentPresenterForTest() {
        return mCurrentPresenter;
    }
}
