// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Point;
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
import org.chromium.components.signin.AccountEmailDisplayHook;
import org.chromium.components.signin.AccountManagerFacade;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountsChangeObserver;
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
public class ProfileDataCache implements IdentityManager.Observer, AccountsChangeObserver {
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

    /**
     * Encapsulates info necessary to overlay a circular badge (e.g., child account icon) on top of
     * a user avatar.
     */
    private static class BadgeConfig {
        private final int mBadgeResId;
        private final Drawable mBadge;
        private final @Px int mBadgeSize;
        private final @Px int mBorderSize;
        private final Point mPosition;

        private BadgeConfig(
                Context context,
                @DrawableRes int badgeResId,
                @Px int badgeSize,
                @Px int borderSize,
                Point position) {
            assert badgeResId != 0;

            mBadgeResId = badgeResId;
            mBadge = AppCompatResources.getDrawable(context, badgeResId);
            mBadgeSize = badgeSize;
            mBorderSize = borderSize;
            mPosition = position;
        }

        Drawable getBadge() {
            return mBadge;
        }

        @Px
        int getBadgeSize() {
            return mBadgeSize;
        }

        @Px
        int getBorderSize() {
            return mBorderSize;
        }

        Point getPosition() {
            return mPosition;
        }

        @Override
        public boolean equals(Object o) {
            return o instanceof BadgeConfig bc
                    && mBadgeResId == bc.mBadgeResId
                    && mBadgeSize == bc.mBadgeSize
                    && mBorderSize == bc.mBorderSize
                    && mPosition.equals(bc.mPosition);
        }

        @Override
        public int hashCode() {
            return Objects.hash(mBadgeResId, mBadgeSize, mBorderSize, mPosition);
        }
    }

