// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.app.Activity;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.night_mode.AutoDarkFeedbackSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Used for gathering a variety of feedback from various components in Chrome and bundling it into
 * a set of Key - Value pairs used to submit feedback requests.
 */
public class ChromeFeedbackCollector extends FeedbackCollector<ChromeFeedbackCollector.InitParams>
        implements Runnable {
    /** Initialization Parameters of the Chrome overload of FeedbackCollector<T>. */
    public static class InitParams {
        public Profile profile;
        public String url;
        public String feedbackContext;

        public InitParams(Profile profile, String url, String feedbackContext) {
            this.profile = profile;
            this.url = url;
            this.feedbackContext = feedbackContext;
        }
    }

    public ChromeFeedbackCollector(
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

        // This is the list of all synchronous sources of feedback.  Please add new synchronous
        // entries here.
        // ATTENTION: Before making any changes or adding new FeedbackSources to feedback
        // collection, please ensure the teams that operationalize feedback are aware and
        // supportive. Contact: chrome-gtech@.
        sources.add(new DeviceInfoFeedbackSource());
        sources.add(new UrlFeedbackSource(initParams.url));
        sources.add(new VariationsFeedbackSource(initParams.profile));
        sources.add(new HistogramFeedbackSource(initParams.profile));
        sources.add(new LowEndDeviceFeedbackSource());
        sources.add(new IMEFeedbackSource());
        sources.add(new PermissionFeedbackSource());
        sources.add(new FeedbackContextFeedbackSource(initParams.feedbackContext));
        sources.add(
                new AutoDarkFeedbackSource(initParams.profile, activity, new GURL(initParams.url)));

        return sources;
    }

    @VisibleForTesting
    @Override
    protected List<AsyncFeedbackSource> buildAsynchronousFeedbackSources(InitParams initParams) {
        List<AsyncFeedbackSource> sources = new ArrayList<>();

        // This is the list of all asynchronous sources of feedback.  Please add new asynchronous
        // entries here.
        // ATTENTION: Before making any changes or adding new FeedbackSources to feedback
        // collection, please ensure the teams that operationalize feedback are aware and
        // supportive. Contact: chrome-gtech@.
        sources.add(new ConnectivityFeedbackSource(initParams.profile));
        sources.add(new SystemInfoFeedbackSource());
        sources.add(new ProcessIdFeedbackSource());

        // FamilyInfoFeedbackSource relies on IdentityManager which is not available for the
        // incognito profile.
        if (!initParams.profile.isOffTheRecord()) {
            sources.add(new FamilyInfoFeedbackSource(initParams.profile));
        }

        return sources;
    }

    List<AsyncFeedbackSource> getAsyncFeedbackSourcesForTesting() {
        return mAsynchronousSources;
    }
}
