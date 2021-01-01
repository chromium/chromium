// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.interstitial;

import android.view.View;

import androidx.annotation.MainThread;

import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator of the incognito interstitial along with IncognitoInterstitialDelegate are the
 * only public class in this package.
 *
 * It is responsible for setting up the incognito interstitial view and model and it serves as an
 * access point for users of this package.
 */
public class IncognitoInterstitialCoordinator {
    /**
     * Constructs an IncognitoInterstitialCoordinator object.
     *
     * @param view The incognito interstitial view.
     * @param incognitoInterstitialDelegate A delegate providing the functionality of the Incognito
     *         interstitial.
     * @param onIncognitoTabOpened Runnable to be called when an incognito tab is opened.
     */
    @MainThread
    public IncognitoInterstitialCoordinator(View view,
            IncognitoInterstitialDelegate incognitoInterstitialDelegate,
            Runnable onIncognitoTabOpened) {
        IncognitoInterstitialViewBinder.setUpView(view);
        IncognitoInterstitialMediator mediator = new IncognitoInterstitialMediator(
                incognitoInterstitialDelegate, onIncognitoTabOpened);
        PropertyModelChangeProcessor.create(
                mediator.getModel(), view, IncognitoInterstitialViewBinder::bindView);
    }
}
