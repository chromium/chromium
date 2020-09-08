// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.interstitial;

import android.view.View;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class regroups the bindView util methods of the
 * incognito interstitial.
 */
class IncognitoInterstitialViewBinder {
    private IncognitoInterstitialViewBinder() {}

    static void bindView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == IncognitoInterstitialProperties.ON_LEARN_MORE_CLICKED) {
            getLearnMoreView(view).setOnClickListener(
                    v -> model.get(IncognitoInterstitialProperties.ON_LEARN_MORE_CLICKED).run());
        } else if (propertyKey == IncognitoInterstitialProperties.ON_CONTINUE_CLICKED) {
            getContinueButtonView(view).setOnClickListener(
                    v -> model.get(IncognitoInterstitialProperties.ON_CONTINUE_CLICKED).run());
        } else {
            throw new IllegalArgumentException(
                    "Cannot update the view for propertyKey: " + propertyKey);
        }
    }

    // Necessary helper methods to return the subviews present inside the incognito
    // interstitial |view|.
    // TODO(crbug.com/1103262): Add these methods to the IncognitoInterstitialView once we implement
    // it in future.
    private static View getContinueButtonView(View view) {
        return view.findViewById(R.id.incognito_interstitial_continue_button);
    }

    private static View getLearnMoreView(View view) {
        return view.findViewById(R.id.incognito_interstitial_learn_more);
    }
}