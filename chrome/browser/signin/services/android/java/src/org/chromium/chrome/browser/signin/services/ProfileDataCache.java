// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.accounts.Account;
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
import androidx.annotation.Nullable;
import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.util.AvatarGenerator;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.ProfileDataSource.ProfileData;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.AccountInfoService;

import java.util.HashMap;
import java.util.Map;

/**
 * Fetches and caches Google Account profile images and full names for the accounts on the device.
 */
@MainThread
public class ProfileDataCache implements ProfileDataSource.Observer, AccountInfoService.Observer {
    /**
     * Observer to get notifications about changes in profile data.
     */
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
        private final Drawable mBadge;
        private final @Px int mBadgeSize;
        private final Point mPosition;
        private final @Px int mBorderSize;

        private BadgeConfig(Context context, @DrawableRes int badgeResId) {
            assert badgeResId != 0 : "badgeResId shouldn't be 0!";
            Resources resources = context.getResources();
            mBadge = AppCompatResources.getDrawable(context, badgeResId);
            mBadgeSize = resources.getDimensionPixelSize(R.dimen.badge_size);
            mPosition = new Point(resources.getDimensionPixelOffset(R.dimen.badge_position_x),
                    resources.getDimensionPixelOffset(R.dimen.badge_position_y));
            mBorderSize = resources.getDimensionPixelSize(R.dimen.badge_border_size);
        }

        Drawable getBadge() {
            return mBadge;
        }

        @Px
        int getBadgeSize() {
            return mBadgeSize;
        }

        Point getPosition() {
            return mPosition;
        }

