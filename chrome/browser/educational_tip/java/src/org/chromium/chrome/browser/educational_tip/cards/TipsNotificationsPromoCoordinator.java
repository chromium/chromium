// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import androidx.annotation.DrawableRes;

import org.chromium.base.CallbackController;
import org.chromium.base.shared_preferences.SharedPreferenceObserver;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/**
 * Coordinator for the Tips Notifications promo card. Removes the card when {@link
 * ChromePreferenceKeys#TIPS_NOTIFICATIONS_OPT_IN_ELIGIBLE} becomes false.
 */
@NullMarked
public class TipsNotificationsPromoCoordinator implements EducationalTipCardProvider {
    private final EducationTipModuleActionDelegate mActionDelegate;
    private final Runnable mOnClickedRunnable;
    private @Nullable SharedPreferenceObserver mEligibilityPrefObserver;

    /**
     * @param onModuleClickedCallback The callback to be called when the module is clicked.
     * @param callbackController The instance of {@link CallbackController}.
     * @param actionDelegate The instance of {@link EducationTipModuleActionDelegate}.
     * @param removeModuleCallback The callback to be called to remove the module.
     */
    public TipsNotificationsPromoCoordinator(
            Runnable onModuleClickedCallback,
            CallbackController callbackController,
            EducationTipModuleActionDelegate actionDelegate,
            Runnable removeModuleCallback) {
        mActionDelegate = actionDelegate;
        Runnable removeModuleRunnable = callbackController.makeCancelable(removeModuleCallback);
        SharedPreferencesManager sharedPreferences = ChromeSharedPreferences.getInstance();

        mEligibilityPrefObserver =
                sharedPreferences.observe(
                        ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_ELIGIBLE,
                        () -> {
                            if (!sharedPreferences.readBoolean(
                                    ChromePreferenceKeys.TIPS_NOTIFICATIONS_OPT_IN_ELIGIBLE,
                                    /* defaultValue= */ false)) {
                                removeModuleRunnable.run();
                            }
                        });

        mOnClickedRunnable =
                callbackController.makeCancelable(
                        () -> {
                            // Stop observing before opting in so writing the eligibility pref to
                            // false does not remove the module before click metrics look up its
                            // rank.
                            if (mEligibilityPrefObserver != null) {
                                mEligibilityPrefObserver.destroy();
                                mEligibilityPrefObserver = null;
                            }
                            mActionDelegate.optInToTipsNotifications();
                            // Record the click before removing the module so Magic Stack can still
                            // look up its rank.
                            onModuleClickedCallback.run();
                            removeModuleRunnable.run();
                        });
    }

    // EducationalTipCardProvider implementation.

    @Override
    public String getCardTitle() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_tips_notifications_title);
    }

    @Override
    public String getCardDescription() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_tips_notifications_description);
    }

    @Override
    public String getCardButtonText() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_tips_notifications_button);
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.tips_notifications_promo_logo;
    }

    @Override
    public void onCardClicked() {
        mOnClickedRunnable.run();
    }

    @Override
    public void destroy() {
        if (mEligibilityPrefObserver != null) {
            mEligibilityPrefObserver.destroy();
            mEligibilityPrefObserver = null;
        }
    }
}
