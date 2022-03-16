// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.incognito.reauth;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager.IncognitoReauthCallback;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * The mediator responsible for handling the interactions with the incognito re-auth view.
 */
class IncognitoReauthMediator {
    private final TabModelSelector mTabModelSelector;
    // The entity responsible for actually calling the underlying system re-authentication.
    private final IncognitoReauthManager mIncognitoReauthManager;
    // The callback that would be fired after an authentication attempt.
    private IncognitoReauthCallback mIncognitoReauthCallback;

    /**
     * @param tabModelSelector The {@link TabModelSelector} which helps to switch to regular
     *        {@link TabModel}.
     * @param incognitoReauthCallback incognitoReauthCallback The {@link IncognitoReauthCallback}
     *                               which would be executed after an authentication attempt.
     * @param incognitoReauthManager The {@link IncognitoReauthManager} instance which would be
     *                               used to initiate re-authentication.
     */
    IncognitoReauthMediator(@NonNull TabModelSelector tabModelSelector,
            @NonNull IncognitoReauthCallback incognitoReauthCallback,
            @NonNull IncognitoReauthManager incognitoReauthManager) {
        mTabModelSelector = tabModelSelector;
        mIncognitoReauthCallback = incognitoReauthCallback;
        mIncognitoReauthManager = incognitoReauthManager;
    }

    void onUnlockIncognitoButtonClicked() {
        mIncognitoReauthManager.startReauthenticationFlow(mIncognitoReauthCallback);
    }

    void onSeeOtherTabsButtonClicked() {
        // We observe {@link TabModel} changes in {@link IncognitoReauthController} and when we
        // will change to regular {@link TabModel} the controller will detect that and hide
        // the dialog.
        mTabModelSelector.selectModel(/*incognito=*/false);
    }
}
