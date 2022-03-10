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
    private final IncognitoReauthManager mIncognitoReauthManager = new IncognitoReauthManager();
    // The callback that would be fired after an authentication attempt.
    private IncognitoReauthCallback mIncognitoReauthCallback;

    /**
     * @param tabModelSelector The {@link TabModelSelector} which helps to switch to regular
     *        {@link TabModel}.
     */
    IncognitoReauthMediator(@NonNull TabModelSelector tabModelSelector,
            @NonNull IncognitoReauthCallback incognitoReauthCallback) {
        mTabModelSelector = tabModelSelector;
        mIncognitoReauthCallback = incognitoReauthCallback;
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
