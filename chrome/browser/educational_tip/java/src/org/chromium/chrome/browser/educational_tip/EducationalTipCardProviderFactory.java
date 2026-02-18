// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.cards.AddressBarPlacementPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.DefaultBrowserPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.EnhancedSafeBrowsingPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.HistorySyncPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.PasswordCheckupPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.QuickDeletePromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.SavePasswordsPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.SetupListCelebratoryPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.SignInPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.TabGroupPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.TabGroupSyncPromoCoordinator;
import org.chromium.chrome.browser.educational_tip.cards.TipsNotificationsPromoCoordinator;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;

/** A factory interface for building a EducationalTipCardProvider instance. */
@NullMarked
public class EducationalTipCardProviderFactory {
    /**
     * @return An instance of EducationalTipCardProvider.
     */
    public static @Nullable EducationalTipCardProvider createInstance(
            @ModuleType int moduleType,
            Runnable onModuleClickedCallback,
            CallbackController callbackController,
            EducationTipModuleActionDelegate actionDelegate,
            Runnable removeModuleCallback) {
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
            case ModuleType.HISTORY_SYNC_PROMO:
                return new HistorySyncPromoCoordinator(
                        onModuleClickedCallback,
                        callbackController,
                        actionDelegate,
                        removeModuleCallback);
            case ModuleType.TIPS_NOTIFICATIONS_PROMO:
                return new TipsNotificationsPromoCoordinator(
                        onModuleClickedCallback, callbackController, actionDelegate);
            case ModuleType.ENHANCED_SAFE_BROWSING_PROMO:
                return new EnhancedSafeBrowsingPromoCoordinator(
                        onModuleClickedCallback, actionDelegate);
            case ModuleType.ADDRESS_BAR_PLACEMENT_PROMO:
                return new AddressBarPlacementPromoCoordinator(
                        onModuleClickedCallback, actionDelegate);
            case ModuleType.SIGN_IN_PROMO:
                return new SignInPromoCoordinator(onModuleClickedCallback, actionDelegate);
            case ModuleType.SAVE_PASSWORDS_PROMO:
                return new SavePasswordsPromoCoordinator(
                        onModuleClickedCallback, callbackController, actionDelegate);
            case ModuleType.PASSWORD_CHECKUP_PROMO:
                return new PasswordCheckupPromoCoordinator(onModuleClickedCallback, actionDelegate);
            case ModuleType.SETUP_LIST_CELEBRATORY_PROMO:
                return new SetupListCelebratoryPromoCoordinator(
                        onModuleClickedCallback, actionDelegate);
            default:
                assert false : "Educational tip module's card type not supported!";
                return null;
        }
    }
}
