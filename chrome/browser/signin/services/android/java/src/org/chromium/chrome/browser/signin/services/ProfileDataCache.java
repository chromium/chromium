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
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.DimenRes;
import androidx.annotation.DrawableRes;
import androidx.annotation.MainThread;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.AvatarGenerator;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
import org.chromium.components.signin.SigninFeatureMap;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.google_apis.gaia.CoreAccountId;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.function.Function;

/**
 * Fetches and caches Google Account profile images and full names for the accounts on the device.
 */
@MainThread
@NullMarked
public class ProfileDataCache implements IdentityManager.Observer {
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
    // The badge for a given account is selected as follows:
    // * If there is a config for that specific account, use that
    // * Else if there is a default config, use that
    // * Else do not display a badge.
    private @Nullable BadgeConfig mDefaultBadgeConfig;
    private final Map<CoreAccountId, BadgeConfig> mPerAccountBadgeConfig = new HashMap<>();
    private final Drawable mPlaceholderImage;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final AccountsCache mAccountsCache = new AccountsCache();

    @VisibleForTesting
    ProfileDataCache(
            Context context,
            AccountManagerFacade accountManagerFacade,
            IdentityManager identityManager,
            @Px int imageSize,
            @Nullable BadgeConfig badgeConfig) {
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
        mDefaultBadgeConfig = badgeConfig;
        mPlaceholderImage = getScaledPlaceholderImage(context, imageSize);
        updateCache();
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
                context.getResources().getDimensionPixelSize(R.dimen.user_picture_size),
                /* badgeConfig= */ null);
    }

    /**
     * @param context Context of the application to extract resources from.
     * @param badgeResId Resource id of the badge to be attached.
     * @return A {@link ProfileDataCache} object with default image size(R.dimen.user_picture_size)
     *     and a badge of given badgeResId provided
     *     <p>TODO(crbug.com/40798208): remove this method and instead migrate users to set
     *     per-account badges?
     */
    public static ProfileDataCache createWithDefaultImageSize(
            Context context, IdentityManager identityManager, @DrawableRes int badgeResId) {
        return new ProfileDataCache(
                context,
                AccountManagerFacadeProvider.getInstance(),
                identityManager,
                context.getResources().getDimensionPixelSize(R.dimen.user_picture_size),
                BadgeConfig.create(badgeResId).withDefaultSizeChildAccountConfig().build(context));
    }

    /**
     * @param context Context of the application to extract resources from.
     * @param imageSizeRedId Resource id of the image size.
     * @return A {@link ProfileDataCache} object with the given image size and no badge.
     */
    public static ProfileDataCache createWithoutBadge(
            Context context, IdentityManager identityManager, @DimenRes int imageSizeRedId) {
        return new ProfileDataCache(
                context,
                AccountManagerFacadeProvider.getInstance(),
                identityManager,
                context.getResources().getDimensionPixelSize(imageSizeRedId),
                /* badgeConfig= */ null);
    }

    /**
     * Gets the list of cached accounts that are synchronized with the device accounts.
     *
     * <p>Accounts data are populated from {@link IdentityManager}. To observe changes to accounts,
     * implement {@link Observer#onAccountsUpdated}.
     *
     * @return A {@link Promise} containing the list of cached {@link DisplayableProfileData}
     *     accounts.
     */
    public Promise<List<DisplayableProfileData>> getAccounts() {
        return mAccountsCache.getAll();
    }

    /**
     * Returns cached {@link DisplayableProfileData} for the given account ID.
     *
     * <p>Method is synchronous and does not trigger any account info fetches. First it checks if
     * the {@link DisplayableProfileData} is in the cache, then it updates the cache if the account
     * is missing. Throws an {@link IllegalArgumentException} if the account still cannot be found.
     *
     * @param accountId The account ID for which to get the profile data.
     * @throws IllegalArgumentException if the account is not found.
     * @return The {@link DisplayableProfileData} for the given account ID.
     */
    public DisplayableProfileData getById(CoreAccountId accountId) {
        if (!mAccountsCache.isLoaded() || !mAccountsCache.contains(accountId)) {
            updateCache();
        }

        var profileData = mAccountsCache.getByAccountId(accountId);
        if (profileData != null) {
            return profileData;
        }

        throw new IllegalArgumentException("Account not found");
    }

    /**
     * Sets a default {@link BadgeConfig} and then populates the cache with the new Badge.
     *
     * @param badgeConfig The badge configuration. If null then the current badge is removed.
     *     <p>If both a per-account and default badge are set, the per-account badge takes
     *     precedence.
     *     <p>TODO(crbug.com/40798208): replace usages of this method with the per-account config
     *     below.
     */
    public void setBadge(@Nullable BadgeConfig badgeConfig) {
        if (Objects.equals(mDefaultBadgeConfig, badgeConfig)) {
            return;
        }

        mDefaultBadgeConfig = badgeConfig;
        updateCache();
    }

    /**
     * Sets a {@link BadgeConfig} for a given account, and then populates the cache with the new
     * Badge.
     *
     * @param coreAccountId The account id for which to set this badge.
     * @param badgeConfig The badge configuration. If null then the current badge is removed.
     *     <p>If both a per-account and default badge are set, the per-account badge takes
     *     precedence.
     */
    public void setBadge(CoreAccountId accountId, @Nullable BadgeConfig badgeConfig) {
        if (mPerAccountBadgeConfig.containsKey(accountId)
                && Objects.equals(mPerAccountBadgeConfig.get(accountId), badgeConfig)) {
            // Update is a no-op. The per-account badge set to accountId is the same as the
            // badgeResId.
            return;
        }
        mPerAccountBadgeConfig.put(accountId, badgeConfig);
        var accountInfo = findAccountInfo(accountId);
        if (accountInfo != null) {
            var displayableProfileData = toDisplayableProfileData(accountInfo);
            mAccountsCache.putAccount(accountInfo.getId(), displayableProfileData);
            fireOnProfileDataUpdated(displayableProfileData);
        }
    }

    public @Nullable BadgeConfig getBadgeConfigForTesting(CoreAccountId accountId) {
        return getBadgeConfigForAccount(accountId);
    }

    /**
     * @param observer Observer that should be notified when new profile images are available.
     */
    public void addObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        if (mObservers.isEmpty()) {
            if (SigninFeatureMap.isEnabled(
                    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)) {
                mIdentityManager.addObserver(assumeNonNull(mIdentityManagerAccountsChangeObserver));
            } else {
                mAccountManagerFacade.addObserver(
                        assumeNonNull(mAccountManagerAccountsChangeObserver));
            }
            mIdentityManager.addObserver(this);
        }
        mObservers.addObserver(observer);
    }

    /**
     * @param observer Observer that was added by {@link #addObserver} and should be removed.
     */
    public void removeObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        mObservers.removeObserver(observer);
        if (mObservers.isEmpty()) {
            mIdentityManager.removeObserver(this);
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

    /** Implements {@link IdentityManager.Observer}. */
    @Override
    public void onExtendedAccountInfoUpdated(AccountInfo accountInfo) {
        if (mIdentityManager.findExtendedAccountInfoByAccountId(accountInfo.getId()) == null) {
            // Account was removed from the IdentityManager.
            // Cache will be updated by onRefreshTokenRemovedForAccount() callback.
            return;
        }
        var displayableProfileData = toDisplayableProfileData(accountInfo);
        mAccountsCache.putAccount(accountInfo.getId(), displayableProfileData);
        fireOnProfileDataUpdated(displayableProfileData);
    }

    /** Checks if the cache contains profile data for the given account ID. */
    public boolean hasProfileDataForTesting(CoreAccountId accountId) {
        return mAccountsCache.getByAccountId(accountId) != null;
    }

    private void updateCache() {
        final @Nullable List<AccountInfo> coreAccounts = getCoreAccountsIfLoaded();
        final @Nullable AccountInfo primaryAccountInfo = getPrimaryAccountInfo();

        if (coreAccounts == null && primaryAccountInfo == null) {
            return;
        }

        // Primary account can be set before list of all accounts is loaded. To avoid that case, we
        // need to add primary account to the list of all accounts manually. That makes potential
        // duplicate on the list, but it will be handled by the updateCache(List<AccountInfo>)
        // method.
        final var allAccounts =
                coreAccounts != null ? new ArrayList<>(coreAccounts) : new ArrayList<AccountInfo>();
        if (primaryAccountInfo != null) {
            allAccounts.add(primaryAccountInfo);
        }
        updateCache(allAccounts);
    }

    private void updateCache(List<AccountInfo> accounts) {
        var displayableAccounts = new LinkedHashMap<CoreAccountId, DisplayableProfileData>();
        for (AccountInfo account : accounts) {
            // Accounts list is combined from accounts with refresh tokens and the primary account
            // at the last position. Because list of accounts is manually combined, there is a
            // chance that the primary account is duplicated. We want to use computeIfAbsent here to
            // avoid overriding the existing entry and double avatar generation.
            displayableAccounts.computeIfAbsent(
                    account.getId(),
                    id -> {
                        var extendedAccountInfo =
                                mIdentityManager.findExtendedAccountInfoByAccountId(id);
                        return toDisplayableProfileData(
                                extendedAccountInfo != null ? extendedAccountInfo : account);
                    });
        }
        mAccountsCache.setAccounts(displayableAccounts);

        mAccountsCache
                .getAll()
                .then((Callback<List<DisplayableProfileData>>) this::fireOnAccountsUpdated);
        // TODO(crbug.com/485130949): Remove that callback after implementation of
        // onAccountsUpdated() in all UIs. (Blocked by crbug.com/480239119)
        mAccountsCache
                .getAll()
                .then((Callback<List<DisplayableProfileData>>) this::fireOnProfileDataUpdated);
    }

    private DisplayableProfileData toDisplayableProfileData(AccountInfo accountInfo) {
        Drawable croppedAvatar =
                accountInfo.getAccountImage() != null
                        ? AvatarGenerator.makeRoundAvatar(
                                mContext.getResources(), accountInfo.getAccountImage(), mImageSize)
                        : mPlaceholderImage;
        BadgeConfig badgeConfig = getBadgeConfigForAccount(accountInfo.getId());
        if (badgeConfig != null) {
            croppedAvatar = overlayBadgeOnUserPicture(badgeConfig, croppedAvatar);
        }

        if (SigninFeatureMap.isEnabled(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)) {
            return new DisplayableProfileData(
                    accountInfo.getId(),
                    accountInfo.getEmail(),
                    croppedAvatar,
                    accountInfo.getFullName(),
                    accountInfo.getGivenName(),
                    accountInfo.canHaveEmailAddressDisplayed());
        } else {
            final var shouldPopulateNames = accountInfo.hasDisplayableInfo() || badgeConfig != null;
            return new DisplayableProfileData(
                    accountInfo.getId(),
                    accountInfo.getEmail(),
                    croppedAvatar,
                    shouldPopulateNames ? accountInfo.getFullName() : null,
                    shouldPopulateNames ? accountInfo.getGivenName() : null,
                    accountInfo.canHaveEmailAddressDisplayed());
        }
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
        var primaryAccountInfo = getPrimaryAccountInfo();
        if (primaryAccountInfo != null && primaryAccountInfo.getId().equals(accountId)) {
            return primaryAccountInfo;
        }
        return null;
    }

    private @Nullable AccountInfo getPrimaryAccountInfo() {
        var primaryAccountInfo = mIdentityManager.getPrimaryAccountInfo();
        if (primaryAccountInfo == null) {
            return null;
        }
        return new AccountInfo.Builder(primaryAccountInfo).build();
    }

    private @Nullable List<AccountInfo> getCoreAccountsIfLoaded() {
        if (SigninFeatureMap.isEnabled(SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS)) {
            if (mIdentityManager.areRefreshTokensLoaded()) {
                return mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken();
            }
            return null;
        } else {
            var accounts = mAccountManagerFacade.getAccounts();
            if (accounts.isFulfilled()) {
                return accounts.getResult();
            }
            return null;
        }
    }

    // TODO(crbug.com/40944114): Consider using UiUtils.drawIconWithBadge instead.
    private Drawable overlayBadgeOnUserPicture(BadgeConfig badgeConfig, Drawable userPicture) {
        int badgeSize = badgeConfig.getBadgeSize();
        int badgedPictureWidth = Math.max(badgeConfig.getPosition().x + badgeSize, mImageSize);
        int badgedPictureHeight = Math.max(badgeConfig.getPosition().y + badgeSize, mImageSize);
        Bitmap badgedPicture =
                Bitmap.createBitmap(
                        badgedPictureWidth, badgedPictureHeight, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(badgedPicture);
        userPicture.setBounds(0, 0, mImageSize, mImageSize);
        userPicture.draw(canvas);

        // Cut a transparent hole through the background image.
        // This will serve as a border to the badge being overlaid.
        Paint paint = new Paint();
        paint.setAntiAlias(true);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
        float badgeRadius = (float) badgeSize / 2;
        float badgeCenterX = badgeConfig.getPosition().x + badgeRadius;
        float badgeCenterY = badgeConfig.getPosition().y + badgeRadius;
        canvas.drawCircle(
                badgeCenterX, badgeCenterY, badgeRadius + badgeConfig.getBorderSize(), paint);

        // Draw the badge
        Drawable badge = badgeConfig.getBadge();
        badge.setBounds(
                badgeConfig.getPosition().x,
                badgeConfig.getPosition().y,
                badgeConfig.getPosition().x + badgeSize,
                badgeConfig.getPosition().y + badgeSize);
        badge.draw(canvas);
        return new BitmapDrawable(mContext.getResources(), badgedPicture);
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

    private @Nullable BadgeConfig getBadgeConfigForAccount(CoreAccountId accountId) {
        return mPerAccountBadgeConfig.get(accountId) != null
                ? mPerAccountBadgeConfig.get(accountId)
                : mDefaultBadgeConfig;
    }

    private class AccountManagerAccountsChangeObserver implements AccountsChangeObserver {

        /** Implements {@link AccountsChangeObserver}. */
        @Override
        public void onCoreAccountInfosChanged() {
            assert !SigninFeatureMap.isEnabled(
                    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS);
            updateCache(mAccountManagerFacade.getAccounts().getResult());
        }
    }

    private class IdentityManagerAccountsChangeObserver implements IdentityManager.Observer {

        /** Implements {@link IdentityManager.Observer}. */
        @Override
        public void onRefreshTokensLoaded() {
            assert SigninFeatureMap.isEnabled(
                    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS);
            updateCache(mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
        }

        /** Implements {@link IdentityManager.Observer}. */
        @Override
        public void onRefreshTokenUpdatedForAccount(CoreAccountInfo coreAccountInfo) {
            assert SigninFeatureMap.isEnabled(
                    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS);
            if (mIdentityManager.areRefreshTokensLoaded()) {
                updateCache(mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
            }
        }

        /** Implements {@link IdentityManager.Observer}. */
        @Override
        public void onRefreshTokenRemovedForAccount(CoreAccountId accountId) {
            assert SigninFeatureMap.isEnabled(
                    SigninFeatures.MAKE_IDENTITY_MANAGER_SOURCE_OF_ACCOUNTS);
            if (mIdentityManager.areRefreshTokensLoaded()) {
                updateCache(mIdentityManager.getExtendedAccountInfoForAccountsWithRefreshToken());
            }
        }
    }

    private static final class AccountsCache {

        private Promise<Map<CoreAccountId, DisplayableProfileData>> mAccounts = new Promise<>();

        private Promise<List<DisplayableProfileData>> getAll() {
            if (mAccounts.isFulfilled()) {
                return Promise.fulfilled(new ArrayList<>(mAccounts.getResult().values()));
            } else {
                return mAccounts.then(
                        (Function<
                                        Map<CoreAccountId, DisplayableProfileData>,
                                        List<DisplayableProfileData>>)
                                accounts -> new ArrayList<>(accounts.values()));
            }
        }

        private void setAccounts(Map<CoreAccountId, DisplayableProfileData> accounts) {
            if (mAccounts.isFulfilled()) {
                mAccounts = Promise.fulfilled(accounts);
            } else {
                mAccounts.fulfill(accounts);
            }
        }

        private void putAccount(CoreAccountId accountId, DisplayableProfileData profileData) {
            if (mAccounts.isFulfilled()) {
                final var accounts = mAccounts.getResult();
                accounts.put(accountId, profileData);
                mAccounts = Promise.fulfilled(accounts);
            } else {
                final var accounts = new LinkedHashMap<CoreAccountId, DisplayableProfileData>();
                accounts.put(accountId, profileData);
                mAccounts.fulfill(accounts);
            }
        }

        private @Nullable DisplayableProfileData getByAccountId(CoreAccountId accountId) {
            if (mAccounts.isFulfilled()) {
                return mAccounts.getResult().get(accountId);
            }
            return null;
        }

        private boolean contains(final CoreAccountId accountId) {
            return getByAccountId(accountId) != null;
        }

        private boolean isLoaded() {
            return mAccounts.isFulfilled();
        }
    }
}