        @Px
        int getBorderSize() {
            return mBorderSize;
        }
    }

    private final Context mContext;
    private final int mImageSize;
    private @Nullable final BadgeConfig mBadgeConfig;
    private final Drawable mPlaceholderImage;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final Map<String, DisplayableProfileData> mCachedProfileData = new HashMap<>();
    private final @Nullable ProfileDataSource mProfileDataSource;
    private final AccountInfoService mAccountInfoService;

    @VisibleForTesting
    ProfileDataCache(Context context, @Px int imageSize, @Nullable BadgeConfig badgeConfig) {
        mContext = context;
        mImageSize = imageSize;
        mBadgeConfig = badgeConfig;
        mPlaceholderImage = getScaledPlaceholderImage(context, imageSize);
        mProfileDataSource = ChromeFeatureList.isEnabled(ChromeFeatureList.DEPRECATE_MENAGERIE_API)
                ? null
                : AccountManagerFacadeProvider.getInstance().getProfileDataSource();
        mAccountInfoService = AccountInfoService.get();
    }

    /**
     * @param context Context of the application to extract resources from.
     * @return A {@link ProfileDataCache} object with default image size(R.dimen.user_picture_size)
     *         and no badge.
     */
    public static ProfileDataCache createWithDefaultImageSizeAndNoBadge(Context context) {
        return new ProfileDataCache(context,
                context.getResources().getDimensionPixelSize(R.dimen.user_picture_size),
                /*badgeConfig=*/null);
    }

    /**
     * @param context Context of the application to extract resources from.
     * @param badgeResId Resource id of the badge to be attached. If it is 0 then no badge is
     *         attached.
     * @return A {@link ProfileDataCache} object with default image size(R.dimen.user_picture_size)
     *         and a badge of given badgeResId provided
     */
    public static ProfileDataCache createWithDefaultImageSize(
            Context context, @DrawableRes int badgeResId) {
        return new ProfileDataCache(context,
                context.getResources().getDimensionPixelSize(R.dimen.user_picture_size),
                new BadgeConfig(context, badgeResId));
    }

    /**
     * @param context Context of the application to extract resources from.
     * @param imageSizeRedId Resource id of the image size.
     * @return A {@link ProfileDataCache} object with the given image size and no badge.
     */
    public static ProfileDataCache createWithoutBadge(
            Context context, @DimenRes int imageSizeRedId) {
        return new ProfileDataCache(context,
                context.getResources().getDimensionPixelSize(imageSizeRedId), /*badgeConfig=*/null);
    }

    /**
     * @return The {@link DisplayableProfileData} containing the profile data corresponding to the
     *         given account or a {@link DisplayableProfileData} with a placeholder image and null
     *         full and given name.
     */
    public DisplayableProfileData getProfileDataOrDefault(String accountEmail) {
        DisplayableProfileData profileData = mCachedProfileData.get(accountEmail);
        if (profileData == null) {
            return new DisplayableProfileData(accountEmail, mPlaceholderImage, null, null);
        }
        return profileData;
    }

    /**
     * @param observer Observer that should be notified when new profile images are available.
     */
    public void addObserver(Observer observer) {
        ThreadUtils.assertOnUiThread();
        if (mObservers.isEmpty()) {
            if (mProfileDataSource != null) {
                mProfileDataSource.addObserver(this);
            }
            mAccountInfoService.addObserver(this);
            populateCache();
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
            if (mProfileDataSource != null) {
                mProfileDataSource.removeObserver(this);
            }
            mAccountInfoService.removeObserver(this);
        }
    }

    private void populateCache() {
        AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts(accounts -> {
            for (Account account : accounts) {
                updateAccountInfoByEmail(account.name);
            }
        });
    }

    @Override
    public void onProfileDataUpdated(ProfileDataSource.ProfileData profileData) {
        ThreadUtils.assertOnUiThread();
        final String email = profileData.getAccountEmail();
        Bitmap avatar = profileData.getAvatar();
        if (avatar == null) {
            // If the avatar is null, try to fetch the monogram from IdentityManager
            final AccountInfo accountInfo = mAccountInfoService.getAccountInfoByEmail(email);
            avatar = accountInfo != null ? accountInfo.getAccountImage() : null;
        }
        updateCacheAndNotifyObservers(
                email, avatar, profileData.getFullName(), profileData.getGivenName());
    }

    @Override
    public void removeProfileData(String accountEmail) {
        mCachedProfileData.remove(accountEmail);
        notifyObservers(accountEmail);
    }

    /**
     * Implements {@link AccountInfoService.Observer}.
     */
    @Override
    public void onAccountInfoUpdated(AccountInfo accountInfo) {
        if (accountInfo.hasDisplayableInfo()) {
            updateCacheAndNotifyObservers(accountInfo.getEmail(), accountInfo.getAccountImage(),
                    accountInfo.getFullName(), accountInfo.getGivenName());
        }
    }

    private void updateAccountInfoByEmail(String email) {
        if (mProfileDataSource != null) {
            ProfileData profileData = mProfileDataSource.getProfileDataForAccount(email);
            if (profileData != null) {
                onProfileDataUpdated(profileData);
            }
            return;
        }
        final AccountInfo accountInfo = mAccountInfoService.getAccountInfoByEmail(email);
        if (accountInfo != null) {
            onAccountInfoUpdated(accountInfo);
        }
    }

    private void updateCacheAndNotifyObservers(
            String email, Bitmap avatar, String fullName, String givenName) {
        Drawable croppedAvatar = avatar != null
                ? AvatarGenerator.makeRoundAvatar(mContext.getResources(), avatar, mImageSize)
                : mPlaceholderImage;
        if (mBadgeConfig != null) {
            croppedAvatar = overlayBadgeOnUserPicture(croppedAvatar);
        }
        mCachedProfileData.put(
                email, new DisplayableProfileData(email, croppedAvatar, fullName, givenName));
        notifyObservers(email);
    }

    private void notifyObservers(String accountEmail) {
        for (Observer observer : mObservers) {
            observer.onProfileDataUpdated(accountEmail);
        }
    }

    private Drawable overlayBadgeOnUserPicture(Drawable userPicture) {
        int badgeSize = mBadgeConfig.getBadgeSize();
        int badgedPictureWidth = Math.max(mBadgeConfig.getPosition().x + badgeSize, mImageSize);
        int badgedPictureHeight = Math.max(mBadgeConfig.getPosition().y + badgeSize, mImageSize);
        Bitmap badgedPicture = Bitmap.createBitmap(
                badgedPictureWidth, badgedPictureHeight, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(badgedPicture);
        userPicture.setBounds(0, 0, mImageSize, mImageSize);
        userPicture.draw(canvas);

        // Cut a transparent hole through the background image.
        // This will serve as a border to the badge being overlaid.
        Paint paint = new Paint();
        paint.setAntiAlias(true);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
        int badgeRadius = badgeSize / 2;
        int badgeCenterX = mBadgeConfig.getPosition().x + badgeRadius;
        int badgeCenterY = mBadgeConfig.getPosition().y + badgeRadius;
        canvas.drawCircle(
                badgeCenterX, badgeCenterY, badgeRadius + mBadgeConfig.getBorderSize(), paint);

        // Draw the badge
        Drawable badge = mBadgeConfig.getBadge();
        badge.setBounds(mBadgeConfig.getPosition().x, mBadgeConfig.getPosition().y,
                mBadgeConfig.getPosition().x + badgeSize, mBadgeConfig.getPosition().y + badgeSize);
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
}
