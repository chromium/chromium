// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;

import java.util.HashSet;

/**
 * Provides information about the trigger states of cards in the educational tip module.
 *
 * <p>This class serves as a single educational tip module's cards trigger logic gateway.
 */
public class EducationalTipCardProviderTriggerState {
    /**
     * A list includes all card types (excluding the default browser promo card) that have been
     * displayed to the user during the current session.
     */
    private final HashSet<Integer> mVisibleCardList;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static EducationalTipCardProviderTriggerState sInstance =
                new EducationalTipCardProviderTriggerState();
    }

    /** Returns the singleton instance of EducationalTipCardProviderTriggerState. */
    public static EducationalTipCardProviderTriggerState getInstance() {
        return EducationalTipCardProviderTriggerState.LazyHolder.sInstance;
    }

    EducationalTipCardProviderTriggerState() {
        mVisibleCardList = new HashSet<>();
    }

    boolean shouldNotifyCardShownPerSession(@NonNull @EducationalTipCardType Integer cardType) {
        // Ensure that the default browser promo card does not trigger this function.
        assert cardType < EducationalTipCardType.NUM_ENTRIES && cardType > 0;

        if (mVisibleCardList.contains(cardType)) {
            return false;
        }

        return mVisibleCardList.add(cardType);
    }
}
