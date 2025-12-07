// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;

import org.jni_zero.CalledByNative;

import org.chromium.base.JniOnceCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.ui.base.WindowAndroid;

@NullMarked
public class DownloadMessageBridge {
    @CalledByNative
    public static void showIncognitoDownloadMessage(JniOnceCallback<Boolean> callback) {
        DownloadMessageUiController messageUiController =
                DownloadManagerService.getDownloadManagerService()
                        .getMessageUiController(/* otrProfileId= */ null);
        messageUiController.showIncognitoDownloadMessage(callback);
    }

    @CalledByNative
    public static void showUnsupportedDownloadMessage(WindowAndroid window) {
        SnackbarManager snackbarManager = SnackbarManagerProvider.from(window);
        if (snackbarManager == null) return;

        Context context = window.getContext().get();
        assumeNonNull(context);
        Snackbar snackbar =
                Snackbar.make(
                        context.getString(R.string.download_file_type_not_supported),
                        null,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_AUTO_LOGIN);
        snackbar.setAction(context.getString(R.string.ok), null);
        snackbar.setDefaultLines(false);
        snackbarManager.showSnackbar(snackbar);
    }
}
