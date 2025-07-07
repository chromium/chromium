// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import org.chromium.base.ObserverList;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Manages the NTP's background configuration and notifies listeners of changes. */
@NullMarked
public class NtpCustomizationConfigManager {
    private @Nullable BitmapDrawable mBackgroundImageDrawable;

    /** An interface to get NewTabPage's configuration updates. */
    public interface HomepageStateListener {
        /** Called when the homepage background is changed. */
        void onBackgroundChanged(@Nullable Drawable backgroundDrawable);
    }

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static final NtpCustomizationConfigManager sInstance = new NtpCustomizationConfigManager();
    }

    private final ObserverList<HomepageStateListener> mHomepageStateListeners;

    /** Returns the singleton instance of NtpCustomizationConfigManager. */
    public static NtpCustomizationConfigManager getInstance() {
        return NtpCustomizationConfigManager.LazyHolder.sInstance;
    }

    private NtpCustomizationConfigManager() {
        mHomepageStateListeners = new ObserverList<>();
    }

    /**
     * Adds a {@link HomepageStateListener} to receive updates when the home modules state changes.
     */
    public void addListener(HomepageStateListener listener) {
        mHomepageStateListeners.addObserver(listener);
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
     * Notifies any listeners about a chrome home page background change.
     *
     * @param context The application context.
     * @param imageBitmap: The NTP's background image.
     */
    public void onBackgroundChanged(Context context, @Nullable Bitmap imageBitmap) {
        if (imageBitmap != null) {
            mBackgroundImageDrawable = new BitmapDrawable(context.getResources(), imageBitmap);
        } else {
            mBackgroundImageDrawable = null;
        }

        for (HomepageStateListener listener : mHomepageStateListeners) {
            listener.onBackgroundChanged(mBackgroundImageDrawable);
        }

        NtpCustomizationUtils.updateBackgroundImageFile(context, imageBitmap);
    }

    /** Gets the cached background image. */
    public @Nullable BitmapDrawable getBackgroundImageDrawable() {
        return mBackgroundImageDrawable;
    }
}
