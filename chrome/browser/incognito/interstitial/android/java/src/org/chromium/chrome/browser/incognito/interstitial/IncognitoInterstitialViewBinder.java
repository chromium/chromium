// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.interstitial;

import android.text.style.StyleSpan;
import android.view.View;
import android.widget.TextView;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.SpanApplier;

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

    /**
     * Sets up the incognito interstitial view.
     */
    static void setUpView(View view) {
        TextView message = view.findViewById(R.id.incognito_interstitial_message);
        message.setText(SpanApplier.applySpans(message.getText().toString(),
                new SpanApplier.SpanInfo(
                        "<b1>", "</b1>", new StyleSpan(android.graphics.Typeface.BOLD)),
                new SpanApplier.SpanInfo(
                        "<b2>", "</b2>", new StyleSpan(android.graphics.Typeface.BOLD))));
    }

    // Necessary helper methods to return the subviews present inside the incognito
    // interstitial |view|.
    private static View getContinueButtonView(View view) {
        return view.findViewById(R.id.incognito_interstitial_continue_button);
    }

    private static View getLearnMoreView(View view) {
        return view.findViewById(R.id.incognito_interstitial_learn_more);
    }
}
