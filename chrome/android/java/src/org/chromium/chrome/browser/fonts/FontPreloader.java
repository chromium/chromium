// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fonts;

import android.content.Context;
import android.graphics.Typeface;

import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.ThreadUtils.ThreadChecker;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;

/**
 * Class to load downloadable fonts async. It should be used by calling {@link FontPreloader#load)
 * early in the app start-up, e.g. {@link Application#onCreate}.
 */
@NullMarked
public class FontPreloader {
    private static @Nullable FontPreloader sInstance;

    private static final Integer[] FONTS = {
        R.font.chrome_google_sans,
        R.font.chrome_google_sans_medium,
        R.font.chrome_google_sans_bold,
        R.font.chrome_google_sans_text,
        R.font.chrome_google_sans_text_medium
    };

    private final ThreadChecker mThreadChecker = new ThreadChecker();

    private final Integer[] mFonts;
    private boolean mInitialized;

    @VisibleForTesting
    FontPreloader(Integer[] fonts) {
        mFonts = fonts;
    }

    private FontPreloader() {
        mFonts = FONTS;
    }

    /**
     * @return The {@link FontPreloader} singleton instance.
     */
    public static FontPreloader getInstance() {
        if (sInstance == null) {
            sInstance = new FontPreloader();
        }
        return sInstance;
    }

    /**
     * Should be called to preload the fonts onCreate. Any subsequent calls will be ignored.
     *
     * @param context A {@link Context} to retrieve the fonts from.
     */
    public void load(Context context) {
        mThreadChecker.assertOnValidThread();
        if (!mInitialized) {
            mInitialized = true;
            context = context.getApplicationContext();
            // Create an empty callback so we can call the async version of #getFont.
            var callback =
                    new ResourcesCompat.FontCallback() {
                        @Override
                        public void onFontRetrieved(Typeface typeface) {}

                        @Override
                        public void onFontRetrievalFailed(int i) {}
                    };
            for (int font : mFonts) {
                ResourcesCompat.getFont(context, font, callback, null);
            }
        }
    }
}
