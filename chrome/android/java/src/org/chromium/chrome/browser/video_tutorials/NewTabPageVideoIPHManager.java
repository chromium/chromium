// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials;

import android.content.Context;
import android.view.ViewStub;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.video_tutorials.iph.VideoIPHCoordinator;
import org.chromium.chrome.browser.video_tutorials.iph.VideoTutorialIPHUtils;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.feature_engagement.Tracker;

import java.util.List;

/**
 * Handles all the logic required for showing the appropriate video tutorial IPH on new tab page.
 * Queries the backend for the sorted list of available video tutorials, and shows the one not seen
 * by user yet. Also responsible for showing the next tutorial IPH when one is consumed or
 * dismissed.
 */
public class NewTabPageVideoIPHManager {
    private Context mContext;
    private Tracker mTracker;
    private VideoIPHCoordinator mVideoIPHCoordinator;
    private VideoTutorialService mVideoTutorialService;

    /**
     * Constructor.
     * @param viewStub The {@link ViewStub} to be inflated to show the IPH.
     * @param profile The associated profile.
     */
    public NewTabPageVideoIPHManager(ViewStub viewStub, Profile profile) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.VIDEO_TUTORIALS)) return;

        mContext = viewStub.getContext();
        mTracker = TrackerFactory.getTrackerForProfile(profile);
        ImageFetcher imageFetcher =
                ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                        profile, GlobalDiscardableReferencePool.getReferencePool());
        Callback<Tutorial> clickListener = this::onClickIPH;
        Callback<Tutorial> dismissListener = this::onDismissIPH;
        mVideoIPHCoordinator = VideoTutorialServiceFactory.createVideoIPHCoordinator(
                viewStub, imageFetcher, clickListener, dismissListener);

        mVideoTutorialService = VideoTutorialServiceFactory.getForProfile(profile);
        mVideoTutorialService.getTutorials(this::onFetchTutorials);
    }

    private void onFetchTutorials(List<Tutorial> tutorials) {
        mTracker.addOnInitializedCallback(success -> {
            if (!success) return;

            showFirstEligibleIPH(tutorials);
        });
    }

    private void showFirstEligibleIPH(List<Tutorial> tutorials) {
        for (Tutorial tutorial : tutorials) {
            String featureName = VideoTutorialIPHUtils.getFeatureName(tutorial.featureType);
            if (mTracker.shouldTriggerHelpUI(featureName)) {
                mVideoIPHCoordinator.showVideoIPH(tutorial);
                mTracker.dismissed(featureName);
                break;
            }
        }
    }

    private void onClickIPH(Tutorial tutorial) {
        // TODO(shaktisahu): Maybe collect this event when video has been halfway watched.
        mTracker.notifyEvent(VideoTutorialIPHUtils.getClickEvent(tutorial.featureType));

        // Bring up the player and start playing the video.
        VideoPlayerActivity.playVideoTutorial(mContext, tutorial);
    }

    private void onDismissIPH(Tutorial tutorial) {
        mTracker.notifyEvent(VideoTutorialIPHUtils.getDismissEvent(tutorial.featureType));

        // TODO(shaktisahu): Animate this. Maybe add a delay.
        mVideoTutorialService.getTutorials(this::onFetchTutorials);
    }
}
