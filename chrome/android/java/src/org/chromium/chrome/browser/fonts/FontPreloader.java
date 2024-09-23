// Copyright 2021 The Chromium Authors
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
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Class to load downloadable fonts async and emit histograms related to the availability of these
 * fonts. It should be used by calling {@link FontPreloader#load) early in the app start-up, e.g.
 * {@link Application#onCreate}. The {@link Activity}s should also call the #on* methods to notify
 * this class of the events as they are used to record metrics.
 */
public class FontPreloader {
    private static FontPreloader sInstance;

    private static final Integer[] FONTS = {
        R.font.chrome_google_sans, R.font.chrome_google_sans_medium, R.font.chrome_google_sans_bold
    };

    private static final Integer[] FONTS_V2 = {
        R.font.chrome_google_sans,
        R.font.chrome_google_sans_medium,
        R.font.chrome_google_sans_bold,
        R.font.chrome_google_sans_text,
        R.font.chrome_google_sans_text_medium
    };

    private static final String UMA_PREFIX = "Android.Fonts";

    private static final String UMA_FONTS_RETRIEVED_BEFORE_INFLATION =
            "TimeDownloadableFontsRetrievedBeforePostInflationStartup";
    private static final String UMA_FONTS_RETRIEVED_AFTER_ON_CREATE =
            "TimeToRetrieveDownloadableFontsAfterOnCreate";
    private static final String UMA_FONTS_RETRIEVED_AFTER_INFLATION =
            "TimeDownloadableFontsRetrievedAfterPostInflationStartup";

    private static final String UMA_FONTS_RETRIEVED_BEFORE_FIRST_DRAW =
            "TimeDownloadableFontsRetrievedBeforeFirstDraw";
    private static final String UMA_FONTS_RETRIEVED_AFTER_FIRST_DRAW =
            "TimeDownloadableFontsRetrievedAfterFirstDraw";

    private static final String UMA_FRE = "FirstRunActivity";
    private static final String UMA_TABBED_ACTIVITY = "ChromeTabbedActivity";
    private static final String UMA_CUSTOM_TAB_ACTIVITY = "CustomTabActivity";

    private final ThreadChecker mThreadChecker = new ThreadChecker();

    private final Integer[] mFonts;
    private boolean mInitialized;
    // Time of first event between |#onAllFontsRetrieved()| and |#onPostInflationStartup*()|.
    private Long mTimeOfFirstEventForPostInflation;
    private long mTimeOfLoadCall;
    private String mActivityNameForPostInflation;
    // Time of first event between |#onAllFontsRetrieved()| and |#onFirstDraw*()|.
    private Long mTimeOfFirstEventForFirstDraw;
    private String mActivityNameForFirstDraw;

    @VisibleForTesting
    FontPreloader(Integer[] fonts) {
        mFonts = fonts;
    }

    private FontPreloader() {
        if (ChromeFeatureList.sAndroidGoogleSansText.isEnabled()) {
            mFonts = FONTS_V2;
        } else {
            mFonts = FONTS;
        }
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
            OnFontCallback callback = new OnFontCallback();
            for (int font : mFonts) {
                ResourcesCompat.getFont(context, font, callback, null);
            }
            mTimeOfLoadCall = SystemClock.elapsedRealtime();
        }
    }

    /** Should be called from FirstRunActivity to notify this class of post-inflation startup. */
    public void onPostInflationStartupFre() {
        mThreadChecker.assertOnValidThread();
        onPostInflationStartup(UMA_FRE);
    }

    /** Should be called from FirstRunActivity to notify this class of the first draw. */
    public void onFirstDrawFre() {
        mThreadChecker.assertOnValidThread();
        onFirstDraw(UMA_FRE);
    }

    /**
     * Should be called from ChromeTabbedActivity to notify this class of post-inflation startup.
     */
    public void onPostInflationStartupTabbedActivity() {
        mThreadChecker.assertOnValidThread();
        onPostInflationStartup(UMA_TABBED_ACTIVITY);
    }

    /**
     * Should be called from ChromeTabbedActivity to notify this class of the first draw. The first
     * draw of ChromeTabbedActivity may be blocked by AppLaunchDrawBlocker, so the caller should
     * account for that.
     */
    public void onFirstDrawTabbedActivity() {
        mThreadChecker.assertOnValidThread();
        onFirstDraw(UMA_TABBED_ACTIVITY);
    }

    /** Should be called from CustomTabActivity to notify this class of post-inflation startup. */
    public void onPostInflationStartupCustomTabActivity() {
        mThreadChecker.assertOnValidThread();
        onPostInflationStartup(UMA_CUSTOM_TAB_ACTIVITY);
    }

    /** Should be called from CustomTabActivity to notify this class of the first draw. */
    public void onFirstDrawCustomTabActivity() {
        mThreadChecker.assertOnValidThread();
        onFirstDraw(UMA_CUSTOM_TAB_ACTIVITY);
    }

    private void onPostInflationStartup(String activityName) {
        // Multiple activities will notify us when they are post inflation, but only the first one
        // matters. It is the one we're racing against to load fonts before they're needed.
        if (mActivityNameForPostInflation != null) return;
        mActivityNameForPostInflation = activityName;

        final long time = SystemClock.elapsedRealtime();
        if (mTimeOfFirstEventForPostInflation == null) {
            mTimeOfFirstEventForPostInflation = time;
        } else {
            RecordHistogram.recordTimesHistogram(
                    String.format(
                            "%s.%s.%s",
                            UMA_PREFIX, UMA_FONTS_RETRIEVED_BEFORE_INFLATION, activityName),
                    time - mTimeOfFirstEventForPostInflation);
        }
    }

    private void onFirstDraw(String activityName) {
        // Multiple activities will notify us when they do their first draw, but only the first one
        // matters.
        if (mActivityNameForFirstDraw != null) return;
        mActivityNameForFirstDraw = activityName;

        long time = SystemClock.elapsedRealtime();
        if (mTimeOfFirstEventForFirstDraw == null) {
            mTimeOfFirstEventForFirstDraw = time;
        } else {
            RecordHistogram.recordTimesHistogram(
                    String.format(
                            "%s.%s.%s",
                            UMA_PREFIX,
                            UMA_FONTS_RETRIEVED_BEFORE_FIRST_DRAW,
                            mActivityNameForFirstDraw),
                    time - mTimeOfFirstEventForFirstDraw);
            // Also record one without the activity name for aggregation across all activities.
            RecordHistogram.recordTimesHistogram(
                    String.format("%s.%s", UMA_PREFIX, UMA_FONTS_RETRIEVED_BEFORE_FIRST_DRAW),
                    time - mTimeOfFirstEventForFirstDraw);
        }
    }

    private void onAllFontsRetrieved() {
        final long time = SystemClock.elapsedRealtime();
        RecordHistogram.recordTimesHistogram(
                String.format("%s.%s", UMA_PREFIX, UMA_FONTS_RETRIEVED_AFTER_ON_CREATE),
                time - mTimeOfLoadCall);

        if (mTimeOfFirstEventForPostInflation == null) {
            mTimeOfFirstEventForPostInflation = time;
        } else {
            RecordHistogram.recordTimesHistogram(
                    String.format(
                            "%s.%s.%s",
                            UMA_PREFIX,
                            UMA_FONTS_RETRIEVED_AFTER_INFLATION,
                            mActivityNameForPostInflation),
                    time - mTimeOfFirstEventForPostInflation);
        }

        if (mTimeOfFirstEventForFirstDraw == null) {
            mTimeOfFirstEventForFirstDraw = time;
        } else {
            RecordHistogram.recordTimesHistogram(
                    String.format(
                            "%s.%s.%s",
                            UMA_PREFIX,
                            UMA_FONTS_RETRIEVED_AFTER_FIRST_DRAW,
                            mActivityNameForFirstDraw),
                    time - mTimeOfFirstEventForFirstDraw);
            // Also record one without the activity name for aggregation across all activities.
            RecordHistogram.recordTimesHistogram(
                    String.format("%s.%s", UMA_PREFIX, UMA_FONTS_RETRIEVED_AFTER_FIRST_DRAW),
                    time - mTimeOfFirstEventForFirstDraw);
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
