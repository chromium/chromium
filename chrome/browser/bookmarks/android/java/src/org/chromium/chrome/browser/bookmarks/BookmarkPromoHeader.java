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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.LegacySyncPromoView;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.chrome.browser.ui.signin.SyncPromoController;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
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
    private static @Nullable Boolean sShouldShowPromoForTests;

    private final Context mContext;
    private final SigninManager mSigninManager;
    private final AccountManagerFacade mAccountManagerFacade;
    private final Runnable mPromoHeaderChangeAction;

    private final @Nullable ProfileDataCache mProfileDataCache;
    private final @Nullable SyncPromoController mSyncPromoController;
    private boolean mShouldShowPromo;
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
                        SigninAndHistorySyncActivityLauncherImpl.get());
        if (syncPromoController.canShowSyncPromo()) {
            mProfileDataCache = ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mContext);
            mSyncPromoController = syncPromoController;
        } else {
            mProfileDataCache = null;
            mSyncPromoController = null;
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
     * @return Whether the promo should be shown or not.
     */
    boolean shouldShowPromo() {
        return mShouldShowPromo;
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
        mSyncPromoController.setUpSyncPromoView(
                mProfileDataCache, view, this::setPersonalizedSigninPromoDeclined);
    }

    /** Detaches the previously configured {@link PersonalizedSigninPromoView}. */
    void detachPersonalizePromoView() {
        if (mSyncPromoController != null) mSyncPromoController.detach();
    }

    /** Saves that the personalized signin promo was declined and updates the UI. */
    private void setPersonalizedSigninPromoDeclined() {
        mShouldShowPromo = calculateShouldShowPromo();
        triggerPromoUpdate();
    }

    private boolean calculateShouldShowPromo() {
        if (sShouldShowPromoForTests != null) {
            return sShouldShowPromoForTests;
        }

        if (mSyncService == null) {
            // |mSyncService| will remain null until the next browser startup, so no sense in
            // offering any promo.
            return false;
        }

        return mSigninManager.isSigninAllowed()
                && mSyncPromoController != null
                && mSyncPromoController.canShowSyncPromo();
    }

    private void updatePromoState() {
        final boolean shouldShowPromo = calculateShouldShowPromo();
        if (shouldShowPromo == mShouldShowPromo) return;

        final boolean hasPromoVisibilityChangedtoShown = !mShouldShowPromo && shouldShowPromo;
        if (hasPromoVisibilityChangedtoShown) {
            if (mSyncPromoController != null) {
                mSyncPromoController.increasePromoShowCount();
            }
        }
        mShouldShowPromo = shouldShowPromo;
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
     * Forces the promo visibility to a particular value for testing purposes.
     *
     * @param shouldShowPromo Whether the promo should be forced to show or forced to hide. A null
     *     value indicates that the promo's visibility is not overridden.
     */
    public static void forcePromoVisibilityForTesting(@Nullable Boolean shouldShowPromo) {
        sShouldShowPromoForTests = shouldShowPromo;
        ResettersForTesting.register(() -> sShouldShowPromoForTests = null);
    }
}
