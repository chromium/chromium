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

import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.AvatarGenerator;
import org.chromium.components.signin.AccountEmailDisplayHook;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoService;
import org.chromium.components.signin.identitymanager.AccountInfoServiceProvider;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

/**
 * Fetches and caches Google Account profile images and full names for the accounts on the device.
 */
@MainThread
@NullMarked
public class ProfileDataCache implements AccountInfoService.Observer {
    /** Observer to get notifications about changes in profile data. */
    public interface Observer {
        /**
         * Notifies that an account's profile data has been updated.
         * @param accountEmail An account email.
         */
        void onProfileDataUpdated(String accountEmail);
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
    private final int mImageSize;
    // The badge for a given account is selected as follows:
    // * If there is a config for that specific account, use that
    // * Else if there is a default config, use that
    // * Else do not display a badge.
    private @Nullable BadgeConfig mDefaultBadgeConfig;
    private final Map<String, BadgeConfig> mPerAccountBadgeConfig = new HashMap<>();
    private final Drawable mPlaceholderImage;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final Map<String, DisplayableProfileData> mCachedProfileData = new HashMap<>();

    // TODO(crbug.com/342205162): Require native for ProfileDataCache creation.
    @VisibleForTesting
    ProfileDataCache(Context context, @Px int imageSize, @Nullable BadgeConfig badgeConfig) {
        mContext = context;
        mImageSize = imageSize;
        mDefaultBadgeConfig = badgeConfig;
        mPlaceholderImage = getScaledPlaceholderImage(context, imageSize);
        // TODO(crbug.com/341948846): Remove AccountInfoService and simplify this.
        Promise<AccountInfoService> accountInfoServicePromise =
                AccountInfoServiceProvider.getPromise();
        if (accountInfoServicePromise.isFulfilled()) {
            populateCache(accountInfoServicePromise.getResult());
        } else {
            accountInfoServicePromise.then(this::populateCache);
        }
    }

