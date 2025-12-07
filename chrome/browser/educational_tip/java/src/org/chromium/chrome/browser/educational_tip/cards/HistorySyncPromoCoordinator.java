// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import androidx.annotation.DrawableRes;

import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.Set;

/**
 * A coordinator that is responsible for displaying the history sync education tip that is show on
 * the NTP in the magic stack if the user is eligible.
 */
@NullMarked
public class HistorySyncPromoCoordinator
        implements EducationalTipCardProvider,
                IdentityManager.Observer,
                SyncService.SyncStateChangedListener {

    private static final String HISTORY_OPT_IN_EDUCATIONAL_TIP_PARAM =
            "history_opt_in_educational_tip_param";

    private final EducationTipModuleActionDelegate mActionDelegate;
    private final Runnable mOnClickedRunnable;
    private final Runnable mRemoveModuleRunnable;
    private final @Nullable IdentityManager mIdentityManager;
    private final @Nullable SyncService mSyncService;

    public HistorySyncPromoCoordinator(
            Runnable onModuleClickedCallback,
            CallbackController callbackController,
            EducationTipModuleActionDelegate actionDelegate,
            Runnable removeModuleCallback) {
        mActionDelegate = actionDelegate;

        mRemoveModuleRunnable =
                callbackController.makeCancelable(
                        () -> {
                            removeModuleCallback.run();
                        });

        mOnClickedRunnable =
                callbackController.makeCancelable(
                        () -> {
                            // removeModuleCallback is passed as a callable to ChromeTabbedActivity
                            // so that the promo is dismssed only after the history sync activity is
                            // complete. Otherwise the promo will be dismissed too early.
                            mActionDelegate.showHistorySyncOptIn(removeModuleCallback);
                            onModuleClickedCallback.run();
                        });

        Profile profile = mActionDelegate.getProfileSupplier().get().getOriginalProfile();
        assert profile != null;

        mIdentityManager = IdentityServicesProvider.get().getIdentityManager(profile);
        assert mIdentityManager != null;
        mIdentityManager.addObserver(this);

        mSyncService = SyncServiceFactory.getForProfile(profile);
        assert mSyncService != null;
        mSyncService.addSyncStateChangedListener(this);
    }

    @Override
    public String getCardTitle() {
        return mActionDelegate.getContext().getString(R.string.educational_tip_history_sync_title);
    }

    @Override
    public String getCardDescription() {
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_history_sync_description);
    }

    @Override
    public String getCardButtonText() {
        int buttonStringParam =
                SigninFeatureMap.getInstance()
                        .getFieldTrialParamByFeatureAsInt(
                                SigninFeatures.HISTORY_OPT_IN_EDUCATIONAL_TIP,
                                HISTORY_OPT_IN_EDUCATIONAL_TIP_PARAM,
                                /* defaultValue= */ 0);

        switch (buttonStringParam) {
            case 0:
                return mActionDelegate
                        .getContext()
                        .getString(R.string.educational_tip_history_sync_button_turn_on);
            case 1:
                return mActionDelegate
                        .getContext()
                        .getString(R.string.educational_tip_history_sync_button_lets_go);
            case 2:
                return mActionDelegate
                        .getContext()
                        .getString(R.string.educational_tip_history_sync_button_continue);
            default:
                throw new IllegalStateException(
                        "Invalid variation state for kHistoryOptInEducationalTip");
        }
    }

    @Override
    public @DrawableRes int getCardImage() {
        return R.drawable.history_sync_promo_logo;
    }

    @Override
    public void onCardClicked() {
        mOnClickedRunnable.run();
    }

    /** Implements {@link IdentityManager.Observer}. */
    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        if (eventDetails.getEventTypeFor(ConsentLevel.SIGNIN)
                == PrimaryAccountChangeEvent.Type.CLEARED) {
            mRemoveModuleRunnable.run();
        }
    }

    /** Implements {@link SyncService.SyncStateChangedListener} */
    @Override
    public void syncStateChanged() {
        assert mSyncService != null;
        if (mSyncService
                .getSelectedTypes()
                .containsAll(Set.of(UserSelectableType.HISTORY, UserSelectableType.TABS))) {
            mRemoveModuleRunnable.run();
        }
    }

    @Override
    public void destroy() {
        assert mIdentityManager != null;
        assert mSyncService != null;

        mIdentityManager.removeObserver(this);
        mSyncService.removeSyncStateChangedListener(this);
    }
}
