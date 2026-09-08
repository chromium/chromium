// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.base.DeviceInput;

/**
 * This class is the Java counterpart of ExclusiveAccessBubbleAndroid. It owns the notification
 * snackbar for the exclusive access APIs (fullscreen, keyboard lock & pointer lock)
 */
@NullMarked
public class ExclusiveAccessBubble {
    private static final int EXCLUSIVE_ACCESS_SNACKBAR_DURATION_MS = 3800;

    private final ExclusiveAccessContext mParentContext;
    private @Nullable Snackbar mSnackbar;
    private final SnackbarManager.SnackbarController mSnackbarController =
            new SnackbarManager.SnackbarController() {
                @Override
                public void onDismissNoAction(@Nullable Object actionData) {
                    mSnackbar = null;
                }
            };

    private @Nullable String mText;

    private ExclusiveAccessBubble(ExclusiveAccessContext parentContext) {
        mParentContext = parentContext;
    }

    @CalledByNative
    public static ExclusiveAccessBubble create(ExclusiveAccessContext context) {
        return new ExclusiveAccessBubble(context);
    }

    @CalledByNative
    public void show() {
        if (mText == null || mSnackbar != null) return;
        SnackbarManager snackbarManager = mParentContext.getSnackbarManager();
        if (snackbarManager == null) return;

        mSnackbar =
                Snackbar.make(
                                mText,
                                mSnackbarController,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_EXCLUSIVE_ACCESS_BUBBLE)
                        // The exclusive access notice is security-critical and should not
                        // be discarded by the timeout of other action snackbars in the queue.
                        .setHighPriority(true)
                        // Use a Java-side timeout so that the timer only starts when
                        // the notice is actually visible to the user.
                        .setDuration(EXCLUSIVE_ACCESS_SNACKBAR_DURATION_MS);
        snackbarManager.showSnackbar(mSnackbar);
    }

    @CalledByNative
    public void update(String text) {
        if (mText != null && mText.equals(text) && mSnackbar != null) return;
        mText = text;

        SnackbarManager snackbarManager = mParentContext.getSnackbarManager();
        if (snackbarManager != null && mSnackbar != null) {
            mSnackbar =
                    Snackbar.make(
                                    text,
                                    mSnackbarController,
                                    Snackbar.TYPE_ACTION,
                                    Snackbar.UMA_EXCLUSIVE_ACCESS_BUBBLE)
                            .setHighPriority(true)
                            .setDuration(EXCLUSIVE_ACCESS_SNACKBAR_DURATION_MS);
            // This will trigger SnackbarManager.updateView() and update the existing view.
            snackbarManager.showSnackbar(mSnackbar);
            return;
        }
        hide();
        show();
    }

    @CalledByNative
    public void hide() {
        SnackbarManager snackbarManager = mParentContext.getSnackbarManager();
        if (snackbarManager != null && mSnackbar != null) {
            snackbarManager.dismissSnackbars(mSnackbarController);
        }
        mSnackbar = null;
    }

    @CalledByNative
    public boolean isVisible() {
        return mSnackbar != null;
    }

    @CalledByNative
    public boolean isKeyboardConnected() {
        return DeviceInput.supportsAlphabeticKeyboard();
    }
}
