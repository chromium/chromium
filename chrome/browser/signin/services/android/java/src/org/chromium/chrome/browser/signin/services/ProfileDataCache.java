// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.DimenRes;
import androidx.annotation.MainThread;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.subscription_eligibility.SubscriptionEligibilityService;
import org.chromium.components.browser_ui.util.AvatarGenerator;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManager.RefreshTokensLoadingState;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * Fetches and caches Google Account profile images and full names for the accounts on the device.
 */
@MainThread
@NullMarked
public class ProfileDataCache
        implements IdentityManager.Observer, SubscriptionEligibilityService.Observer {
    /** Observer to get notifications about changes in profile data. */
    public interface Observer {

        /**
         * Notifies that the list of accounts has been updated.
         *
         * @param accounts The list of accounts.
         *     <p>TODO(crbug.com/480239119): Remove default implementation.
         */
        default void onAccountsUpdated(List<DisplayableProfileData> accounts) {}

        /**
         * Notifies that an account's profile data has been updated.
         *
         * <p>Note: This may be called for the primary account before the full account list is
         * loaded, i.e. before the {@link ProfileDataCache#getAccounts()} Promise is fulfilled.
         *
         * @param profileData The profile data that has been updated.
         */
        void onProfileDataUpdated(DisplayableProfileData profileData);
    }

    private final Context mContext;
    private final AccountManagerFacade mAccountManagerFacade;
    private final @Nullable AccountManagerAccountsChangeObserver
            mAccountManagerAccountsChangeObserver;
    private final IdentityManager mIdentityManager;
    private final @Nullable IdentityManagerAccountsChangeObserver
            mIdentityManagerAccountsChangeObserver;
    private final int mImageSize;
    private final int mAvatarSize;
    private final int mTotalPadding;
    private final Map<CoreAccountId, BadgeConfig> mPerAccountBadgeConfig = new HashMap<>();
    private final Drawable mPlaceholderImage;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final Map<CoreAccountId, DisplayableProfileData> mAccountsCache = new LinkedHashMap<>();
    private Promise<List<DisplayableProfileData>> mAccountsPromise = new Promise<>();
    private final boolean mAiTierRingEnabled;
    private final @Nullable SubscriptionEligibilityService mSubscriptionEligibilityService;
    private final @Px int mRingThicknessPx;
    private final @Nullable SubscriptionTierBrandingDelegate mBrandingDelegate;
    private @Nullable Drawable mAccountCirclePlaceholderImage;

    @VisibleForTesting
    ProfileDataCache(
            Context context,
            AccountManagerFacade accountManagerFacade,
            IdentityManager identityManager,
            @Nullable SubscriptionEligibilityService subscriptionEligibilityService,
            @Px int imageSize,
            @Px int ringThicknessPx,
            boolean aiTierRingEnabled,
            @Nullable SubscriptionTierBrandingDelegate brandingDelegate) {
        assert identityManager != null;
        mContext = context;
        mAccountManagerFacade = accountManagerFacade;
        mIdentityManager = identityManager;
        if (SigninFeatureMap.isEnabled(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)) {
            mAccountManagerAccountsChangeObserver = null;
            mIdentityManagerAccountsChangeObserver = new IdentityManagerAccountsChangeObserver();
        } else {
            mAccountManagerAccountsChangeObserver = new AccountManagerAccountsChangeObserver();
            mIdentityManagerAccountsChangeObserver = null;
        }
        mImageSize = imageSize;
        mRingThicknessPx = ringThicknessPx;
        mAiTierRingEnabled = aiTierRingEnabled;
        mBrandingDelegate = brandingDelegate;
        mSubscriptionEligibilityService = subscriptionEligibilityService;

        if (mAiTierRingEnabled) {
            int ringSpacingPx =
                    context.getResources().getDimensionPixelSize(R.dimen.ai_tier_ring_spacing);
            mTotalPadding = mRingThicknessPx + ringSpacingPx;
            mAvatarSize = mImageSize - 2 * mTotalPadding;
        } else {
            mTotalPadding = 0;
            mAvatarSize = mImageSize;
        }

        mPlaceholderImage = getScaledPlaceholderImage(context, mAvatarSize);
        updateCache();
    }

    /**
     * @param context Context of the application to extract resources from.
     * @param identityManager IdentityManager to use.
     * @param subscriptionEligibilityService SubscriptionEligibilityService to observe.
     * @param imageSize Size of the image.
     * @param ringThicknessPx The thickness of the AI tier ring in pixels.
     * @return A {@link ProfileDataCache} object configured to draw the AI tier ring. Note that the
     *     final generated image size will strictly be `imageSize`. The inner avatar will be shrunk
     *     to accommodate the ring.
     */
    public static ProfileDataCache createWithAiTierRing(
            Context context,
            IdentityManager identityManager,
            SubscriptionEligibilityService subscriptionEligibilityService,
            @Px int imageSize,
            @Px int ringThicknessPx) {
        SubscriptionTierBrandingDelegate brandingDelegate =
                ServiceLoaderUtil.maybeCreate(SubscriptionTierBrandingDelegate.class);
        return new ProfileDataCache(
                context,
                AccountManagerFacadeProvider.getInstance(),
                identityManager,
                subscriptionEligibilityService,
                imageSize,
                ringThicknessPx,
                /* aiTierRingEnabled= */ true,
                brandingDelegate);
    }

    /**
     * @param context Context of the application to extract resources from.
     * @return A {@link ProfileDataCache} object with default image size(R.dimen.user_picture_size)
     *     and no badge.
     */
    public static ProfileDataCache createWithDefaultImageSizeAndNoBadge(
            Context context, IdentityManager identityManager) {
        return new ProfileDataCache(
                context,
                AccountManagerFacadeProvider.getInstance(),
                identityManager,
                /* subscriptionEligibilityService= */ null,
                context.getResources().getDimensionPixelSize(R.dimen.user_picture_size),
                /* ringThicknessPx= */ 0,
                /* aiTierRingEnabled= */ false,
                /* brandingDelegate= */ null);
    }

    /**
     * @param context Context of the application to extract resources from.
     * @param imageSizeResId Resource id of the image size.
     * @return A {@link ProfileDataCache} object with the given image size and no badge.
     */
    public static ProfileDataCache createWithoutBadge(
            Context context, IdentityManager identityManager, @DimenRes int imageSizeResId) {
        return new ProfileDataCache(
                context,
                AccountManagerFacadeProvider.getInstance(),
                identityManager,
                /* subscriptionEligibilityService= */ null,
                context.getResources().getDimensionPixelSize(imageSizeResId),
                /* ringThicknessPx= */ 0,
                /* aiTierRingEnabled= */ false,
                /* brandingDelegate= */ null);
    }

    /**
     * @return A {@link Drawable} containing the placeholder image. Note that the generated image
     *     size will strictly be the image size this cache was created with.
     */
    public Drawable getPlaceholderImage() {
        // TODO(crbug.com/543773382): Consider deduplicating with getScaledPlaceholderImage().
        if (mAccountCirclePlaceholderImage == null) {
            Drawable accountCircle =
                    AppCompatResources.getDrawable(mContext, R.drawable.account_circle);
            assert accountCircle != null;
            Bitmap output = Bitmap.createBitmap(mImageSize, mImageSize, Bitmap.Config.ARGB_8888);
            output.setDensity(mContext.getResources().getDisplayMetrics().densityDpi);
            Canvas canvas = new Canvas(output);

            accountCircle.setBounds(
                    mTotalPadding,
                    mTotalPadding,
                    mImageSize - mTotalPadding,
                    mImageSize - mTotalPadding);
            accountCircle.draw(canvas);
            mAccountCirclePlaceholderImage = new BitmapDrawable(mContext.getResources(), output);
        }
        return mAccountCirclePlaceholderImage;
    }

    /**
     * Gets the list of cached accounts that are synchronized with the device accounts.
     *
     * <p>Accounts data are populated from {@link IdentityManager}. To observe changes to accounts,
     * implement {@link Observer#onAccountsUpdated}.
     *
     * <p>Note: {@link #getById} may return cached profile data for an already-known account (such
     * as the primary account) before this Promise is fulfilled. Once fulfilled, the returned
     * Promise holds a snapshot of the accounts at the time of the call.
     *
     * @return A {@link Promise} containing the list of cached {@link DisplayableProfileData}
     *     accounts.
     */
    public Promise<List<DisplayableProfileData>> getAccounts() {
        return mAccountsPromise;
    }

    /**
     * Returns cached {@link DisplayableProfileData} for the given account ID.
     *
     * <p>Method is synchronous and does not trigger any account info fetches. First it checks if
     * the {@link DisplayableProfileData} is in the cache. If the cache hasn't been populated yet,
     * or doesn't know the account, the cache is refreshed from the current source of accounts. If
     * the account still cannot be found afterwards, an {@link IllegalArgumentException} is thrown -
     * this means data for this account is not available.
     *
     * <p>Note: This method may succeed for an already-known account (such as the primary account)
     * even before the {@link #getAccounts()} Promise is fulfilled.
     *
     * @param accountId The account ID for which to get the profile data.
     * @throws IllegalArgumentException if the account is not found.
     * @return The {@link DisplayableProfileData} for the given account ID.
     */
    public DisplayableProfileData getById(CoreAccountId accountId) {
        if (!mAccountsCache.containsKey(accountId)) {
            updateCache();
        }

        var profileData = mAccountsCache.get(accountId);
        if (profileData == null) {
            throw new IllegalArgumentException("Account not found");
        }

        return profileData;
    }

    /**
     * Sets a {@link BadgeConfig} for a given account and refreshes its cached profile data, if any.
     * Otherwise, the badge is applied once the account's profile data is built.
     *
     * @param accountId The account id for which to set this badge.
     * @param badgeConfig The badge configuration. If null then the current badge is removed.
     */
    public void setBadge(CoreAccountId accountId, @Nullable BadgeConfig badgeConfig) {
        if (Objects.equals(mPerAccountBadgeConfig.get(accountId), badgeConfig)) {
            // Update is a no-op. The per-account badge set to accountId is the same as the
            // badgeConfig.
            return;
        }
        if (badgeConfig != null) {
            mPerAccountBadgeConfig.put(accountId, badgeConfig);
        } else {
            mPerAccountBadgeConfig.remove(accountId);
        }
        if (SigninFeatureMap.isEnabled(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)) {
            if (!mAccountsCache.containsKey(accountId)) return;
        }
        var accountInfo = findAccountInfo(accountId);
        if (accountInfo != null) updateCache(accountInfo);
    }

    public @Nullable BadgeConfig getBadgeConfigForTesting(CoreAccountId accountId) {
        return mPerAccountBadgeConfig.get(accountId);
    }

    /**
     * @param observer Observer that should be notified when new profile images are available.
     */
    public void addObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        if (!mObservers.isEmpty()) {
            mObservers.addObserver(observer);
            return;
        }

        if (SigninFeatureMap.isEnabled(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)) {
            mIdentityManager.addObserver(assumeNonNull(mIdentityManagerAccountsChangeObserver));
        } else {
            mAccountManagerFacade.addObserver(assumeNonNull(mAccountManagerAccountsChangeObserver));
        }
        mIdentityManager.addObserver(this);
        if (mAiTierRingEnabled && mSubscriptionEligibilityService != null) {
            mSubscriptionEligibilityService.addObserver(this);
        }
        mObservers.addObserver(observer);
        if (SigninFeatureMap.isEnabled(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
                && !mAccountsPromise.isFulfilled()) {
            updateCache();
        }
    }

    /**
     * @param observer Observer that was added by {@link #addObserver} and should be removed.
     */
    public void removeObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        mObservers.removeObserver(observer);
        if (mObservers.isEmpty()) {
            mIdentityManager.removeObserver(this);
            if (mAiTierRingEnabled && mSubscriptionEligibilityService != null) {
                mSubscriptionEligibilityService.removeObserver(this);
            }
            if (SigninFeatureMap.isEnabled(
                    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)) {
                mIdentityManager.removeObserver(
                        assumeNonNull(mIdentityManagerAccountsChangeObserver));
            } else {
                mAccountManagerFacade.removeObserver(
                        assumeNonNull(mAccountManagerAccountsChangeObserver));
            }
        }
    }

    /** Implements {@link SubscriptionEligibilityService.Observer}. */
    @Override
    public void onAiSubscriptionTierChanged() {
        // AI tier rings are only displayed for eligible tiers. We must update the cache
        // when the tier changes so that the ring is applied (or removed) accordingly.
        if (!mAiTierRingEnabled) return;
        updateCache();
    }

    /** Implements {@link SubscriptionEligibilityService.Observer}. */
    @Override
    public void onSubscriptionBenefitsChanged() {
        // Subscription benefits ring is only displayed for eligible tiers. We must update the cache
        // when benefits change so that the ring is applied (or removed) accordingly.
        if (!mAiTierRingEnabled) return;
        updateCache();
    }

    /** Implements {@link IdentityManager.Observer}. */
    @Override
    public void onPrimaryAccountChanged(PrimaryAccountChangeEvent eventDetails) {
        // AI tier rings are only displayed for the primary account. We must update the cache
        // when the primary account changes so that the ring is applied (or removed) accordingly.
        if (!mAiTierRingEnabled) return;
        updateCache();
    }

    /** Implements {@link IdentityManager.Observer}. */
    @Override
    public void onExtendedAccountInfoUpdated(AccountInfo accountInfo) {
        if (mIdentityManager.findExtendedAccountInfoByAccountId(accountInfo.getId()) == null) {
            // Account was removed from the IdentityManager.
            // Cache will be updated by onRefreshTokenRemovedForAccount() callback.
            return;
        }
        if (SigninFeatureMap.isEnabled(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)) {
            // Only known accounts are updated here. New accounts are added by
            // onRefreshTokenUpdatedForAccount() once they have a refresh token, as observers rely
            // on the accounts with refresh tokens.
            if (!mAccountsCache.containsKey(accountInfo.getId())) return;
        }
        updateCache(accountInfo);
    }

    /** Checks if the cache contains profile data for the given account ID. */
    public boolean hasProfileDataForTesting(CoreAccountId accountId) {
        return mAccountsCache.containsKey(accountId);
    }

    private void updateCache() {
        final @Nullable List<AccountInfo> coreAccounts = getCoreAccountsIfLoaded();
        final @Nullable AccountInfo primaryAccountInfo = mIdentityManager.getPrimaryAccountInfo();

        // If the full account list is not loaded yet, cache the primary account (if present)
        // for getById() lookups, but do not fulfill the getAccounts() Promise.
        if (coreAccounts == null) {
            if (primaryAccountInfo != null) {
                var profileData = toDisplayableProfileData(primaryAccountInfo);
                mAccountsCache.put(primaryAccountInfo.getId(), profileData);
                fireOnProfileDataUpdated(profileData);
            }
            return;
        }

        // The primary account can be set before it appears in the loaded coreAccounts list.
        final var allAccounts = new ArrayList<>(coreAccounts);
        if (primaryAccountInfo != null) {
            allAccounts.add(primaryAccountInfo);
        }
        updateCache(allAccounts);
    }

    /**
     * Updates the cached profile data of a single account and notifies the observers about it.
     * onAccountsUpdated() is only called if MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS flag is
     * enabled and the {@link #getAccounts()} Promise is fulfilled. onProfileDataUpdated() is always
     * called.
     *
     * <p>TODO(crbug.com/571803558): updateCache() and addToCache() methods have similar logic and
     * should be unified after the MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS flag cleanup.
     */
    private void updateCache(final AccountInfo accountInfo) {
        if (SigninFeatureMap.isEnabled(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)) {
            assert mAccountsCache.containsKey(accountInfo.getId());
        }
        var profileData = toDisplayableProfileData(accountInfo);
        mAccountsCache.put(accountInfo.getId(), profileData);
        refreshAccountsPromiseIfFulfilled();
        if (SigninFeatureMap.isEnabled(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)
                && mAccountsPromise.isFulfilled()) {
            fireOnAccountsUpdated(mAccountsPromise.getResult());
        }
        fireOnProfileDataUpdated(profileData);
    }

    /**
     * Adds a new account to the cache and notifies the observers about the updated account list and
     * the profile data of the added account. Must only be called once the account list has been
     * loaded, i.e. the {@link #getAccounts()} Promise is fulfilled.
     *
     * <p>TODO(crbug.com/571803558): updateCache() and addToCache() methods have similar logic and
     * should be unified after the MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS flag cleanup.
     */
    private void addToCache(final AccountInfo accountInfo) {
        assert !mAccountsCache.containsKey(accountInfo.getId());
        assert mAccountsPromise.isFulfilled();
        var profileData = toDisplayableProfileData(accountInfo);
        mAccountsCache.put(accountInfo.getId(), profileData);
        mAccountsPromise = Promise.fulfilled(new ArrayList<>(mAccountsCache.values()));
        fireOnAccountsUpdated(mAccountsPromise.getResult());
        fireOnProfileDataUpdated(profileData);
    }

    private void updateCache(List<AccountInfo> accounts) {
        var displayableAccounts = new LinkedHashMap<CoreAccountId, DisplayableProfileData>();
        for (AccountInfo account : accounts) {
            // Use computeIfAbsent to avoid overriding existing entries and redundant avatar
            // generation.
            displayableAccounts.computeIfAbsent(
                    account.getId(),
                    id -> {
                        var extendedAccountInfo =
                                mIdentityManager.findExtendedAccountInfoByAccountId(id);
                        return toDisplayableProfileData(
                                extendedAccountInfo != null ? extendedAccountInfo : account);
                    });
        }
        mAccountsCache.clear();
        mAccountsCache.putAll(displayableAccounts);
        List<DisplayableProfileData> allAccounts = new ArrayList<>(mAccountsCache.values());
        if (!mAccountsPromise.isFulfilled()) {
            mAccountsPromise.fulfill(allAccounts);
        } else {
            mAccountsPromise = Promise.fulfilled(allAccounts);
        }

        fireOnAccountsUpdated(allAccounts);
        // TODO(crbug.com/485130949): Remove that callback after implementation of
        // onAccountsUpdated() in all UIs. (Blocked by crbug.com/480239119)
        fireOnProfileDataUpdated(allAccounts);
    }

    /**
     * Keeps the {@link #getAccounts()} Promise in sync after a single cache entry changes. Does
     * nothing until the full account list has been loaded.
     */
    private void refreshAccountsPromiseIfFulfilled() {
        if (mAccountsPromise.isFulfilled()) {
            mAccountsPromise = Promise.fulfilled(new ArrayList<>(mAccountsCache.values()));
        }
    }

    private DisplayableProfileData toDisplayableProfileData(AccountInfo accountInfo) {
        Drawable croppedAvatar =
                accountInfo.getAccountImage() != null
                        ? AvatarGenerator.makeRoundAvatar(
                                mContext.getResources(), accountInfo.getAccountImage(), mAvatarSize)
                        : mPlaceholderImage;
        BadgeConfig badgeConfig = mPerAccountBadgeConfig.get(accountInfo.getId());
        boolean hasAiTierRing = false;

        if (mAiTierRingEnabled) {
            croppedAvatar = padAvatarForAiTierRing(croppedAvatar);
        }

        if (badgeConfig != null) {
            croppedAvatar =
                    overlayBadgeOnUserPicture(badgeConfig, croppedAvatar, mAiTierRingEnabled);
        } else {
            hasAiTierRing = isEligibleForAiTierRing(accountInfo);
            if (hasAiTierRing) {
                croppedAvatar = overlayAiRingOnAvatar(croppedAvatar);
            }
        }

        if (SigninFeatureMap.isEnabled(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)) {
            return new DisplayableProfileData(
                    accountInfo.getId(),
                    accountInfo.getEmail(),
                    croppedAvatar,
                    accountInfo.getFullName(),
                    accountInfo.getGivenName(),
                    accountInfo.canHaveEmailAddressDisplayed(),
                    hasAiTierRing);
        } else {
            final var shouldPopulateNames = accountInfo.hasDisplayableInfo() || badgeConfig != null;
            return new DisplayableProfileData(
                    accountInfo.getId(),
                    accountInfo.getEmail(),
                    croppedAvatar,
                    shouldPopulateNames ? accountInfo.getFullName() : null,
                    shouldPopulateNames ? accountInfo.getGivenName() : null,
                    accountInfo.canHaveEmailAddressDisplayed(),
                    hasAiTierRing);
        }
    }

    private boolean isEligibleForAiTierRing(AccountInfo accountInfo) {
        if (!mAiTierRingEnabled || mSubscriptionEligibilityService == null) return false;
        AccountInfo primaryAccount = mIdentityManager.getPrimaryAccountInfo();
        boolean isPrimary =
                primaryAccount != null && primaryAccount.getId().equals(accountInfo.getId());
        return isPrimary && mSubscriptionEligibilityService.getAiSubscriptionTier() > 0;
    }

    private void fireOnAccountsUpdated(List<DisplayableProfileData> accounts) {
        for (Observer observer : mObservers) {
            observer.onAccountsUpdated(accounts);
        }
    }

    private void fireOnProfileDataUpdated(List<DisplayableProfileData> accounts) {
        for (DisplayableProfileData profileData : accounts) {
            fireOnProfileDataUpdated(profileData);
        }
    }

    private void fireOnProfileDataUpdated(DisplayableProfileData profileData) {
        for (Observer observer : mObservers) {
            observer.onProfileDataUpdated(profileData);
        }
    }

    private @Nullable AccountInfo findAccountInfo(CoreAccountId accountId) {
        var accountInfo = mIdentityManager.findExtendedAccountInfoByAccountId(accountId);
        if (accountInfo != null) {
            return accountInfo;
        }
        var coreAccounts = getCoreAccountsIfLoaded();
        if (coreAccounts != null) {
            for (var coreAccountInfo : coreAccounts) {
                if (coreAccountInfo.getId().equals(accountId)) {
                    return coreAccountInfo;
                }
            }
        }
        var primaryAccountInfo = mIdentityManager.getPrimaryAccountInfo();
        if (primaryAccountInfo != null && primaryAccountInfo.getId().equals(accountId)) {
            return primaryAccountInfo;
        }
        return null;
    }

    private @Nullable List<AccountInfo> getCoreAccountsIfLoaded() {
        if (SigninFeatureMap.isEnabled(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)) {
            switch (mIdentityManager.getRefreshTokensLoadingState()) {
                case RefreshTokensLoadingState.SUCCEEDED:
                    return mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken();
                case RefreshTokensLoadingState.FAILED:
                    return List.of();
                default:
                    return null;
            }
        } else {
            var accounts = mAccountManagerFacade.getAccounts();
            if (accounts.isFulfilled()) {
                return accounts.getResult();
            }
            return null;
        }
    }

    // TODO(crbug.com/40944114): Consider using UiUtils.drawIconWithBadge instead.
    private Drawable overlayBadgeOnUserPicture(
            BadgeConfig badgeConfig, Drawable userPicture, boolean isPadded) {
        int padding = isPadded ? mTotalPadding : 0;

        int badgeSize = badgeConfig.getBadgeSize();
        int badgeX = badgeConfig.getPosition().x + padding;
        int badgeY = badgeConfig.getPosition().y + padding;
        int borderSize = badgeConfig.getBorderSize();

        int badgedPictureWidth = isPadded ? mImageSize : Math.max(badgeX + badgeSize, mImageSize);
        int badgedPictureHeight = isPadded ? mImageSize : Math.max(badgeY + badgeSize, mImageSize);

        Bitmap badgedPicture =
                Bitmap.createBitmap(
                        badgedPictureWidth, badgedPictureHeight, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(badgedPicture);
        userPicture.setBounds(0, 0, mImageSize, mImageSize);
        userPicture.draw(canvas);

        // Cut a transparent hole through the background image.
        // This will serve as a border to the badge being overlaid.
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
        float badgeRadius = (float) badgeSize / 2;
        float badgeCenterX = badgeX + badgeRadius;
        float badgeCenterY = badgeY + badgeRadius;
        canvas.drawCircle(badgeCenterX, badgeCenterY, badgeRadius + borderSize, paint);

        // Draw the badge
        Drawable badge = badgeConfig.getBadge();
        badge.setBounds(badgeX, badgeY, badgeX + badgeSize, badgeY + badgeSize);
        badge.draw(canvas);
        return new BitmapDrawable(mContext.getResources(), badgedPicture);
    }

    private Drawable padAvatarForAiTierRing(Drawable userPicture) {
        Bitmap badgedPicture = Bitmap.createBitmap(mImageSize, mImageSize, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(badgedPicture);

        Rect oldBounds = userPicture.getBounds();
        userPicture.setBounds(
                mTotalPadding,
                mTotalPadding,
                mImageSize - mTotalPadding,
                mImageSize - mTotalPadding);
        userPicture.draw(canvas);
        userPicture.setBounds(oldBounds);

        return new BitmapDrawable(mContext.getResources(), badgedPicture);
    }

    private Drawable overlayAiRingOnAvatar(Drawable paddedPicture) {
        Bitmap badgedPicture = ((BitmapDrawable) paddedPicture).getBitmap();
        Canvas canvas = new Canvas(badgedPicture);

        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(mRingThicknessPx);

        RectF bounds = new RectF(0, 0, badgedPicture.getWidth(), badgedPicture.getHeight());
        float strokeInset = mRingThicknessPx / 2f;
        bounds.inset(strokeInset, strokeInset);

        if (mBrandingDelegate != null && !bounds.isEmpty()) {
            paint.setShader(mBrandingDelegate.getRingShader(bounds));
        } else {
            // Fall back to the default solid blue color if no delegate or shader is provided.
            paint.setColor(Color.BLUE);
            paint.setShader(null);
        }

        canvas.drawOval(bounds, paint);
        return paddedPicture;
    }

    private static Drawable getScaledPlaceholderImage(Context context, int imageSize) {
        Drawable drawable =
                AppCompatResources.getDrawable(context, R.drawable.logo_avatar_anonymous);
        assert drawable != null;
        Bitmap output = Bitmap.createBitmap(imageSize, imageSize, Config.ARGB_8888);
        Canvas canvas = new Canvas(output);
        // Fill the canvas with transparent color.
        canvas.drawColor(Color.TRANSPARENT);
        // Draw the placeholder on the canvas.
        drawable.setBounds(0, 0, imageSize, imageSize);
        drawable.draw(canvas);
        return new BitmapDrawable(context.getResources(), output);
    }

    private class AccountManagerAccountsChangeObserver implements AccountsChangeObserver {

        /** Implements {@link AccountsChangeObserver}. */
        @Override
        public void onAccountsChanged() {
            assert !SigninFeatureMap.isEnabled(
                    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS);
            updateCache(mAccountManagerFacade.getAccounts().getResult());
        }
    }

    private class IdentityManagerAccountsChangeObserver implements IdentityManager.Observer {

        /** Implements {@link IdentityManager.Observer}. */
        @Override
        public void onRefreshTokensLoaded(@RefreshTokensLoadingState int state) {
            assert SigninFeatureMap.isEnabled(
                    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS);
            updateCache();
        }

        /**
         * Implements {@link IdentityManager.Observer}.
         *
         * <p>TODO(crbug.com/569915868): Observe OnExtendedAccountInfoUpdated instead.
         */
        @Override
        public void onRefreshTokenUpdatedForAccount(CoreAccountInfo coreAccountInfo) {
            assert SigninFeatureMap.isEnabled(
                    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS);
            if (mIdentityManager.getRefreshTokensLoadingState()
                    != RefreshTokensLoadingState.SUCCEEDED) return;
            // Refresh token updates are sent for all accounts on every reload of accounts. Profile
            // data changes of known accounts are handled by onExtendedAccountInfoUpdated(), so only
            // newly added accounts have to be handled here. This is the first point where
            // getExtendedAccountInfoForAccountsWithRefreshToken() returns the new account, which
            // observers of onAccountsUpdated() rely on.
            if (mAccountsCache.containsKey(coreAccountInfo.getId())) return;
            var accountInfo =
                    mIdentityManager.findExtendedAccountInfoByAccountId(coreAccountInfo.getId());
            addToCache(assumeNonNull(accountInfo));
        }

        /**
         * Implements {@link IdentityManager.Observer}.
         *
         * <p>TODO(crbug.com/569915868): Observe OnExtendedAccountInfoRemoved instead.
         */
        @Override
        public void onRefreshTokenRemovedForAccount(CoreAccountId accountId) {
            assert SigninFeatureMap.isEnabled(
                    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS);
            if (mIdentityManager.getRefreshTokensLoadingState()
                    == RefreshTokensLoadingState.SUCCEEDED) {
                updateCache(mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
            }
        }
    }
}
