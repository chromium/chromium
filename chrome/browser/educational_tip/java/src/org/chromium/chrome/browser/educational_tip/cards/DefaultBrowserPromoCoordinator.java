// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;

/** Coordinator for the default browser promo card. */
public class DefaultBrowserPromoCoordinator implements EducationalTipCardProvider {
    private final Context mContext;

    // For the default browser promo card specifically, it is triggered only when the user clicks on
    // the bottom sheet, directing them to the default app settings page.
    private final Runnable mOnModuleClickedCallback;

    public DefaultBrowserPromoCoordinator(
            @NonNull Context context, @NonNull Runnable onModuleClickedCallback) {
        mContext = context;
        mOnModuleClickedCallback = onModuleClickedCallback;
    }

    @Override
    public String getCardTitle() {
        return mContext.getString(
                org.chromium.chrome.browser.educational_tip.R.string
                        .educational_tip_default_browser_title);
    }

    @Override
    public String getCardDescription() {
        return mContext.getString(
                org.chromium.chrome.browser.educational_tip.R.string
                        .educational_tip_default_browser_description);
    }

    @Override
    public int getCardImage() {
        return org.chromium.chrome.browser.educational_tip.R.drawable.default_browser_promo_logo;
    }

    @Override
    public void onCardClicked() {
        mOnModuleClickedCallback.run();
    }
}
