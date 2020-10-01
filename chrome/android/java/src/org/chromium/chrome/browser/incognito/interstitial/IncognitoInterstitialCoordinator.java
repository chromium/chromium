// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.interstitial;

import android.text.style.StyleSpan;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.MainThread;

import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.text.SpanApplier;

/**
 * The coordinator of the incognito interstitial along with IncognitoInterstitialDelegate are the
 * only public class in this package.
 *
 * It is responsible for setting up the incognito interstitial view and model and it serves as an
 * access point for users of this package.
 */
public class IncognitoInterstitialCoordinator {
    /**
     * TODO(crbug.com/1103262): Instead of passing a general View element pass an
     * IncognitoInterstitialView.
     *
     * Constructs an IncognitoInterstitialCoordinator object.
     *
     * @param view The incognito interstitial view.
     * @param incognitoInterstitialDelegate A delegate providing the functionality of the Incognito
     *         interstitial.
     */
    @MainThread
    public IncognitoInterstitialCoordinator(
            View view, IncognitoInterstitialDelegate incognitoInterstitialDelegate) {
        formatIncognitoInterstitialMessage(view);
        IncognitoInterstitialMediator mediator =
                new IncognitoInterstitialMediator(incognitoInterstitialDelegate);
        PropertyModelChangeProcessor.create(
                mediator.getModel(), view, IncognitoInterstitialViewBinder::bindView);
    }

    private static void formatIncognitoInterstitialMessage(View incognitoInterstitialView) {
        TextView incognitoInterstitialMessageView =
                incognitoInterstitialView.findViewById(R.id.incognito_interstitial_message);

        String incognitoInterstitialMessageText =
                incognitoInterstitialMessageView.getText().toString();

        incognitoInterstitialMessageView.setText(
                SpanApplier.applySpans(incognitoInterstitialMessageText,
                        new SpanApplier.SpanInfo(
                                "<b1>", "</b1>", new StyleSpan(android.graphics.Typeface.BOLD)),
                        new SpanApplier.SpanInfo(
                                "<b2>", "</b2>", new StyleSpan(android.graphics.Typeface.BOLD))));
    }
}