    /**
     * @param context Context of the application to extract resources from.
     * @return A {@link ProfileDataCache} object with default image size(R.dimen.user_picture_size)
     *     and no badge.
     */
    public static ProfileDataCache createWithDefaultImageSizeAndNoBadge(Context context) {
        return new ProfileDataCache(
                context,
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
            Context context, @DrawableRes int badgeResId) {
        return new ProfileDataCache(
                context,
                context.getResources().getDimensionPixelSize(R.dimen.user_picture_size),
                createDefaultSizeChildAccountBadgeConfig(context, badgeResId));
    }

    /**
     * @param context Context of the application to extract resources from.
     * @param imageSizeRedId Resource id of the image size.
     * @return A {@link ProfileDataCache} object with the given image size and no badge.
     */
    public static ProfileDataCache createWithoutBadge(
            Context context, @DimenRes int imageSizeRedId) {
        return new ProfileDataCache(
                context,
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
     * @return The {@link DisplayableProfileData} containing the profile data corresponding to the
     *     given account or a {@link DisplayableProfileData} with a placeholder image and null full
     *     and given name.
     */
    public DisplayableProfileData getProfileDataOrDefault(@Nullable String accountEmail) {
        DisplayableProfileData profileData = mCachedProfileData.get(accountEmail);
        if (profileData == null) {
            assumeNonNull(accountEmail);
            return new DisplayableProfileData(
                    accountEmail,
                    mPlaceholderImage,
                    null,
                    null,
                    AccountEmailDisplayHook.canHaveEmailAddressDisplayed(accountEmail));
        }
        return profileData;
    }

    /**
     * Sets a default {@link BadgeConfig} and then populates the cache with the new Badge.
     *
     * @param BadgeConfig The badge configuration. If null then the current badge is removed.
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
        mCachedProfileData.clear();
        AccountInfoServiceProvider.getPromise().then(this::populateCache);
    }

    /**
     * Sets a {@link BadgeConfig} for a given account, and then populates the cache with the new
     * Badge.
     *
     * @param accountEmail The account email for which to set this badge.
     * @param BadgeConfig The badge configuration. If null then the current badge is removed.
     *     <p>If both a per-account and default badge are set, the per-account badge takes
     *     precedence.
     *     <p>TODO(crbug.com/40274844): Replace accountEmail with CoreAccountId or CoreAccountInfo.
     */
    public void setBadge(String accountEmail, @Nullable BadgeConfig badgeConfig) {
        if (mPerAccountBadgeConfig.containsKey(accountEmail)
                && Objects.equals(mPerAccountBadgeConfig.get(accountEmail), badgeConfig)) {
            // Update is a no-op. The per-account badge set to accountEmail is the same as the
            // badgeResId.
            return;
        }
        mPerAccountBadgeConfig.put(accountEmail, badgeConfig);
        AccountInfoServiceProvider.getPromise()
                .then(
                        accountInfoService -> {
                            populateCacheForAccount(accountInfoService, accountEmail);
                        });
    }

    /**
     * @param observer Observer that should be notified when new profile images are available.
     */
    public void addObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        if (mObservers.isEmpty()) {
            AccountInfoServiceProvider.getPromise()
                    .then(
                            accountInfoService -> {
                                accountInfoService.addObserver(this);
                            });
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
            AccountInfoServiceProvider.getPromise()
                    .then(
                            accountInfoService -> {
                                accountInfoService.removeObserver(this);
                            });
        }
    }

    /** Implements {@link AccountInfoService.Observer}. */
    @Override
    public void onAccountInfoUpdated(@Nullable AccountInfo accountInfo) {
        // We don't update the cache if the account information and ProfileDataCache config mean
        // that we would just be returning the default profile data.
        if (accountInfo != null
                && (accountInfo.hasDisplayableInfo()
                        || getBadgeConfigForAccount(accountInfo.getEmail()) != null)) {
            updateCacheAndNotifyObservers(
                    accountInfo.getEmail(),
                    accountInfo.getAccountImage(),
                    accountInfo.getFullName(),
                    accountInfo.getGivenName(),
                    accountInfo.canHaveEmailAddressDisplayed());
        }
    }

    /**
     * @return Whether the cache contains non-default profile data for the given account.
     */
    public boolean hasProfileDataForTesting(String accountEmail) {
        return mCachedProfileData.containsKey(accountEmail);
    }

    private void populateCache(AccountInfoService accountInfoService) {
        var accountsPromise = AccountManagerFacadeProvider.getInstance().getAccounts();
        if (accountsPromise.isFulfilled()) {
            populateCacheForAllAccounts(accountInfoService, accountsPromise.getResult());
        } else {
            accountsPromise.then(
                    accounts -> {
                        populateCacheForAllAccounts(accountInfoService, accounts);
                    });
        }
    }

    private void populateCacheForAllAccounts(
            AccountInfoService accountInfoService, List<AccountInfo> accounts) {
        for (CoreAccountInfo account : accounts) {
            populateCacheForAccount(accountInfoService, account.getEmail());
        }
    }

    // TODO(crbug.com/40274844): Replace accountEmail with CoreAccountId or CoreAccountInfo.
    private void populateCacheForAccount(
            AccountInfoService accountInfoService, String accountEmail) {
        // TODO(crbug.com/341948846): Remove AccountInfoService and simplify this.
        Promise<@Nullable AccountInfo> accountInfoPromise =
                accountInfoService.getAccountInfoByEmail(accountEmail);
        if (accountInfoPromise.isFulfilled()) {
            onAccountInfoUpdated(accountInfoPromise.getResult());
        } else {
            accountInfoPromise.then(this::onAccountInfoUpdated);
        }
    }

    private void updateCacheAndNotifyObservers(
            String email,
            @Nullable Bitmap avatar,
            String fullName,
            String givenName,
            boolean hasDisplayableEmailAddress) {
        Drawable croppedAvatar =
                avatar != null
                        ? AvatarGenerator.makeRoundAvatar(
                                mContext.getResources(), avatar, mImageSize)
                        : mPlaceholderImage;
        BadgeConfig badgeConfig = getBadgeConfigForAccount(email);
        if (badgeConfig != null) {
            croppedAvatar = overlayBadgeOnUserPicture(badgeConfig, croppedAvatar);
        }
        mCachedProfileData.put(
                email,
                new DisplayableProfileData(
                        email, croppedAvatar, fullName, givenName, hasDisplayableEmailAddress));
        notifyObservers(email);
    }

    private void notifyObservers(String accountEmail) {
        for (Observer observer : mObservers) {
            observer.onProfileDataUpdated(accountEmail);
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

    private @Nullable BadgeConfig getBadgeConfigForAccount(String email) {
        return mPerAccountBadgeConfig.get(email) != null
                ? mPerAccountBadgeConfig.get(email)
                : mDefaultBadgeConfig;
    }
}
