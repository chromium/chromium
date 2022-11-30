// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.video_tutorials;

import android.content.Context;
import android.content.Intent;
import android.view.ViewStub;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialService;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialServiceFactory;
import org.chromium.chrome.browser.video_tutorials.iph.VideoIPHCoordinator;
import org.chromium.chrome.browser.video_tutorials.iph.VideoTutorialIPHUtils;
import org.chromium.chrome.browser.video_tutorials.metrics.VideoTutorialMetrics;
import org.chromium.chrome.browser.video_tutorials.metrics.VideoTutorialMetrics.UserAction;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.image_fetcher.ImageFetcher;
import org.chromium.components.image_fetcher.ImageFetcherConfig;
import org.chromium.components.image_fetcher.ImageFetcherFactory;

import java.util.ArrayList;
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
        mVideoIPHCoordinator = createVideoIPHCoordinator(
                viewStub, createImageFetcher(profile), this::onClickIPH, this::onDismissIPH);
        mVideoTutorialService = VideoTutorialServiceFactory.getForProfile(profile);
        mVideoTutorialService.getTutorials(this::onFetchTutorials);
    }

    private void onFetchTutorials(List<Tutorial> tutorials) {
        if (tutorials.isEmpty()) return;

        // Add the summary tutorial to the list.
        List<Tutorial> tutorialsCopy = new ArrayList<>(tutorials);
        mVideoTutorialService.getTutorial(FeatureType.SUMMARY, tutorial -> {
            if (tutorial != null) tutorialsCopy.add(tutorial);

            mTracker.addOnInitializedCallback(success -> {
                if (!success) return;
                showFirstEligibleIPH(tutorialsCopy);
            });
        });
    }

    private void showFirstEligibleIPH(List<Tutorial> tutorials) {
        for (Tutorial tutorial : tutorials) {
            String featureName = VideoTutorialIPHUtils.getFeatureNameForNTP(tutorial.featureType);
            if (featureName == null) continue;
            if (mTracker.shouldTriggerHelpUI(featureName)) {
                VideoTutorialMetrics.recordUserAction(
                        tutorial.featureType, UserAction.IPH_NTP_SHOWN);
                mVideoIPHCoordinator.showVideoIPH(tutorial);
                mTracker.dismissed(featureName);
                break;
            }
        }
    }

    private void onClickIPH(Tutorial tutorial) {
        // TODO(shaktisahu): Maybe collect this event when video has been halfway watched.
        mTracker.notifyEvent(VideoTutorialIPHUtils.getClickEvent(tutorial.featureType));
        VideoTutorialMetrics.recordUserAction(tutorial.featureType, UserAction.IPH_NTP_CLICKED);

        // Bring up the player and start playing the video.
        if (tutorial.featureType == FeatureType.SUMMARY) {
            launchTutorialListActivity();
        } else {
            launchVideoPlayer(tutorial);
        }
    }

    private void onDismissIPH(Tutorial tutorial) {
        mTracker.notifyEvent(VideoTutorialIPHUtils.getDismissEvent(tutorial.featureType));
        VideoTutorialMetrics.recordUserAction(tutorial.featureType, UserAction.IPH_NTP_DISMISSED);

        // TODO(shaktisahu): Animate this. Maybe add a delay.
        mVideoTutorialService.getTutorials(this::onFetchTutorials);
    }

    @VisibleForTesting
    protected void launchVideoPlayer(Tutorial tutorial) {
        VideoPlayerActivity.playVideoTutorial(mContext, tutorial.featureType);
    }

    @VisibleForTesting
    protected void launchTutorialListActivity() {
        Intent intent = new Intent();
        intent.setClass(mContext, VideoTutorialListActivity.class);
        mContext.startActivity(intent);
    }

    @VisibleForTesting
    protected ImageFetcher createImageFetcher(Profile profile) {
        return ImageFetcherFactory.createImageFetcher(ImageFetcherConfig.IN_MEMORY_WITH_DISK_CACHE,
                profile.getProfileKey(), GlobalDiscardableReferencePool.getReferencePool());
    }

    @VisibleForTesting
    protected VideoIPHCoordinator createVideoIPHCoordinator(ViewStub viewStub,
            ImageFetcher imageFetcher, Callback<Tutorial> clickListener,
            Callback<Tutorial> dismissListener) {
        return VideoTutorialServiceFactory.createVideoIPHCoordinator(
                viewStub, imageFetcher, clickListener, dismissListener);
    }
}
