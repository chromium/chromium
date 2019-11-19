// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.graphics.Bitmap;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Provides access to the search provider's logo via the C++ LogoService.
 */
public class LogoBridge {

    /**
     * A logo for a search provider (e.g. the Yahoo! logo or Google doodle).
     */
    public static class Logo {
        /**
         * The logo image. Non-null.
         */
        public final Bitmap image;

        /**
         * The URL to navigate to when the user clicks on the logo. May be null.
         */
        public final String onClickUrl;

        /**
         * The accessibility text describing the logo. May be null.
         */
        public final String altText;

        /**
         * The URL to download animated GIF logo. If null, there is no animated logo to download.
         */
        public final String animatedLogoUrl;

        Logo(Bitmap image, String onClickUrl, String altText, String animatedLogoUrl) {
            this.image = image;
            this.onClickUrl = onClickUrl;
            this.altText = altText;
            this.animatedLogoUrl = animatedLogoUrl;
        }
    }

    /**
     * Observer for receiving the logo when it's available.
     */
    public interface LogoObserver {
        /**
         * Called when the cached or fresh logo is available. This may be called up to two times,
         * once with the cached logo and once with a freshly downloaded logo.
         *
         * @param logo The search provider's logo.
         * @param fromCache Whether the logo was loaded from the cache.
         */
        @CalledByNative("LogoObserver")
        void onLogoAvailable(Logo logo, boolean fromCache);
    }

    private long mNativeLogoBridge;

    /**
     * Creates a LogoBridge for getting the logo of the default search provider.
     *
     * @param profile Profile of the tab that will show the logo.
     */
    public LogoBridge(Profile profile) {
        mNativeLogoBridge = LogoBridgeJni.get().init(LogoBridge.this, profile);
    }

    /**
     * Cleans up the C++ side of this class. After calling this, LogoObservers passed to
     * getCurrentLogo() will no longer receive updates.
     */
    void destroy() {
        assert mNativeLogoBridge != 0;
        LogoBridgeJni.get().destroy(mNativeLogoBridge, LogoBridge.this);
        mNativeLogoBridge = 0;
    }

    /**
     * Gets the current logo for the default search provider.
     *
     * @param logoObserver The observer to receive the cached and/or fresh logos when they're
     *                     available. logoObserver.onLogoAvailable() may be called synchronously if
     *                     the cached logo is already available.
     */
    void getCurrentLogo(LogoObserver logoObserver) {
        LogoBridgeJni.get().getCurrentLogo(mNativeLogoBridge, LogoBridge.this, logoObserver);
    }

    @CalledByNative
    private static Logo createLogo(Bitmap image, String onClickUrl, String altText, String gifUrl) {
        return new Logo(image, onClickUrl, altText, gifUrl);
    }

    @NativeMethods
    interface Natives {
        long init(LogoBridge caller, Profile profile);
        void getCurrentLogo(long nativeLogoBridge, LogoBridge caller, LogoObserver logoObserver);
        void destroy(long nativeLogoBridge, LogoBridge caller);
    }
}
