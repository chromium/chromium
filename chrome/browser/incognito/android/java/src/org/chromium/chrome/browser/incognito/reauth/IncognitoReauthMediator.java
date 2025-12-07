// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.incognito.reauth;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager.IncognitoReauthCallback;

/** The mediator responsible for handling the interactions with the incognito re-auth view. */
@NullMarked
class IncognitoReauthMediator {
    private final Runnable mShowTabSwitcherRunnable;
    // The entity responsible for actually calling the underlying system re-authentication.
    private final IncognitoReauthManager mIncognitoReauthManager;
    // The callback that would be fired after an authentication attempt.
    private final IncognitoReauthCallback mIncognitoReauthCallback;

    /**
     * @param incognitoReauthCallback incognitoReauthCallback The {@link IncognitoReauthCallback}
     *     which would be executed after an authentication attempt.
     * @param incognitoReauthManager The {@link IncognitoReauthManager} instance which would be used
     *     to initiate re-authentication.
     * @param showTabSwitcherRunnable A {link Runnable} to show the tab switcher UI.
     */
    IncognitoReauthMediator(
            IncognitoReauthCallback incognitoReauthCallback,
            IncognitoReauthManager incognitoReauthManager,
            Runnable showTabSwitcherRunnable) {
        mIncognitoReauthCallback = incognitoReauthCallback;
        mIncognitoReauthManager = incognitoReauthManager;
        mShowTabSwitcherRunnable = showTabSwitcherRunnable;
    }

    void onUnlockIncognitoButtonClicked() {
        mIncognitoReauthManager.startReauthenticationFlow(mIncognitoReauthCallback);
    }

    void onSeeOtherTabsButtonClicked() {
        // We observe {@link TabModel} changes in {@link IncognitoReauthController} and when we
        // will change to regular {@link TabModel} the controller will detect that and hide
        // the dialog.
        mShowTabSwitcherRunnable.run();
    }
}
