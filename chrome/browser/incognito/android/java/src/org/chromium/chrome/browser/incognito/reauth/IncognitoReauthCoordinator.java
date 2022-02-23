// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * The coordinator which is responsible for showing the Incognito re-authentication page.
 *
 * TODO(crbug.com/1227656):  Add View inflation and set up the re-auth dialog.
 */
class IncognitoReauthCoordinator {
    private final @NonNull Context mContext;
    private final @NonNull ModalDialogManager mModalDialogManager;
    private final @NonNull IncognitoReauthManager.IncognitoReauthCallback mIncognitoReauthCallback;
    private final boolean mShowFullScreen;

    /**
     * @param context The {@link Context} to use for inflating the Incognito re-auth view.
     * @param tabModelSelector The {@link TabModelSelector} which will be passed to the mediator in
     *         order to switch {@link TabModel} when the user clicks on "See other tabs" button.
     * @param modalDialogManager The {@link ModalDialogManager} which is used to fire the dialog
     *         containing the Incognito re-auth view.
     * @param incognitoReauthCallback The {@link IncognitoReauthCallback} which would be executed
     *         after an authentication attempt.
     * @param showFullScreen Whether to show a fullscreen / tab based re-auth dialog.
     */
    public IncognitoReauthCoordinator(@NonNull Context context,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull IncognitoReauthManager.IncognitoReauthCallback incognitoReauthCallback,
            boolean showFullScreen) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mIncognitoReauthCallback = incognitoReauthCallback;
        mShowFullScreen = showFullScreen;
    }

    void showDialog() {}

    void hideDialogAndDestroy(@DialogDismissalCause int dismissalCause) {
        destroy();
    }

    private void destroy() {}
}
