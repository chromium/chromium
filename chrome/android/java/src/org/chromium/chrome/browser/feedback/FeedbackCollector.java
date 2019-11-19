// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Used for gathering a variety of feedback from various components in Chrome and bundling it into
 * a set of Key - Value pairs used to submit feedback requests.
 */
public class FeedbackCollector implements Runnable {
    /** The timeout for gathering data asynchronously. This timeout is ignored for screenshots. */
    private static final int TIMEOUT_MS = 500;

    private final List<FeedbackSource> mSynchronousSources;
    private final List<AsyncFeedbackSource> mAsynchronousSources;
    private final long mStartTime = SystemClock.elapsedRealtime();

    private final String mCategoryTag;
    private final String mDescription;

    private ScreenshotSource mScreenshotTask;

    /** The callback is cleared once notified so we will never notify the caller twice. */
    private Callback<FeedbackCollector> mCallback;

    public FeedbackCollector(Activity activity, Profile profile, @Nullable String url,
            @Nullable String categoryTag, @Nullable String description,
            @Nullable String feedbackContext, boolean takeScreenshot,
            Callback<FeedbackCollector> callback) {
        mCategoryTag = categoryTag;
        mDescription = description;
        mCallback = callback;

        // 1. Build all synchronous and asynchronous sources.
        mSynchronousSources = buildSynchronousFeedbackSources(profile, url, feedbackContext);
        mAsynchronousSources = buildAsynchronousFeedbackSources(profile);

        // 2. Build the screenshot task if necessary.
        if (takeScreenshot) mScreenshotTask = buildScreenshotSource(activity);

        // 3. Start all asynchronous sources and the screenshot task.
        CollectionUtil.forEach(mAsynchronousSources, source -> source.start(this));
        if (mScreenshotTask != null) mScreenshotTask.capture(this);

        // 4. Kick off a task to timeout the async sources.
        ThreadUtils.postOnUiThreadDelayed(this, TIMEOUT_MS);

        // 5. Sanity check in case everything finished or we have no sources.
        checkIfReady();
    }

    @VisibleForTesting
    protected List<FeedbackSource> buildSynchronousFeedbackSources(
            Profile profile, @Nullable String url, @Nullable String feedbackContext) {
        List<FeedbackSource> sources = new ArrayList<>();

        // This is the list of all synchronous sources of feedback.  Please add new synchronous
        // entries here.
        sources.addAll(AppHooks.get().getAdditionalFeedbackSources().getSynchronousSources());
        sources.add(new UrlFeedbackSource(url));
        sources.add(new VariationsFeedbackSource(profile));
        sources.add(new DataReductionProxyFeedbackSource(profile));
        sources.add(new HistogramFeedbackSource(profile));
        sources.add(new LowEndDeviceFeedbackSource());
        sources.add(new IMEFeedbackSource());
        sources.add(new PermissionFeedbackSource());
        sources.add(new FeedbackContextFeedbackSource(feedbackContext));
        sources.add(new DuetFeedbackSource());
        sources.add(new InterestFeedFeedbackSource());

        // Sanity check in case a source is added to the wrong list.
        for (FeedbackSource source : sources) {
            assert !(source instanceof AsyncFeedbackSource);
        }

        return sources;
    }

    @VisibleForTesting
    protected List<AsyncFeedbackSource> buildAsynchronousFeedbackSources(Profile profile) {
        List<AsyncFeedbackSource> sources = new ArrayList<>();

        // This is the list of all asynchronous sources of feedback.  Please add new asynchronous
        // entries here.
        sources.addAll(AppHooks.get().getAdditionalFeedbackSources().getAsynchronousSources());
        sources.add(new ConnectivityFeedbackSource(profile));
        sources.add(new SystemInfoFeedbackSource());
        sources.add(new ProcessIdFeedbackSource());

        return sources;
    }

    @VisibleForTesting
    protected ScreenshotSource buildScreenshotSource(Activity activity) {
        return new ScreenshotTask(activity);
    }

    /** @return The category tag for this feedback report. */
    public String getCategoryTag() {
        return mCategoryTag;
    }

    /** @return The description of this feedback report. */
    public String getDescription() {
        return mDescription;
    }

    /**
     * Deprecated.  Please use {@link #getLogs()} instead for all potentially large feedback data.
     * @return Returns the histogram data from {@link #getLogs()}.
     */
    @Deprecated
    public String getHistograms() {
        return getLogs().get(HistogramFeedbackSource.HISTOGRAMS_KEY);
    }

    /**
     * After calling this, this collector will not notify the {@link Callback} specified in the
     * constructor (if it hasn't already).
     *
     * @return A {@link Bundle} containing all of the feedback for this report.
     * @see #getLogs() to get larger feedback data (logs).
     */
    public Bundle getBundle() {
        ThreadUtils.assertOnUiThread();

        // At this point we will no longer update the caller if we get more info from sources.
        mCallback = null;

        Bundle bundle = new Bundle();
        doWorkOnAllFeedbackSources(source -> {
            Map<String, String> feedback = source.getFeedback();
            if (feedback == null) return;

            CollectionUtil.forEach(feedback, e -> { bundle.putString(e.getKey(), e.getValue()); });
        });
        return bundle;
    }

    /**
     * After calling this, this collector will not notify the {@link Callback} specified in the
     * constructor (if it hasn't already).
     *
     * @return A {@link Map} containing all of the logs for this report.
     * @see #getBundle() to get smaller feedback data (key -> value).
     */
    public Map<String, String> getLogs() {
        ThreadUtils.assertOnUiThread();

        // At this point we will no longer update the caller if we get more info from sources.
        mCallback = null;

        Map<String, String> logs = new HashMap<>();
        doWorkOnAllFeedbackSources(source -> {
            Pair<String, String> log = source.getLogs();
            if (log == null) return;

            logs.put(log.first, log.second);
        });
        return logs;
    }

    /** @return A screenshot for this report (if one was able to be taken). */
    public @Nullable Bitmap getScreenshot() {
        return mScreenshotTask == null ? null : mScreenshotTask.getScreenshot();
    }

    /**
     * Allows overriding the internal screenshot logic to always return {@code screenshot}.
     * @param screenshot The screenshot {@link Bitmap} to use.
     */
    public void setScreenshot(@Nullable Bitmap screenshot) {
        mScreenshotTask = new StaticScreenshotSource(screenshot);
        mScreenshotTask.capture(this);
    }

    /* Called whenever an AsyncFeedbackCollector is done querying data or we have timed out. */
    @Override
    public void run() {
        checkIfReady();
    }

    private void checkIfReady() {
        if (mCallback == null) return;

        // The screenshot capture overrides the timeout.
        if (mScreenshotTask != null && !mScreenshotTask.isReady()) return;

        if (mAsynchronousSources.size() > 0
                && SystemClock.elapsedRealtime() - mStartTime < TIMEOUT_MS) {
            for (AsyncFeedbackSource source : mAsynchronousSources) {
                if (!source.isReady()) return;
            }
        }

        final Callback<FeedbackCollector> callback = mCallback;
        mCallback = null;

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                callback.onResult(FeedbackCollector.this);
            }
        });
    }

    private void doWorkOnAllFeedbackSources(Callback<FeedbackSource> worker) {
        CollectionUtil.forEach(mSynchronousSources, worker);
        CollectionUtil.forEach(mAsynchronousSources, worker);
    }
}
