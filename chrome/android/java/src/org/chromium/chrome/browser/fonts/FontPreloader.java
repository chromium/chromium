// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fonts;

import android.content.Context;
import android.graphics.Typeface;
import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.ThreadUtils.ThreadChecker;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;

/**
 * Class to load downloadable fonts async. It should be used by calling {@link FontPreloader#load)
 * early in the app start-up, e.g. {@link Application#onCreate}.
 */
public class FontPreloader {
    private static FontPreloader sInstance;

    private static final Integer[] FONTS = {R.font.chrome_google_sans,
            R.font.chrome_google_sans_medium, R.font.chrome_google_sans_bold};
    private static final String UMA_PREFIX = "Android.Fonts";
    private static final String UMA_FONTS_RETRIEVED_BEFORE_INFLATION =
            "TimeDownloadableFontsRetrievedBeforePostInflationStartup";
    private static final String UMA_FONTS_RETRIEVED_AFTER_ON_CREATE =
            "TimeToRetrieveDownloadableFontsAfterOnCreate";
    private static final String UMA_FONTS_RETRIEVED_AFTER_INFLATION =
            "TimeDownloadableFontsRetrievedAfterPostInflationStartup";
    private static final String UMA_FRE = "FirstRunActivity";
    private static final String UMA_TABBED_ACTIVITY = "ChromeTabbedActivity";
    private static final String UMA_CUSTOM_TAB_ACTIVITY = "CustomTabActivity";

    private final ThreadChecker mThreadChecker = new ThreadChecker();

    private Integer[] mFonts = FONTS;
    private boolean mInitialized;
    private Long mTimeOfFirstEvent;
    private long mTimeOfLoadCall;
    private String mActivityName;

    @VisibleForTesting
    FontPreloader(Integer[] fonts) {
        mFonts = fonts;
    }

    private FontPreloader() {}

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
            OnFontCallback callback = new OnFontCallback();
            for (int font : mFonts) {
                ResourcesCompat.getFont(context, font, callback, null);
            }
            mTimeOfLoadCall = SystemClock.elapsedRealtime();
        }
    }

    /**
     * Should be called from FirstRunActivity to notify this class of post-inflation startup.
     */
    public void onPostInflationStartupFre() {
        mThreadChecker.assertOnValidThread();
        onPostInflationStartup(UMA_FRE);
    }

    /**
     * Should be called from ChromeTabbedActivity to notify this class of post-inflation startup.
     */
    public void onPostInflationStartupTabbedActivity() {
        mThreadChecker.assertOnValidThread();
        onPostInflationStartup(UMA_TABBED_ACTIVITY);
    }

    /**
     * Should be called from CustomTabActivity to notify this class of post-inflation startup.
     */
    public void onPostInflationStartupCustomTabActivity() {
        mThreadChecker.assertOnValidThread();
        onPostInflationStartup(UMA_CUSTOM_TAB_ACTIVITY);
    }

    private void onPostInflationStartup(String activityName) {
        // Multiple activities will notify us when they are post inflation, but only the first one
        // matters. It is the one we're racing against to load fonts before they're needed.
        if (mActivityName != null) return;
        mActivityName = activityName;

        final long time = SystemClock.elapsedRealtime();
        if (mTimeOfFirstEvent == null) {
            mTimeOfFirstEvent = time;
        } else {
            RecordHistogram.recordTimesHistogram(
                    String.format("%s.%s.%s", UMA_PREFIX, UMA_FONTS_RETRIEVED_BEFORE_INFLATION,
                            activityName),
                    time - mTimeOfFirstEvent);
        }
    }

    private void onAllFontsRetrieved() {
        final long time = SystemClock.elapsedRealtime();
        RecordHistogram.recordTimesHistogram(
                String.format("%s.%s", UMA_PREFIX, UMA_FONTS_RETRIEVED_AFTER_ON_CREATE),
                time - mTimeOfLoadCall);
        if (mTimeOfFirstEvent == null) {
            mTimeOfFirstEvent = time;
        } else {
            RecordHistogram.recordTimesHistogram(
                    String.format("%s.%s.%s", UMA_PREFIX, UMA_FONTS_RETRIEVED_AFTER_INFLATION,
                            mActivityName),
                    time - mTimeOfFirstEvent);
        }
    }

    private class OnFontCallback extends ResourcesCompat.FontCallback {
        private int mNumberOfFontsRetrieved;

        @Override
        public void onFontRetrieved(@NonNull Typeface typeface) {
            mThreadChecker.assertOnValidThread();
            if (++mNumberOfFontsRetrieved == mFonts.length) {
                onAllFontsRetrieved();
            }
        }

        @Override
        public void onFontRetrievalFailed(int reason) {}
    }
}
