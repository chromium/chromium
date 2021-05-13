// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.content.Context;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.feed.v2.FeedProcessScopeDependencyProvider;
import org.chromium.chrome.browser.feed.v2.FeedStream;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.xsurface.SurfaceScopeDependencyProvider;
import org.chromium.chrome.browser.xsurface.SurfaceScopeDependencyProvider.AutoplayEvent;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;

/**
 * Provides activity and darkmode context for a single surface.
 */
public class FeedSurfaceScopeDependencyProvider implements SurfaceScopeDependencyProvider {
    // This must match the FeedAutoplayEvent enum in enums.xml.
    private @interface FeedAutoplayEvent {
        int AUTOPLAY_REQUESTED = 0;
        int AUTOPLAY_STARTED = 1;
        int AUTOPLAY_STOPPED = 2;
        int AUTOPLAY_ENDED = 3;
        int AUTOPLAY_CLICKED = 4;
        int NUM_ENTRIES = 5;
    }

    private static final String TAG = "Feed";
    private final Activity mActivity;
    private final Context mActivityContext;
    private final boolean mDarkMode;
    private final FeedStream mFeedStream;

    public FeedSurfaceScopeDependencyProvider(
            Activity activity, Context activityContext, boolean darkMode, FeedStream feedStream) {
        mActivityContext = FeedProcessScopeDependencyProvider.createFeedContext(activityContext);
        mDarkMode = darkMode;
        mFeedStream = feedStream;
        mActivity = activity;
    }

    @Override
    public Activity getActivity() {
        return mActivity;
    }

    @Override
    public Context getActivityContext() {
        return mActivityContext;
    }

    @Override
    public boolean isDarkModeEnabled() {
        return mDarkMode;
    }

    @Override
    public AutoplayPreference getAutoplayPreference() {
        assert ThreadUtils.runningOnUiThread();
        @VideoPreviewsType
        int videoPreviewsType = FeedServiceBridge.getVideoPreviewsTypePreference();
        switch (videoPreviewsType) {
            case VideoPreviewsType.NEVER:
                return AutoplayPreference.AUTOPLAY_DISABLED;
            case VideoPreviewsType.WIFI_AND_MOBILE_DATA:
                return AutoplayPreference.AUTOPLAY_ON_WIFI_AND_MOBILE_DATA;
            case VideoPreviewsType.WIFI:
            default:
                return AutoplayPreference.AUTOPLAY_ON_WIFI_ONLY;
        }
    }

    @Override
    public void reportAutoplayEvent(AutoplayEvent event) {
        int feedAutoplayEvent;
        if (event == AutoplayEvent.AUTOPLAY_REQUESTED) {
            feedAutoplayEvent = FeedAutoplayEvent.AUTOPLAY_REQUESTED;
        } else if (event == AutoplayEvent.AUTOPLAY_STARTED) {
            feedAutoplayEvent = FeedAutoplayEvent.AUTOPLAY_STARTED;
        } else if (event == AutoplayEvent.AUTOPLAY_STOPPED) {
            feedAutoplayEvent = FeedAutoplayEvent.AUTOPLAY_STOPPED;
        } else if (event == AutoplayEvent.AUTOPLAY_ENDED) {
            feedAutoplayEvent = FeedAutoplayEvent.AUTOPLAY_ENDED;
        } else if (event == AutoplayEvent.AUTOPLAY_CLICKED) {
            feedAutoplayEvent = FeedAutoplayEvent.AUTOPLAY_CLICKED;
            NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_VIDEO);
        } else {
            Log.wtf(TAG, "Unable to map AutoplayEvent " + event.name());
            return;
        }
        RecordHistogram.recordEnumeratedHistogram("ContentSuggestions.Feed.AutoplayEvent",
                feedAutoplayEvent, FeedAutoplayEvent.NUM_ENTRIES);
    }

    /**
     * FeedLoggingDependencyProvider implementation.
     */

    @Override
    public String getAccountName() {
        // Don't return account name if there's a signed-out session ID.
        if (!getSignedOutSessionId().isEmpty()) {
            return "";
        }
        assert ThreadUtils.runningOnUiThread();
        CoreAccountInfo primaryAccount =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        return (primaryAccount == null) ? "" : primaryAccount.getEmail();
    }

    @Override
    public String getClientInstanceId() {
        // Don't return client instance id if there's a signed-out session ID.
        if (!getSignedOutSessionId().isEmpty()) {
            return "";
        }
        assert ThreadUtils.runningOnUiThread();
        return FeedServiceBridge.getClientInstanceId();
    }

    @Override
    public int[] getExperimentIds() {
        assert ThreadUtils.runningOnUiThread();
        return mFeedStream.getExperimentIds();
    }

    @Override
    public boolean isActivityLoggingEnabled() {
        return mFeedStream.isActivityLoggingEnabled();
    }

    @Override
    public String getSignedOutSessionId() {
        return mFeedStream.getSignedOutSessionId();
    }

    /**
     * Stores a view FeedAction for eventual upload. 'data' is a serialized FeedAction protobuf
     * message.
     */
    @Override
    public void processViewAction(byte[] data) {
        mFeedStream.processViewAction(data);
    }
}
