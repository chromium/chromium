// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;
import org.chromium.chrome.browser.educational_tip.cards.DefaultBrowserPromoCoordinator;

/** A factory interface for building a EducationalTipCardProvider instance. */
public class EducationalTipCardProviderFactory {
    /**
     * @return An instance of EducationalTipCardProvider.
     */
    static EducationalTipCardProvider createInstance(
            @NonNull Context context,
            @EducationalTipCardType int cardType,
            @NonNull Runnable onModuleClickedCallback) {
        if (cardType == EducationalTipCardType.DEFAULT_BROWSER_PROMO) {
            return new DefaultBrowserPromoCoordinator(context, onModuleClickedCallback);
        }

        assert false : "Educational tip module's card type not supported!";
        return null;
    }
}
