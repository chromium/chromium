// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import android.content.Context;

import org.chromium.base.CancelableRunnable;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.content_public.browser.ImmersivePlaybackConfirmationStatus;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/**
 * Controller for the immersive playback confirmation snackbar. It handles the snackbar interactions
 * and shows the format selection dialog if the user clicks "Yes".
 */
@NullMarked
public class ImmersivePlaybackSnackbarController implements SnackbarManager.SnackbarController {
    private final Context mContext;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
    private final Tab mTab;
    private final @Nullable FullscreenManager mFullscreenManager;
    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onPageLoadStarted(Tab tab, GURL url) {
                    dismiss();
                }

                @Override
                public void onContentChanged(Tab tab) {
                    dismiss();
                }
            };
    private final FullscreenManager.Observer mFullscreenObserver =
            new FullscreenManager.Observer() {
                @Override
                public void onExitFullscreen(Tab tab) {
                    dismiss();
                }
            };
    private @Nullable ImmersivePlaybackConfirmationCallback mCallback;
    private @Nullable ImmersiveVideoFormatSelectionDialog mDialog;
    private @Nullable CancelableRunnable mPendingShowTask;
    private boolean mAreObserversRegistered;

    public ImmersivePlaybackSnackbarController(
            Context context,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            Tab tab,
            @Nullable FullscreenManager fullscreenManager) {
        mContext = context;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mTab = tab;
        mFullscreenManager = fullscreenManager;
    }

    /**
     * Shows the snackbar after a delay.
     *
     * @param callback Callback to be invoked when the snackbar is dismissed.
     * @param delayMs Delay before showing the snackbar.
     */
    public void show(ImmersivePlaybackConfirmationCallback callback, long delayMs) {
        dismiss();
        mCallback = callback;

        if (mModalDialogManagerSupplier.get() == null) {
            reportResultAndReset(
                    ImmersivePlaybackConfirmationStatus.FAILED,
                    ImmersiveStereoMode.MONO,
                    ImmersiveProjectionType.QUAD);
            return;
        }

        registerObservers();
        showSnackbarDelayed(delayMs);
    }

    /** Dismisses the snackbar and dialog if showing. */
    public void dismiss() {
        cancelPendingShowTask();
        unregisterObservers();
        mSnackbarManagerSupplier.get().dismissSnackbars(this);
        if (mDialog != null) {
            mDialog.dismiss();
            mDialog = null;
        }
    }

    @Override
    public void onAction(@Nullable Object actionData) {
        unregisterObservers();
        ModalDialogManager modalDialogManager = mModalDialogManagerSupplier.get();
        if (modalDialogManager == null) {
            reportResultAndReset(
                    ImmersivePlaybackConfirmationStatus.FAILED,
                    ImmersiveStereoMode.MONO,
                    ImmersiveProjectionType.QUAD);
            return;
        }

        showDialog(modalDialogManager);
    }

    @Override
    public void onDismissNoAction(@Nullable Object actionData) {
        unregisterObservers();
        reportResultAndReset(
                ImmersivePlaybackConfirmationStatus.DECLINED,
                ImmersiveStereoMode.MONO,
                ImmersiveProjectionType.QUAD);
    }

    private void showSnackbar() {
        cancelPendingShowTask();
        String message = mContext.getString(R.string.immersive_playback_confirmation_message);
        Snackbar snackbar =
                Snackbar.make(message, this, Snackbar.TYPE_ACTION, Snackbar.UMA_UNKNOWN);
        snackbar.setAction(mContext.getString(R.string.immersive_playback_confirmation_yes), null);
        mSnackbarManagerSupplier.get().showSnackbar(snackbar);
    }

    private void showSnackbarDelayed(long delayMs) {
        if (delayMs <= 0) {
            showSnackbar();
        } else {
            mPendingShowTask = new CancelableRunnable(this::showSnackbar);
            PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, mPendingShowTask, delayMs);
        }
    }

    private void showDialog(ModalDialogManager modalDialogManager) {
        mDialog =
                new ImmersiveVideoFormatSelectionDialog(
                        mContext, modalDialogManager, this::reportResultAndReset);
        mDialog.show();
    }

    private void registerObservers() {
        if (mAreObserversRegistered) return;
        mAreObserversRegistered = true;

        mTab.addObserver(mTabObserver);
        if (mFullscreenManager != null) {
            mFullscreenManager.addObserver(mFullscreenObserver);
        }
    }

    private void unregisterObservers() {
        if (!mAreObserversRegistered) return;
        mAreObserversRegistered = false;

        mTab.removeObserver(mTabObserver);
        if (mFullscreenManager != null) {
            mFullscreenManager.removeObserver(mFullscreenObserver);
        }
    }

    private void cancelPendingShowTask() {
        if (mPendingShowTask != null) {
            mPendingShowTask.cancel();
            mPendingShowTask = null;
        }
    }

    private void reportResultAndReset(
            @ImmersivePlaybackConfirmationStatus int status,
            @ImmersiveStereoMode int stereoMode,
            @ImmersiveProjectionType int projectionType) {
        cancelPendingShowTask();
        mDialog = null;
        if (mCallback != null) {
            mCallback.onResult(status, stereoMode, projectionType);
            mCallback = null;
        }
    }
}
