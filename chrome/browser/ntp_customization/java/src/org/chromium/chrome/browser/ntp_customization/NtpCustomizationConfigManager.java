// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
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
    private @Nullable BitmapDrawable mBackgroundImageDrawable;
    private @ColorInt int mBackgroundColor;
    private boolean mIsMvtToggleOn;

    /** An interface to get NewTabPage's configuration updates. */
    public interface HomepageStateListener {
        /** Called when the state of the toggle for the Most Visited Tiles section changes. */
        default void onMvtToggleChanged() {}

        /**
         * Called when a customized homepage background image is chosen.
         *
         * @param backgroundDrawable The new background image drawable.
         * @param fromInitialization Whether the update of the background comes from the
         *     initialization of the {@link NtpCustomizationConfigManager}, i.e, loading the image
         *     from the device.
         * @param oldType The previously set background type for NTPs.
         * @param newType The new background type of NTPs.
         */
        default void onBackgroundChanged(
                Drawable backgroundDrawable,
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
        maybeInitialize();
    }

    @VisibleForTesting
    void maybeInitialize() {
        if (mIsInitialized) return;

        mIsInitialized = true;
        mBackgroundImageType = NtpCustomizationUtils.getNtpBackgroundImageType();
        if (mBackgroundImageType == NtpBackgroundImageType.IMAGE_FROM_DISK) {
            NtpCustomizationUtils.readNtpBackgroundImage(
                    (bitmap) -> {
                        if (bitmap == null) {
                            // When failed to load image from the disk, resets to the default color.
                            NtpCustomizationUtils.resetBackgroundColor();
                            return;
                        }

                        notifyBackgroundImageChanged(
                                bitmap,
                                /* fromInitialization= */ true,
                                NtpBackgroundImageType.DEFAULT);
                    },
                    EXECUTOR);
        } else if (mBackgroundImageType == NtpBackgroundImageType.CHROME_COLOR) {
            mBackgroundColor =
                    NtpCustomizationUtils.getBackgroundColorFromSharedPreference(mBackgroundColor);
            // Skips notifying the observers if the default background color isn't initialized. Any
            // observer can initialize the default color when calling #getBackgroundColor(Context).
            if (mBackgroundColor != COLOR_NOT_SET) {
                notifyBackgroundColorChanged(
                        mBackgroundColor,
                        /* fromInitialization= */ true,
                        NtpBackgroundImageType.DEFAULT);
            }
        }

        mIsMvtToggleOn =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.IS_MVT_VISIBLE, true);
    }

    /**
     * Adds a {@link HomepageStateListener} to receive updates when the home modules state changes.
     */
    public void addListener(HomepageStateListener listener) {
        mHomepageStateListeners.addObserver(listener);

        if (!mIsInitialized) return;

        switch (mBackgroundImageType) {
            case NtpBackgroundImageType.IMAGE_FROM_DISK -> listener.onBackgroundChanged(
                    assumeNonNull(mBackgroundImageDrawable),
                    /* fromInitialization= */ true,
                    NtpBackgroundImageType.DEFAULT,
                    mBackgroundImageType);
            case NtpBackgroundImageType.CHROME_COLOR, NtpBackgroundImageType.DEFAULT -> listener
                    .onBackgroundColorChanged(
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
     * @param bitmap : The NTP's background image.
     */
    public void onBackgroundChanged(Bitmap bitmap) {
        @NtpBackgroundImageType int oldType = mBackgroundImageType;

        mBackgroundImageType = NtpBackgroundImageType.IMAGE_FROM_DISK;
        NtpCustomizationUtils.setNtpBackgroundImageType(mBackgroundImageType);

        notifyBackgroundImageChanged(bitmap, /* fromInitialization= */ false, oldType);

        NtpCustomizationUtils.updateBackgroundImageFile(bitmap);
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
        } else if (mBackgroundImageType == NtpBackgroundImageType.DEFAULT) {
            notifyBackgroundColorChanged(
                    getDefaultBackgroundColor(context), /* fromInitialization= */ false, oldType);
            NtpCustomizationUtils.resetBackgroundColor();
        }
    }

    /**
     * Notifies the NTP's background image is changed.
     *
     * @param imageBitmap The new background image.
     * @param fromInitialization Whether the update of the background comes from the initialization
     *     of the {@link NtpCustomizationConfigManager}, i.e, loading the image from the device.
     * @param oldType The previously set background type for NTP.
     */
    @VisibleForTesting
    public void notifyBackgroundImageChanged(
            Bitmap imageBitmap, boolean fromInitialization, @NtpBackgroundImageType int oldType) {
        mBackgroundImageDrawable =
                new BitmapDrawable(
                        ContextUtils.getApplicationContext().getResources(), imageBitmap);

        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onBackgroundChanged(
                    assumeNonNull(mBackgroundImageDrawable),
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

    /** Gets the cached background image. */
    public @Nullable BitmapDrawable getBackgroundImageDrawable() {
        return mBackgroundImageDrawable;
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
        if (mBackgroundImageType == NtpBackgroundImageType.DEFAULT) {
            mBackgroundColor = getDefaultBackgroundColor(context);
        }
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

    public void setBackgroundImageDrawableForTesting(@Nullable BitmapDrawable bitmapDrawable) {
        mBackgroundImageDrawable = bitmapDrawable;
    }

    public @ColorInt int getBackgroundColorForTesting() {
        return mBackgroundColor;
    }

    public int getListenersSizeForTesting() {
        return mHomepageStateListeners.size();
    }

    public void setBackgroundImageTypeFroTesting(@NtpBackgroundImageType int backgroundImageType) {
        mBackgroundImageType = backgroundImageType;
    }

    public void resetForTesting() {
        mHomepageStateListeners.clear();
        mIsInitialized = false;
        mBackgroundImageType = NtpBackgroundImageType.DEFAULT;
        mBackgroundImageDrawable = null;
        mIsMvtToggleOn = false;
    }
}
