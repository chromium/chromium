// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationUtils.NtpBackgroundImageType;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.concurrent.Executor;

/** Manages the NTP's background configuration and notifies listeners of changes. */
@NullMarked
public class NtpCustomizationConfigManager {
    private static final Executor EXECUTOR =
            (Runnable r) -> PostTask.postTask(TaskTraits.USER_BLOCKING_MAY_BLOCK, r);
    private boolean mIsInitialized;
    private @NtpBackgroundImageType int mBackgroundImageType;
    private @Nullable BitmapDrawable mBackgroundImageDrawable;
    private boolean mIsMvtToggleOn;

    /** An interface to get NewTabPage's configuration updates. */
    public interface HomepageStateListener {
        /** Called when the state of the toggle for the Most Visited Tiles section changes. */
        default void onMvtToggleChanged() {}

        /** Called when the homepage background is changed. */
        default void onBackgroundChanged(@Nullable Drawable backgroundDrawable) {}
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
        maybeInitialize();
    }

    @VisibleForTesting
    void maybeInitialize() {
        if (mIsInitialized) return;

        mIsInitialized = true;
        mBackgroundImageType = NtpCustomizationUtils.getNtpBackgroundImageType();
        if (mBackgroundImageType == NtpBackgroundImageType.IMAGE_FROM_DISK) {
            NtpCustomizationUtils.readNtpBackgroundImage(
                    (bitmap) -> notifyBackgroundImageChanged(bitmap), EXECUTOR);
        }
        mIsMvtToggleOn =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.IS_MVT_VISIBLE, true);
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

    /**
     * Adds a {@link HomepageStateListener} to receive updates when the home modules state changes.
     */
    public void addListener(HomepageStateListener listener) {
        mHomepageStateListeners.addObserver(listener);

        if (mIsInitialized) {
            listener.onBackgroundChanged(mBackgroundImageDrawable);
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
    public void onBackgroundChanged(@Nullable Bitmap bitmap) {
        mBackgroundImageType =
                bitmap == null
                        ? NtpBackgroundImageType.DEFAULT
                        : NtpBackgroundImageType.IMAGE_FROM_DISK;
        NtpCustomizationUtils.setNtpBackgroundImageType(mBackgroundImageType);

        notifyBackgroundImageChanged(bitmap);

        NtpCustomizationUtils.updateBackgroundImageFile(bitmap);
    }

    /**
     * Notifies the NTP's background image is changed.
     *
     * @param imageBitmap The new background image.
     */
    private void notifyBackgroundImageChanged(@Nullable Bitmap imageBitmap) {
        if (imageBitmap != null) {
            mBackgroundImageDrawable =
                    new BitmapDrawable(
                            ContextUtils.getApplicationContext().getResources(), imageBitmap);
        } else {
            mBackgroundImageDrawable = null;
        }

        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onBackgroundChanged(mBackgroundImageDrawable);
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

    public void setBackgroundImageDrawableForTesting(@Nullable BitmapDrawable bitmapDrawable) {
        mBackgroundImageDrawable = bitmapDrawable;
    }
}
