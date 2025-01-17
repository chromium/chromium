// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import androidx.annotation.NonNull;

import org.chromium.base.CallbackController;
import org.chromium.chrome.browser.educational_tip.cards.DefaultBrowserPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.QuickDeletePromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.TabGroupPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.TabGroupSyncPromoCoordinator;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;

/** A factory interface for building a EducationalTipCardProvider instance. */
public class EducationalTipCardProviderFactory {
    /**
     * @return An instance of EducationalTipCardProvider.
     */
    static EducationalTipCardProvider createInstance(
            @ModuleType int moduleType,
            @NonNull Runnable onModuleClickedCallback,
            @NonNull CallbackController callbackController,
            @NonNull EducationTipModuleActionDelegate actionDelegate) {
        switch (moduleType) {
            case ModuleType.DEFAULT_BROWSER_PROMO:
                return new DefaultBrowserPromoCoordinator(onModuleClickedCallback, actionDelegate);
            case ModuleType.TAB_GROUP_PROMO:
                return new TabGroupPromoCoordinator(
                        onModuleClickedCallback, callbackController, actionDelegate);
            case ModuleType.TAB_GROUP_SYNC_PROMO:
                return new TabGroupSyncPromoCoordinator(
                        onModuleClickedCallback, callbackController, actionDelegate);
            case ModuleType.QUICK_DELETE_PROMO:
                return new QuickDeletePromoCoordinator(
                        onModuleClickedCallback, callbackController, actionDelegate);
            default:
                assert false : "Educational tip module's card type not supported!";
                return null;
        }
    }
}
