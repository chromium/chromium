// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.cards;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.DrawableRes;

import org.chromium.base.CallbackController;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.EducationTipModuleActionDelegate;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.setup_list.SetupListCompletable;
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.metrics.SigninAccessPoint;
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
                SyncService.SyncStateChangedListener,
                SetupListCompletable {

    private final EducationTipModuleActionDelegate mActionDelegate;
    private final Runnable mOnClickedRunnable;
    private final Runnable mRemoveModuleRunnable;
    private final @Nullable IdentityManager mIdentityManager;
    private final @Nullable SyncService mSyncService;
    private @Nullable BottomSheetSigninAndHistorySyncCoordinator mSignInCoordinator;

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

        if (SigninFeatureMap.getInstance().isActivitylessSigninAllEntryPointEnabled()) {
            mSignInCoordinator =
                    mActionDelegate.createBottomSheetSigninAndHistorySyncCoordinator(
                            new BottomSheetSigninAndHistorySyncCoordinator.Delegate() {
                                @Override
                                public void onFlowComplete(
                                        SigninAndHistorySyncCoordinator.Result result) {
                                    // Use the cancelable mRemoveModuleRunnable to ensure it
                                    // does nothing if this coordinator is destroyed before
                                    // the flow completes.
                                    mRemoveModuleRunnable.run();
                                }
                            },
                            SigninAccessPoint.HISTORY_SYNC_EDUCATIONAL_TIP);
        }

        mOnClickedRunnable =
                callbackController.makeCancelable(
                        () -> {
                            if (SigninFeatureMap.getInstance()
                                    .isActivitylessSigninAllEntryPointEnabled()) {
                                assumeNonNull(mSignInCoordinator)
                                        .startSigninFlow(
                                                mActionDelegate
                                                        .createHistorySyncBottomSheetConfig());
                            } else {
                                // removeModuleCallback is passed as a callable to
                                // ChromeTabbedActivity so that the promo is dismssed only after the
                                // history sync activity is complete. Otherwise the promo will be
                                // dismissed too early.
                                mActionDelegate.showHistorySyncOptInLegacy(removeModuleCallback);
                            }
                            onModuleClickedCallback.run();
                        });

        Profile profile =
                assumeNonNull(mActionDelegate.getProfileSupplier().get()).getOriginalProfile();
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
        return mActionDelegate
                .getContext()
                .getString(R.string.educational_tip_history_sync_button_lets_go);
    }

    @Override
    public @DrawableRes int getCardImage() {
        if (SetupListModuleUtils.isSetupListModule(ModuleType.HISTORY_SYNC_PROMO)) {
            return R.drawable.setup_list_history_sync_promo_logo;
        }
        return R.drawable.history_sync_promo_logo;
    }

    @Override
    public void onCardClicked() {
        mOnClickedRunnable.run();
    }

    @Override
    public boolean isComplete() {
        return SetupListModuleUtils.isModuleCompleted(ModuleType.HISTORY_SYNC_PROMO);
    }

    @Override
    public @DrawableRes int getCardImageCompletedResId() {
        return R.drawable.setup_list_completed_background_wavy_circle;
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
            if (SetupListModuleUtils.isSetupListModule(ModuleType.HISTORY_SYNC_PROMO)) {
                SetupListModuleUtils.setModuleCompleted(
                        ModuleType.HISTORY_SYNC_PROMO, /* silent= */ true);
            } else {
                mRemoveModuleRunnable.run();
            }
        }
    }

    @Override
    public void destroy() {
        assert mIdentityManager != null;
        assert mSyncService != null;

        mIdentityManager.removeObserver(this);
        mSyncService.removeSyncStateChangedListener(this);

        if (mSignInCoordinator != null) {
            mSignInCoordinator.destroy();
            mSignInCoordinator = null;
        }
    }
}
