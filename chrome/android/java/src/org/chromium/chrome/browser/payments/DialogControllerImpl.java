// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Context;
import android.content.DialogInterface.OnClickListener;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.components.payments.DialogController;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** The implementation of dialog display controller for PaymentRequest using AlertDialog. */
/* package */ class DialogControllerImpl implements DialogController {
    private final WebContents mWebContents;

    /**
     * Constructs the dialog display controller for PaymentRequest.
     *
     * @param webContents The web contents where PaymentRequest API was invoked. Should not be null.
     */
    /* package */ DialogControllerImpl(WebContents webContents) {
        assert webContents != null;

        mWebContents = webContents;
    }

    // Implement DialogController:
    @Override
    public void showReadyToPayDebugInfo(String readyToPayDebugInfo) {
        assert readyToPayDebugInfo != null;

        Context context = getActivityContextForLiveVisibleWebContents();
        if (context == null) {
            return;
        }
        new AlertDialog.Builder(context, R.style.ThemeOverlay_BrowserUI_AlertDialog)
                .setMessage(readyToPayDebugInfo)
                .show();
    }

    // Implement DialogController:
    @Override
    public void showLeavingIncognitoWarning(
            Callback<String> denyCallback, Runnable approveCallback) {
        assert denyCallback != null;
        assert approveCallback != null;

        Context context = getActivityContextForLiveVisibleWebContents();
        if (context == null) {
            denyCallback.onResult(ErrorStrings.ACTIVITY_NOT_FOUND);
            return;
        }

        new AlertDialog.Builder(context, R.style.ThemeOverlay_BrowserUI_AlertDialog)
                .setTitle(R.string.external_app_leave_incognito_warning_title)
                .setMessage(R.string.external_payment_app_leave_incognito_warning)
                .setPositiveButton(
                        R.string.ok, (OnClickListener) (dialog, which) -> approveCallback.run())
                .setNegativeButton(
                        R.string.cancel,
                        (OnClickListener)
                                (dialog, which) ->
                                        denyCallback.onResult(ErrorStrings.USER_CANCELLED))
                .setOnCancelListener(dialog -> denyCallback.onResult(ErrorStrings.USER_CANCELLED))
                .show();
    }

    /**
     * @return The Activity context of the web contents where PaymentRequest was invoked, if these
     *     web contents are not being destroyed or hidden. (Otherwise null.)
     */
    @Nullable
    private Context getActivityContextForLiveVisibleWebContents() {
        if (mWebContents.isDestroyed() || mWebContents.getVisibility() != Visibility.VISIBLE) {
            return null;
        }

        WindowAndroid window = mWebContents.getTopLevelNativeWindow();
        return window == null ? null : window.getActivity().get();
    }
}
