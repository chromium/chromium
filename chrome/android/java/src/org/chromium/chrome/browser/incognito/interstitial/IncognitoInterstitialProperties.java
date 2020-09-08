// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.interstitial;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Properties for account picker.
 */
class IncognitoInterstitialProperties {
    private IncognitoInterstitialProperties() {}

    // PropertyKey for the button |Learn more|
    static final PropertyModel.ReadableObjectPropertyKey<Runnable> ON_LEARN_MORE_CLICKED =
            new PropertyModel.ReadableObjectPropertyKey<>("on_learn_more_clicked");

    // PropertyKey for the button |Continue|
    static final PropertyModel.ReadableObjectPropertyKey<Runnable> ON_CONTINUE_CLICKED =
            new PropertyModel.ReadableObjectPropertyKey<>("on_continue_clicked");

    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {ON_LEARN_MORE_CLICKED, ON_CONTINUE_CLICKED};

    static PropertyModel createModel(Runnable onLearnMoreClicked, Runnable onContinueClicked) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(ON_LEARN_MORE_CLICKED, onLearnMoreClicked)
                .with(ON_CONTINUE_CLICKED, onContinueClicked)
                .build();
    }
}
