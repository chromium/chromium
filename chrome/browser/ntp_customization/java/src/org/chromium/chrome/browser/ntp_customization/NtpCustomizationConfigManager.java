// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Bitmap;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.ntp_customization.theme.BackgroundImageInfo;
import org.chromium.chrome.browser.ntp_customization.theme.chrome_colors.NtpThemeColorInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.concurrent.Executor;

/** Manages the NTP's background configuration and notifies listeners of changes. */
@NullMarked
public class NtpCustomizationConfigManager {
    public static final int COLOR_NOT_SET = -1;
    private static final Executor EXECUTOR =
            (Runnable r) -> PostTask.postTask(TaskTraits.USER_BLOCKING_MAY_BLOCK, r);

    private boolean mIsInitialized;
    private @NtpBackgroundImageType int mBackgroundImageType;
    private @Nullable Bitmap mOriginalBitmap;
    private @Nullable BackgroundImageInfo mBackgroundImageInfo;
    private @ColorInt int mBackgroundColor;
    private boolean mIsMvtToggleOn;

    /** An interface to get NewTabPage's configuration updates. */
    public interface HomepageStateListener {
        /** Called when the state of the toggle for the Most Visited Tiles section changes. */
        default void onMvtToggleChanged() {}

        /**
         * Called when a customized homepage background image is chosen.
         *
         * @param originalBitmap The new background image drawable.
         * @param backgroundImageInfo The {@link BackgroundImageInfo} object containing the portrait
         *     and landscape matrices.
         * @param fromInitialization Whether the update of the background comes from the
         *     initialization of the {@link NtpCustomizationConfigManager}, i.e, loading the image
         *     from the device.
         * @param oldType The previously set background type for NTPs.
         * @param newType The new background type of NTPs.
         */
        default void onBackgroundChanged(
                Bitmap originalBitmap,
                @Nullable BackgroundImageInfo backgroundImageInfo,
                boolean fromInitialization,
                @NtpBackgroundImageType int oldType,
                @NtpBackgroundImageType int newType) {}

        /**
         * Called when the user chooses a customized homepage background color or resets to the
         * default Chrome's color.
         *
         * @param backgroundColor The new background color.
         * @param fromInitialization Whether the update of the background comes from the
         *     initialization of the {@link NtpCustomizationConfigManager}, i.e, loading the image
         *     from the device.
         * @param oldType The previously set background type for NTPs.
         * @param newType The new background type of NTPs.
         */
        default void onBackgroundColorChanged(
                @ColorInt int backgroundColor,
                boolean fromInitialization,
                @NtpBackgroundImageType int oldType,
                @NtpBackgroundImageType int newType) {}

        /**
         * Called to notify observers to get refreshed system's window insets.
         *
         * @param consumeTopInset Whether the observer should consume the new window insets.
         */
        default void refreshWindowInsets(boolean consumeTopInset) {}
    }

