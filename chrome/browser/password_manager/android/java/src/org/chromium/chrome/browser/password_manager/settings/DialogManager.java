// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This class manages a {@link DialogFragment}.
 * In particular, it ensures that the dialog stays visible for a minimum time period, so that
 * earlier calls to hide it are delayed appropriately. It also allows to override the delaying for
 * testing purposes.
 */
public final class DialogManager {
    /**
     * Contains the reference to a {@link android.app.DialogFragment} between the call to {@link
     * show} and dismissing the dialog.
     */
    @Nullable private DialogFragment mDialogFragment;

    /**
     * The least amout of time for which {@link mDialogFragment} should stay visible to avoid
     * flickering. It was chosen so that it is enough to read the approx. 3 words on it, but not too
     * long (to avoid the user waiting while Chrome is already idle).
     */
    private static final long MINIMUM_LIFE_SPAN_MILLIS = 1000L;

    /** This is used to post the unblocking signal for hiding the dialog fragment. */
    private CallbackDelayer mDelayer = new TimedCallbackDelayer(MINIMUM_LIFE_SPAN_MILLIS);

    /** Allows to fake the timed delayer. */
    public void replaceCallbackDelayerForTesting(CallbackDelayer newDelayer) {
        mDelayer = newDelayer;
    }

    /**
     * Used to gate hiding of a dialog on two actions: one automatic delayed signal and one manual
     * call to {@link hide}. This is not null between the calls to {@link show} and {@link hide}.
     */
    @Nullable private SingleThreadBarrierClosure mBarrierClosure;

    /** Callback to run after the dialog was hidden. Can be null if no hiding was requested.*/
    @Nullable private Runnable mCallback;

    private boolean mShowingRequested;

    /** Possible actions taken on the dialog during {@link #hide}. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({HideActions.NO_OP, HideActions.HIDDEN_IMMEDIATELY, HideActions.HIDING_DELAYED})
    public @interface HideActions {
        /** The dialog has not been shown, so it is not being hidden. */
        int NO_OP = 0;

        /** {@link #mBarrierClosure} was signalled so the dialog is hidden now. */
        int HIDDEN_IMMEDIATELY = 1;

        /** The hiding is being delayed until {@link #mBarrierClosure} is signalled further. */
        int HIDING_DELAYED = 2;
    }

    /** Interface to notify, during @{link #hide}, which action was taken. */
    public interface ActionsConsumer {
        void consume(@HideActions int action);
    }

    /** The callback called everytime {@link #hide} is executed. */
    @Nullable private final ActionsConsumer mActionsConsumer;

    /**
     * Constructs a DialogManager, optionally with a callback to report which action was taken on
     * hiding.
     * @param actionsConsumer The callback called everytime {@link #hide} is executed.
     */
    public DialogManager(@Nullable ActionsConsumer actionsConsumer) {
        mActionsConsumer = actionsConsumer;
    }

    /**
     * Shows the dialog after the specified delay.
     *
     * @param dialog to be shown.
     * @param fragmentManager needed to call {@link android.app.DialogFragment#show}.
     * @param delay the delay in ms after which the dialog will be displayed (if not canceled during
     *         this delay).
     */
    public void showWithDelay(DialogFragment dialog, FragmentManager fragmentManager, int delay) {
        mShowingRequested = true;
        new TimedCallbackDelayer(delay)
                .delay(
                        () -> {
                            // hide() might have been called during the delay.
                            if (mShowingRequested) {
                                show(dialog, fragmentManager);
                            }
                        });
    }

    /**
     * Shows the dialog.
     * @param dialog to be shown.
     * @param fragmentManager needed to call {@link android.app.DialogFragment#show}
     */
    public void show(DialogFragment dialog, FragmentManager fragmentManager) {
        mShowingRequested = true;
        mDialogFragment = dialog;
        mDialogFragment.show(fragmentManager, null);
        // Initiate the barrier closure, expecting 2 runs: one automatic but delayed, and one
        // explicit, to hide the dialog.
        mBarrierClosure = new SingleThreadBarrierClosure(2, this::hideImmediately);
        // This is the automatic but delayed signal.
        mDelayer.delay(mBarrierClosure);
    }

    /**
     * Hides the dialog as soon as possible, but not sooner than {@link MINIMUM_LIFE_SPAN_MILLIS}
     * milliseconds after it was shown. Attempts to hide the dialog when none is shown are
     * gracefully ignored but the callback is called in any case.
     * @param callback is asynchronously called as soon as the dialog is no longer visible.
     */
    public void hide(Runnable callback) {
        if (mActionsConsumer != null) {
            @HideActions final int action;
            if (mBarrierClosure == null) {
                action = HideActions.NO_OP;
            } else if (mBarrierClosure.isReady()) {
                action = HideActions.HIDDEN_IMMEDIATELY;
            } else {
                action = HideActions.HIDING_DELAYED;
            }
            mActionsConsumer.consume(action);
        }
        mCallback = callback;
        // The barrier closure is null if the dialog was not shown. In that case don't wait before
        // confirming the hidden state.
        if (mBarrierClosure == null) {
            hideImmediately();
        } else {
            mBarrierClosure.run();
        }
    }

    /**
     * Synchronously hides the dialog without any delay. Attempts to hide the dialog when
     * none is shown are gracefully ignored but |mCallback| is called in any case if present.
     */
    private void hideImmediately() {
        if (mDialogFragment != null) mDialogFragment.dismiss();
        // Post the callback to ensure that it is always run asynchronously, even if hide() took a
        // shortcut for a missing shown().
        if (mCallback != null) PostTask.postTask(TaskTraits.UI_DEFAULT, mCallback);
        reset();
    }

    /** Resets the dialog reference and metadata related to it.*/
    private void reset() {
        mDialogFragment = null;
        mCallback = null;
        mBarrierClosure = null;
        mShowingRequested = false;
    }
}