    private final Context mContext;
    private final AccountManagerFacade mAccountManagerFacade;
    private final IdentityManager mIdentityManager;
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
                createDefaultSizeChildAccountBadgeConfig(context, badgeResId));
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
     * Creates a {@link BadgeConfig} with default badge size.
     *
     * @param context Context of the application to extract resources from.
     * @param badgeResId Resource id of the badge to be attached.
     * @return A {@link BadgeConfig} with default badge size(R.dimen.badge_size) of given badgeResId
     *     provided.
     */
    public static BadgeConfig createDefaultSizeChildAccountBadgeConfig(
            Context context, @DrawableRes int badgeResId) {
        assert badgeResId != 0;

        Resources resources = context.getResources();
        return new BadgeConfig(
                context,
                badgeResId,
                resources.getDimensionPixelSize(R.dimen.badge_size),
                resources.getDimensionPixelSize(R.dimen.badge_border_size),
                new Point(
                        resources.getDimensionPixelOffset(R.dimen.badge_position_x),
                        resources.getDimensionPixelOffset(R.dimen.badge_position_y)));
    }

    /**
     * Creates a {@link BadgeConfig} with toolbar identity disc badge size.
     *
     * @param context Context of the application to extract resources from.
     * @param badgeResId Resource id of the badge to be attached.
     * @return A {@link BadgeConfig} with toolbar identity disc badge size badge
     *     size(R.dimen.toolbar_identity_disc_badge_size) of given badgeResId provided.
     */
    public static BadgeConfig createToolbarIdentityDiscBadgeConfig(
            Context context, @DrawableRes int badgeResId) {
        assert badgeResId != 0;

        Resources resources = context.getResources();
        return new BadgeConfig(
                context,
                badgeResId,
                resources.getDimensionPixelSize(R.dimen.toolbar_identity_disc_badge_size),
                resources.getDimensionPixelSize(R.dimen.toolbar_identity_disc_badge_border_size),
                new Point(
                        resources.getDimensionPixelOffset(
                                R.dimen.toolbar_identity_disc_badge_position_x),
                        resources.getDimensionPixelOffset(
                                R.dimen.toolbar_identity_disc_badge_position_y)));
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
     * @return The {@link DisplayableProfileData} containing the profile data corresponding to the
     *     given account or a {@link DisplayableProfileData} with a placeholder image and null full
     *     and given name.
     */
    public DisplayableProfileData getProfileDataOrDefault(@Nullable String accountEmail) {
        DisplayableProfileData profileData =
                accountEmail != null ? mAccountsCache.getByEmail(accountEmail) : null;
        if (profileData == null) {
            return createDefaultProfileData(assumeNonNull(accountEmail));
        }
        return profileData;
    }

    /**
     * Returns cached {@link DisplayableProfileData} for the given account ID.
     *
     * <p>Method is synchronous and does not trigger any account info fetches. First it checks if
     * the {@link DisplayableProfileData} is in the cache, then it falls back to the {@link
     * AccountManagerFacade} to find an email value and create a limited {@link
     * DisplayableProfileData} object. Throws an exception if the account is not found.
     *
     * @param accountId The account ID for which to get the profile data.
     * @throws IllegalArgumentException if the account is not found.
     * @return The {@link DisplayableProfileData} for the given account ID.
     */
    public DisplayableProfileData getById(CoreAccountId accountId) {
        var profileData = mAccountsCache.getByAccountId(accountId);
        if (profileData != null) {
            return profileData;
        }

        // TODO(https://crbug.com/483627535): Remove that fallback to AccountManagerFacade after
        // full migration to IdentityManager.
        var accounts = mAccountManagerFacade.getAccounts();
        if (accounts.isFulfilled()) {
            for (var account : accounts.getResult()) {
                if (account.getId().equals(accountId)) {
                    return createDefaultProfileData(account.getEmail());
                }
            }
        }

        throw new IllegalArgumentException("Account not found");
    }

    private DisplayableProfileData createDefaultProfileData(String accountEmail) {
        return new DisplayableProfileData(
                accountEmail,
                mPlaceholderImage,
                null,
                null,
                AccountEmailDisplayHook.canHaveEmailAddressDisplayed(accountEmail));
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
            // Update is a no-op. The per-account badge set to accountEmail is the same as the
            // badgeResId.
            return;
        }
        mPerAccountBadgeConfig.put(accountId, badgeConfig);
        var accountInfo = mIdentityManager.findExtendedAccountInfoByAccountId(accountId);
        onExtendedAccountInfoUpdated(accountInfo);
    }

    /**
     * @param observer Observer that should be notified when new profile images are available.
     */
    public void addObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        if (mObservers.isEmpty()) {
            mAccountManagerFacade.addObserver(this);
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
            mAccountManagerFacade.removeObserver(this);
            mIdentityManager.removeObserver(this);
        }
    }

    @Override
    public void onCoreAccountInfosChanged() {
        updateCache();
    }

    /** Implements {@link IdentityManager.Observer}. */
    @Override
    public void onExtendedAccountInfoUpdated(@Nullable AccountInfo accountInfo) {
        // We don't update the cache if the account information and ProfileDataCache config mean
        // that we would just be returning the default profile data.
        if (accountInfo != null
                && (accountInfo.hasDisplayableInfo()
                        || getBadgeConfigForAccount(accountInfo.getId()) != null)) {
            var displayableProfileData = toDisplayableProfileData(accountInfo);
            mAccountsCache.putAccount(
                    new AccountsCache.AccountEntry(accountInfo.getId(), displayableProfileData));
            fireOnProfileDataUpdated(displayableProfileData);
        }
    }

    /**
     * @return Whether the cache contains non-default profile data for the given account.
     */
    public boolean hasProfileDataForTesting(String accountEmail) {
        return mAccountsCache.getByEmail(accountEmail) != null;
    }

    private void updateCache() {
        var accounts = mAccountManagerFacade.getAccounts();
        if (!accounts.isFulfilled()) {
            // When the list of accounts is ready - onCoreAccountInfosChanged will call
            // updateCache again.
            return;
        }

        List<AccountsCache.AccountEntry> displayableAccounts = new ArrayList<>();
        for (CoreAccountInfo account : accounts.getResult()) {
            var accountInfo = mIdentityManager.findExtendedAccountInfoByAccountId(account.getId());
            if (accountInfo != null
                    && (accountInfo.hasDisplayableInfo()
                            || getBadgeConfigForAccount(accountInfo.getId()) != null)) {
                displayableAccounts.add(
                        new AccountsCache.AccountEntry(
                                accountInfo.getId(), toDisplayableProfileData(accountInfo)));
            }
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
        return new DisplayableProfileData(
                accountInfo.getEmail(),
                croppedAvatar,
                accountInfo.getFullName(),
                accountInfo.getGivenName(),
                accountInfo.canHaveEmailAddressDisplayed());
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

        private void setAccounts(List<AccountEntry> accounts) {
            var accountsMap = new LinkedHashMap<CoreAccountId, DisplayableProfileData>();
            for (var account : accounts) {
                accountsMap.put(account.mAccountId, account.mProfileData);
            }

            if (mAccounts.isFulfilled()) {
                mAccounts = Promise.fulfilled(accountsMap);
            } else {
                mAccounts.fulfill(accountsMap);
            }
        }

        private void putAccount(AccountEntry account) {
            if (mAccounts.isFulfilled()) {
                final var accounts = mAccounts.getResult();
                accounts.put(account.mAccountId, account.mProfileData);
                mAccounts = Promise.fulfilled(accounts);
            } else {
                final var accounts = new LinkedHashMap<CoreAccountId, DisplayableProfileData>();
                accounts.put(account.mAccountId, account.mProfileData);
                mAccounts.fulfill(accounts);
            }
        }

        private @Nullable DisplayableProfileData getByEmail(String email) {
            if (mAccounts.isFulfilled()) {
                for (var account : mAccounts.getResult().values()) {
                    if (account.getAccountEmail().equals(email)) {
                        return account;
                    }
                }
            }
            return null;
        }

        private @Nullable DisplayableProfileData getByAccountId(CoreAccountId accountId) {
            if (mAccounts.isFulfilled()) {
                return mAccounts.getResult().get(accountId);
            }
            return null;
        }

        private static final class AccountEntry {
            private final CoreAccountId mAccountId;
            private final DisplayableProfileData mProfileData;

            private AccountEntry(CoreAccountId accountId, DisplayableProfileData profileData) {
                mAccountId = accountId;
                mProfileData = profileData;
            }
        }
    }
}
