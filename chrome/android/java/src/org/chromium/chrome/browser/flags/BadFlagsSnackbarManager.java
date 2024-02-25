// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.ui.base.WindowAndroid;

/** This class provides for native code to manage a Bad Flags {@link Snackbar}. */
@JNINamespace("chrome")
public class BadFlagsSnackbarManager {
    /**
     * Show the snackbar.
     *
     * @param windowAndroid The {@link WindowAndroid} for the current activity.
     * @param message Message to be shown in the snackbar.
     */
    @CalledByNative
    public static void show(WindowAndroid windowAndroid, String message) {
        createSnackbar(message, SnackbarManagerProvider.from(windowAndroid));
    }

    @VisibleForTesting
    static void createSnackbar(String message, SnackbarManager snackbarManager) {
        if (snackbarManager == null) return;
        Snackbar snackBar =
                Snackbar.make(message, null, Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_BAD_FLAGS);
        snackBar.setSingleLine(false);
        snackBar.setDuration(SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS);
        snackbarManager.showSnackbar(snackBar);
    }
}
