// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Used for gathering feedback from the feed in Chrome and bundling it into a set of Key - Value
 * pairs used to submit feedback requests.
 */
public class FeedFeedbackCollector extends FeedbackCollector<FeedFeedbackCollector.InitParams>
        implements Runnable {
    /** Initialization parameters needed by the Feed overload of FeedbackCollector<T>. */
    public static class InitParams {
        public Profile profile;
        public String url;
        public Map<String, String> feedContext;

        public InitParams(Profile profile, String url, Map<String, String> feedContext) {
            this.profile = profile;
            this.url = url;
            this.feedContext = feedContext;
        }
    }

    public FeedFeedbackCollector(
            Activity activity,
            @Nullable String categoryTag,
            @Nullable String description,
            @Nullable ScreenshotSource screenshotSource,
            InitParams initParams,
            Callback<FeedbackCollector> callback,
            Profile profile) {
        super(categoryTag, description, callback);

        init(activity, screenshotSource, initParams, profile);
    }

    @VisibleForTesting
    @Override
    protected List<FeedbackSource> buildSynchronousFeedbackSources(
            Activity activity, InitParams initParams) {
        List<FeedbackSource> sources = new ArrayList<>();

        // Since Interest feed feedback goes to a different destiation, we don't include other PSD
        // for privacy reasons.
        sources.add(new UrlFeedbackSource(initParams.url));
        sources.add(new InterestFeedFeedbackSource(initParams.feedContext));

        return sources;
    }

    @VisibleForTesting
    @Override
    protected List<AsyncFeedbackSource> buildAsynchronousFeedbackSources(InitParams initParams) {
        List<AsyncFeedbackSource> sources = new ArrayList<>();

        // This is the list of all asynchronous sources of feedback.  Please add new asynchronous
        // entries here.
        sources.add(new ConnectivityFeedbackSource(initParams.profile));
        sources.add(new SystemInfoFeedbackSource());
        sources.add(new ProcessIdFeedbackSource());

        return sources;
    }
}
