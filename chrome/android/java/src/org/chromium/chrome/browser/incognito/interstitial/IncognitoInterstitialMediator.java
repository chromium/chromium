// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.interstitial;

import androidx.annotation.MainThread;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * The mediator of incognito interstitial handles all the signals from the outside world.
 *
 * It defines the business logic when the user clicks on "Learn more" or "Continue" button.
 */
class IncognitoInterstitialMediator {
    private final PropertyModel mModel;
    private final IncognitoInterstitialDelegate mIncognitoInterstitialDelegate;
    private final Runnable mOnIncognitoTabOpened;

    IncognitoInterstitialMediator(IncognitoInterstitialDelegate incognitoInterstitialDelegate,
            Runnable onIncognitoTabOpened) {
        mModel = IncognitoInterstitialProperties.createModel(
                this::onLearnMoreClicked, this::onContinueClicked);
        mIncognitoInterstitialDelegate = incognitoInterstitialDelegate;
        mOnIncognitoTabOpened = onIncognitoTabOpened;
    }

    PropertyModel getModel() {
        return mModel;
    }

    /**
     * Callback for {@link IncognitoInterstitialProperties#ON_LEARN_MORE_CLICKED}
     */
    @MainThread
    private void onLearnMoreClicked() {
        mIncognitoInterstitialDelegate.openLearnMorePage();
    }

    /**
     * Callback for {@link IncognitoInterstitialProperties#ON_CONTINUE_CLICKED}
     */
    @MainThread
    private void onContinueClicked() {
        mOnIncognitoTabOpened.run();
        mIncognitoInterstitialDelegate.openCurrentUrlInIncognitoTab();
    }
}