    private static @Nullable NtpCustomizationConfigManager sInstanceForTesting;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static final NtpCustomizationConfigManager sInstance = new NtpCustomizationConfigManager();
    }

    private final ObserverList<HomepageStateListener> mHomepageStateListeners;

    /** Returns the singleton instance of NtpCustomizationConfigManager. */
    public static NtpCustomizationConfigManager getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return NtpCustomizationConfigManager.LazyHolder.sInstance;
    }

    private NtpCustomizationConfigManager() {
        mHomepageStateListeners = new ObserverList<>();
        // Don't use the application's context to initialize these colors since only the Activity's
        // context is themed. Otherwise a wrong color is provided.
        mBackgroundColor = COLOR_NOT_SET;

        mBackgroundImageType = NtpCustomizationUtils.getNtpBackgroundImageType();
        if (mBackgroundImageType == NtpBackgroundImageType.IMAGE_FROM_DISK) {
            mIsInitialized = true;
            BackgroundImageInfo imageInfo = NtpCustomizationUtils.readNtpBackgroundImageMatrices();
            NtpCustomizationUtils.readNtpBackgroundImage(
                    (bitmap) -> {
                        if (bitmap == null) {
                            // When failed to load image from the disk, resets to the default color.
                            NtpCustomizationUtils.resetCustomizedColors();
                            return;
                        }
                        notifyBackgroundImageChanged(
                                bitmap,
                                imageInfo,
                                /* fromInitialization= */ true,
                                NtpBackgroundImageType.DEFAULT);
                    },
                    EXECUTOR);
        }

        mIsMvtToggleOn =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.IS_MVT_VISIBLE, true);
    }

    @VisibleForTesting
    void maybeInitializeColorTheme(Context context) {
        if (mIsInitialized) return;

        mIsInitialized = true;
        mBackgroundColor = getDefaultBackgroundColor(context);
        if (mBackgroundImageType == NtpBackgroundImageType.CHROME_COLOR) {
            mBackgroundColor =
                    NtpCustomizationUtils.getBackgroundColorFromSharedPreference(mBackgroundColor);
            notifyBackgroundColorChanged(
                    mBackgroundColor,
                    /* fromInitialization= */ true,
                    /* oldType= */ NtpBackgroundImageType.DEFAULT);
        }
    }

    /**
     * Adds a {@link HomepageStateListener} to receive updates when the home modules state changes.
     */
    public void addListener(HomepageStateListener listener, Context context) {
        mHomepageStateListeners.addObserver(listener);

        if (!mIsInitialized) {
            maybeInitializeColorTheme(context);
            return;
        }

        switch (mBackgroundImageType) {
            case NtpBackgroundImageType.IMAGE_FROM_DISK -> {
                if (mOriginalBitmap != null && mBackgroundImageInfo != null) {
                    listener.onBackgroundChanged(
                            mOriginalBitmap,
                            mBackgroundImageInfo,
                            /* fromInitialization= */ true,
                            NtpBackgroundImageType.DEFAULT,
                            mBackgroundImageType);
                }
            }
            case NtpBackgroundImageType.CHROME_COLOR, NtpBackgroundImageType.DEFAULT ->
                    listener.onBackgroundColorChanged(
                            mBackgroundColor,
                            /* fromInitialization= */ true,
                            NtpBackgroundImageType.DEFAULT,
                            mBackgroundImageType);
        }
    }

    /**
     * Removes the given listener from the state listener list.
     *
     * @param listener The listener to remove.
     */
    public void removeListener(HomepageStateListener listener) {
        mHomepageStateListeners.removeObserver(listener);
    }

    /**
     * Notifies listeners about the NTP's background change, and persistent the selected background
     * image to disk.
     *
     * @param bitmap The new background image bitmap before transformations.
     * @param backgroundImageInfo The {@link BackgroundImageInfo} object containing the portrait and
     *     landscape matrices.
     */
    public void onBackgroundChanged(Bitmap bitmap, BackgroundImageInfo backgroundImageInfo) {
        @NtpBackgroundImageType int oldType = mBackgroundImageType;

        mBackgroundImageType = NtpBackgroundImageType.IMAGE_FROM_DISK;
        NtpCustomizationUtils.setNtpBackgroundImageType(mBackgroundImageType);

        notifyBackgroundImageChanged(
                bitmap, backgroundImageInfo, /* fromInitialization= */ false, oldType);

        NtpCustomizationUtils.updateBackgroundImageMatrices(backgroundImageInfo);
        NtpCustomizationUtils.updateBackgroundImageFile(bitmap);

        // Picks the primary color for the bitmap and saves it to the SharedPreference.
        @ColorInt Integer primaryColor = NtpCustomizationUtils.getContentBasedSeedColor(bitmap);
        if (primaryColor != null) {
            NtpCustomizationUtils.setCustomizedPrimaryColor(primaryColor.intValue());
        }
    }

    /**
     * Notifies listeners about the NTP's background color change: 1) If a new customized color is
     * chosen: save the selected background color to the SharedPreference. 2) If resting to Chrome's
     * default color: delete the color key from the SharedPreference.
     *
     * @param context : The current Activity context.
     * @param colorInfo : The new NTP's background color.
     * @param backgroundImageType : The new background image type.
     */
    public void onBackgroundColorChanged(
            Context context,
            @Nullable NtpThemeColorInfo colorInfo,
            @NtpBackgroundImageType int backgroundImageType) {
        @NtpBackgroundImageType int oldType = mBackgroundImageType;

        mBackgroundImageType = backgroundImageType;
        NtpCustomizationUtils.setNtpBackgroundImageType(mBackgroundImageType);

        if (mBackgroundImageType == NtpBackgroundImageType.CHROME_COLOR) {
            notifyBackgroundColorChanged(
                    assumeNonNull(colorInfo).backgroundColor,
                    /* fromInitialization= */ false,
                    oldType);
            NtpCustomizationUtils.setBackgroundColor(colorInfo.backgroundColor);
            NtpCustomizationUtils.setCustomizedPrimaryColor(colorInfo.primaryColor);
        } else if (mBackgroundImageType == NtpBackgroundImageType.DEFAULT) {
            notifyBackgroundColorChanged(
                    getDefaultBackgroundColor(context), /* fromInitialization= */ false, oldType);
            NtpCustomizationUtils.resetCustomizedColors();
        }
    }

    /**
     * Notifies the NTP's background image is changed.
     *
     * @param originalBitmap The new background image bitmap before transformations.
     * @param backgroundImageInfo The {@link BackgroundImageInfo} object containing the portrait and
     *     landscape matrices.
     * @param fromInitialization Whether the update of the background comes from the initialization
     *     of the {@link NtpCustomizationConfigManager}, i.e, loading the image from the device.
     */
    @VisibleForTesting
    public void notifyBackgroundImageChanged(
            Bitmap originalBitmap,
            @Nullable BackgroundImageInfo backgroundImageInfo,
            boolean fromInitialization,
            @NtpBackgroundImageType int oldType) {
        mOriginalBitmap = originalBitmap;
        mBackgroundImageInfo = backgroundImageInfo;

        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onBackgroundChanged(
                    originalBitmap,
                    backgroundImageInfo,
                    fromInitialization,
                    oldType,
                    NtpBackgroundImageType.IMAGE_FROM_DISK);
        }
    }

    /**
     * Notifies the NTP's background color is changed.
     *
     * @param color The new background color.
     * @param fromInitialization Whether the update of the background comes from the initialization
     *     of the {@link NtpCustomizationConfigManager}, i.e,loading the image from the device.
     * @param oldType The previously set background type for NTP.
     */
    @VisibleForTesting
    public void notifyBackgroundColorChanged(
            @ColorInt int color, boolean fromInitialization, @NtpBackgroundImageType int oldType) {
        mBackgroundColor = color;

        // Clear out image state when switching to a color background.
        mOriginalBitmap = null;
        mBackgroundImageInfo = null;

        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onBackgroundColorChanged(
                    mBackgroundColor, fromInitialization, oldType, mBackgroundImageType);
        }
    }

    /** Notifies observers to refresh the system's WindowInsets. */
    public void notifyRefreshWindowInsets(boolean consumeTopInset) {
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.refreshWindowInsets(consumeTopInset);
        }
    }

    /** Returns the user's preference for whether the Most Visited Tiles section is visible. */
    public boolean getPrefIsMvtToggleOn() {
        return mIsMvtToggleOn;
    }

    /**
     * Sets the user preference for whether the Most Visited Tiles section is visible.
     *
     * @param isMvtToggleOn True to show the section, false to hide it.
     */
    public void setPrefIsMvtToggleOn(boolean isMvtToggleOn) {
        mIsMvtToggleOn = isMvtToggleOn;
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.IS_MVT_VISIBLE, isMvtToggleOn);

        // Notifies all the listeners.
        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onMvtToggleChanged();
        }
    }

    /** Gets the NTP's background image type. */
    public @NtpBackgroundImageType int getBackgroundImageType() {
        return mBackgroundImageType;
    }

    /**
     * Returns the current background color for NTP. Needs to use the Activity's context rather than
     * the application's context, which isn't themed and will provide a wrong color.
     *
     * @param context The current Activity context. It is themed and can provide the correct color.
     */
    public @ColorInt int getBackgroundColor(Context context) {
        if (!mIsInitialized) return getDefaultBackgroundColor(context);

        return mBackgroundColor;
    }

    /**
     * Returns the default background color for NTP. Needs to use the Activity's context rather than
     * the application's context, which isn't themed and will provide a wrong color.
     *
     * @param context The current Activity context. It is themed and can provide the correct color.
     */
    @VisibleForTesting
    @ColorInt
    int getDefaultBackgroundColor(Context context) {
        return ContextCompat.getColor(context, R.color.home_surface_background_color);
    }

    /**
     * Sets a NtpCustomizationConfigManager instance for testing.
     *
     * @param instance The instance to set.
     */
    public static void setInstanceForTesting(@Nullable NtpCustomizationConfigManager instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    public void setBackgroundColorForTesting(@ColorInt int color) {
        mBackgroundColor = color;
    }

    public @ColorInt int getBackgroundColorForTesting() {
        return mBackgroundColor;
    }

    public int getListenersSizeForTesting() {
        return mHomepageStateListeners.size();
    }

    public void setBackgroundImageTypeForTesting(@NtpBackgroundImageType int backgroundImageType) {
        mBackgroundImageType = backgroundImageType;
    }

    void setIsInitializedForTesting(boolean isInitialized) {
        mIsInitialized = isInitialized;
    }

    public void resetForTesting() {
        mHomepageStateListeners.clear();
        mIsInitialized = false;
        mBackgroundImageType = NtpBackgroundImageType.DEFAULT;
        mOriginalBitmap = null;
        mBackgroundImageInfo = null;
        mIsMvtToggleOn = false;
    }
}
