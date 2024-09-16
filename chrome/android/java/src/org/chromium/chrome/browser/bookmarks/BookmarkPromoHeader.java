// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.LegacySyncPromoView;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.SyncPromoController;
import org.chromium.chrome.browser.ui.signin.SyncPromoController.SyncPromoState;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.browser.ui.signin.signin_promo.SigninPromoCoordinator;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.SyncService;

/**
 * Class that manages all the logic and UI behind the signin promo header in the bookmark content
 * UI. The header is shown only on certain situations, (e.g., not signed in).
 */
public class BookmarkPromoHeader
        implements SyncService.SyncStateChangedListener,
                SignInStateObserver,
                ProfileDataCache.Observer,
                AccountsChangeObserver {
    // TODO(kkimlabs): Figure out the optimal number based on UMA data.
    private static final int MAX_SIGNIN_AND_SYNC_PROMO_SHOW_COUNT = 10;

    private static @Nullable @SyncPromoState Integer sPromoStateForTests;

    private final Context mContext;
    private final SigninManager mSigninManager;
    private final AccountManagerFacade mAccountManagerFacade;
    private final Runnable mPromoHeaderChangeAction;

    private @Nullable ProfileDataCache mProfileDataCache;
    private final @Nullable SyncPromoController mSyncPromoController;
    private final @Nullable SigninPromoCoordinator mSigninPromoCoordinator;
    private @SyncPromoState int mPromoState = SyncPromoState.NO_PROMO;
    private final @Nullable SyncService mSyncService;
    private final Profile mProfile;

    /**
     * Initializes the class. Note that this will start listening to signin related events and
     * update itself if needed.
     */
    BookmarkPromoHeader(Context context, Profile profile, Runnable promoHeaderChangeAction) {
        mContext = context;
        mProfile = profile;
        mPromoHeaderChangeAction = promoHeaderChangeAction;
        mSyncService = SyncServiceFactory.getForProfile(profile);
        mSigninManager = IdentityServicesProvider.get().getSigninManager(mProfile);
        mAccountManagerFacade = AccountManagerFacadeProvider.getInstance();

        AccountPickerBottomSheetStrings bottomSheetStrings =
                new AccountPickerBottomSheetStrings.Builder(
                                R.string.signin_account_picker_bottom_sheet_title)
                        .build();
        SyncPromoController syncPromoController =
                new SyncPromoController(
                        mProfile,
                        bottomSheetStrings,
                        SigninAccessPoint.BOOKMARK_MANAGER,
                        SyncConsentActivityLauncherImpl.get(),
                        SigninAndHistorySyncActivityLauncherImpl.get());
        if (SigninPromoCoordinator.canShowBookmarkSigninPromo(mProfile)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)) {
            // TODO(crbug.com/327387704): Remove dependency on ProfileDataCache and move
            // corresponding logic into SigninPromoMediator.
            mProfileDataCache = null;
            mSigninPromoCoordinator =
                    new SigninPromoCoordinator(
                            mContext,
                            org.chromium.chrome.browser.ui.signin.R.string
                                    .signin_promo_title_bookmarks,
                            org.chromium.chrome.browser.ui.signin.R.string
                                    .signin_promo_description_bookmarks,
                            false,
                            false);
            mSyncPromoController = null;
        } else if (!ChromeFeatureList.isEnabled(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)
                && syncPromoController.canShowSyncPromo()) {
            mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mContext);
            mSyncPromoController = syncPromoController;
            mSigninPromoCoordinator = null;
        } else {
            mProfileDataCache = null;
            mSyncPromoController = null;
            mSigninPromoCoordinator = null;
        }

        if (mSyncService != null) mSyncService.addSyncStateChangedListener(this);
        mSigninManager.addSignInStateObserver(this);
        if (mSyncPromoController != null) {
            mAccountManagerFacade.addObserver(this);
            mProfileDataCache.addObserver(this);
        }

        updatePromoState();
    }

    /** Clean ups the class. Must be called once done using this class. */
    void destroy() {
        if (mSyncService != null) mSyncService.removeSyncStateChangedListener(this);

        if (mSyncPromoController != null) {
            mAccountManagerFacade.removeObserver(this);
            mProfileDataCache.removeObserver(this);
        }

        mSigninManager.removeSignInStateObserver(this);
    }

    /**
     * @return The current state of the promo.
     */
    @SyncPromoState
    int getPromoState() {
        return mPromoState;
    }

    /** Returns personalized signin promo header {@link View}. */
    View createPersonalizedSigninAndSyncPromoHolder(ViewGroup parent) {
        return LayoutInflater.from(mContext)
                .inflate(R.layout.sync_promo_view_bookmarks, parent, false);
    }

    /** Returns sync promo header {@link View}. */
    View createSyncPromoHolder(ViewGroup parent) {
        return LegacySyncPromoView.create(parent, mProfile, SigninAccessPoint.BOOKMARK_MANAGER);
    }

    /** Sets up the sync promo view. */
    void setUpSyncPromoView(PersonalizedSigninPromoView view) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)) {
            // TODO(crbug.com/327387704): Set up the view directly in the
            // BookmarkManagerCoordinator.
            mSigninPromoCoordinator.setView(view);
        } else {
            mSyncPromoController.setUpSyncPromoView(
                    mProfileDataCache, view, this::setPersonalizedSigninPromoDeclined);
        }
    }

    /** Detaches the previously configured {@link PersonalizedSigninPromoView}. */
    void detachPersonalizePromoView() {
        if (mSyncPromoController != null) mSyncPromoController.detach();
    }

    /** Saves that the personalized signin promo was declined and updates the UI. */
    private void setPersonalizedSigninPromoDeclined() {
        mPromoState = calculatePromoState();
        triggerPromoUpdate();
    }

    /**
     * @return Whether the personalized signin promo should be shown to user.
     */
    private boolean shouldShowBookmarkSigninPromo() {
        return mSigninManager.isSyncOptInAllowed()
                && ((mSyncPromoController != null && mSyncPromoController.canShowSyncPromo())
                        || (mSigninPromoCoordinator != null
                                && SigninPromoCoordinator.canShowBookmarkSigninPromo(mProfile)));
    }

    private @SyncPromoState int calculatePromoState() {
        if (sPromoStateForTests != null) {
            return sPromoStateForTests;
        }

        if (mSyncService == null) {
            // |mSyncService| will remain null until the next browser startup, so no sense in
            // offering any promo.
            return SyncPromoState.NO_PROMO;
        }

        if (ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            return shouldShowBookmarkSigninPromo()
                    ? SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE
                    : SyncPromoState.NO_PROMO;
        } else if (!mSigninManager.getIdentityManager().hasPrimaryAccount(ConsentLevel.SYNC)) {
            if (!shouldShowBookmarkSigninPromo()) {
                return SyncPromoState.NO_PROMO;
            }

            return mSigninManager.getIdentityManager().hasPrimaryAccount(ConsentLevel.SIGNIN)
                    ? SyncPromoState.PROMO_FOR_SIGNED_IN_STATE
                    : SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE;
        }

        boolean impressionLimitNotReached =
                ChromeSharedPreferences.getInstance()
                                .readInt(ChromePreferenceKeys.SIGNIN_AND_SYNC_PROMO_SHOW_COUNT)
                        < MAX_SIGNIN_AND_SYNC_PROMO_SHOW_COUNT;
        if (mSyncService.getSelectedTypes().isEmpty() && impressionLimitNotReached) {
            return SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE;
        }
        return SyncPromoState.NO_PROMO;
    }

    private void updatePromoState() {
        final @SyncPromoState int newState = calculatePromoState();
        if (newState == mPromoState) return;

        // PROMO_SYNC state and it's impression counts is not tracked by SyncPromoController.
        final boolean hasSyncPromoStateChangedtoShown =
                (mPromoState == SyncPromoState.NO_PROMO
                                || mPromoState == SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE)
                        && (newState == SyncPromoState.PROMO_FOR_SIGNED_OUT_STATE
                                || newState == SyncPromoState.PROMO_FOR_SIGNED_IN_STATE);
        if (hasSyncPromoStateChangedtoShown) {
            if (mSyncPromoController != null) {
                assert mSigninPromoCoordinator == null;
                mSyncPromoController.increasePromoShowCount();
            }
            if (mSigninPromoCoordinator != null) {
                assert mSyncPromoController == null;
                mSigninPromoCoordinator.increasePromoShowCount();
            }
        }
        if (newState == SyncPromoState.PROMO_FOR_SYNC_TURNED_OFF_STATE) {
            ChromeSharedPreferences.getInstance()
                    .incrementInt(ChromePreferenceKeys.SIGNIN_AND_SYNC_PROMO_SHOW_COUNT);
        }
        mPromoState = newState;
    }

    // SyncService.SyncStateChangedListener implementation.
    @Override
    public void syncStateChanged() {
        updatePromoState();
        triggerPromoUpdate();
    }

    // SignInStateObserver implementation.
    @Override
    public void onSignedIn() {
        updatePromoState();
        triggerPromoUpdate();
    }

    @Override
    public void onSignedOut() {
        updatePromoState();
        triggerPromoUpdate();
    }

    // ProfileDataCache.Observer implementation.
    @Override
    public void onProfileDataUpdated(String accountEmail) {
        triggerPromoUpdate();
    }

    // AccountsChangeObserver implementation.
    @Override
    public void onCoreAccountInfosChanged() {
        triggerPromoUpdate();
    }

    private void triggerPromoUpdate() {
        detachPersonalizePromoView();
        mPromoHeaderChangeAction.run();
    }

    /**
     * Forces the promo state to a particular value for testing purposes.
     *
     * @param promoState The promo state to which the header will be set to.
     */
    public static void forcePromoStateForTesting(@Nullable @SyncPromoState Integer promoState) {
        sPromoStateForTests = promoState;
        ResettersForTesting.register(() -> sPromoStateForTests = null);
    }
}
