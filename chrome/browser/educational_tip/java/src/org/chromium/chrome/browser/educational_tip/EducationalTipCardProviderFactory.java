// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import androidx.annotation.NonNull;

import org.chromium.base.CallbackController;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider.EducationalTipCardType;
import org.chromium.chrome.browser.educational_tip.cards.DefaultBrowserPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.QuickDeletePromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.TabGroupPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.TabGroupSyncPromoCoordinator;

/** A factory interface for building a EducationalTipCardProvider instance. */
public class EducationalTipCardProviderFactory {
    /**
     * @return An instance of EducationalTipCardProvider.
     */
    static EducationalTipCardProvider createInstance(
            @EducationalTipCardType int cardType,
            @NonNull Runnable onModuleClickedCallback,
            @NonNull CallbackController callbackController,
            @NonNull EducationTipModuleActionDelegate actionDelegate) {
        switch (cardType) {
            case EducationalTipCardType.DEFAULT_BROWSER_PROMO:
                return new DefaultBrowserPromoCoordinator(onModuleClickedCallback, actionDelegate);
            case EducationalTipCardType.TAB_GROUPS:
                return new TabGroupPromoCoordinator(
                        onModuleClickedCallback, callbackController, actionDelegate);
            case EducationalTipCardType.TAB_GROUP_SYNC:
                return new TabGroupSyncPromoCoordinator(
                        onModuleClickedCallback, callbackController, actionDelegate);
            case EducationalTipCardType.QUICK_DELETE:
                return new QuickDeletePromoCoordinator(
                        onModuleClickedCallback, callbackController, actionDelegate);
            default:
                assert false : "Educational tip module's card type not supported!";
                return null;
        }
    }
}
