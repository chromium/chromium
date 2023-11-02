// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.video_tutorials;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;

import android.content.Context;
import android.view.ViewStub;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.AdditionalAnswers;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialServiceFactory;
import org.chromium.chrome.browser.video_tutorials.iph.VideoIPHCoordinator;
import org.chromium.chrome.browser.video_tutorials.iph.VideoTutorialIPHUtils;
import org.chromium.chrome.browser.video_tutorials.test.TestVideoTutorialService;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerDetails;
import org.chromium.components.image_fetcher.ImageFetcher;

import java.util.HashMap;
import java.util.Map;

/**
 * Unit tests for {@link NewTabPageVideoIPHManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures(ChromeFeatureList.VIDEO_TUTORIALS)
public class NewTabPageVideoIPHManagerTest {
    @Mock
    private Context mContext;
    @Mock
    private ViewStub mViewStub;
    @Mock
    private Profile mProfile;
    @Mock
    private ImageFetcher mImageFetcher;
    @Mock
    private Tracker mTracker;
    @Mock
    private VideoIPHCoordinator mVideoIPHCoordinator;

    private TestVideoTutorialService mTestVideoTutorialService;
    private TestNewTabPageVideoIPHManager mVideoIPHManager;
    private Callback<Tutorial> mIPHClickListener;
    private Callback<Tutorial> mIPHDismissListener;

    private class TestNewTabPageVideoIPHManager extends NewTabPageVideoIPHManager {
        public boolean videoPlayerLaunched;
        public boolean videoListLaunched;

        public TestNewTabPageVideoIPHManager(ViewStub viewStub, Profile profile) {
            super(viewStub, profile);
        }

        @Override
        protected VideoIPHCoordinator createVideoIPHCoordinator(ViewStub viewStub,
                ImageFetcher imageFetcher, Callback<Tutorial> clickListener,
                Callback<Tutorial> dismissListener) {
            mIPHClickListener = clickListener;
            mIPHDismissListener = dismissListener;
            return mVideoIPHCoordinator;
        }

        @Override
        protected void launchVideoPlayer(Tutorial tutorial) {
            videoPlayerLaunched = true;
        }

        @Override
        protected void launchTutorialListActivity() {
            videoListLaunched = true;
        }

        @Override
        protected ImageFetcher createImageFetcher(Profile profile) {
            return mImageFetcher;
        }
    }

    @Before
    public void setUp() {
        UmaRecorderHolder.resetForTesting();
        MockitoAnnotations.initMocks(this);
        Mockito.when(mViewStub.getContext()).thenReturn(mContext);

        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.VIDEO_TUTORIALS, true);
        FeatureList.setTestFeatures(features);

        mTestVideoTutorialService = new TestVideoTutorialService();
        VideoTutorialServiceFactory.setVideoTutorialServiceForTesting(mTestVideoTutorialService);
        Mockito.doAnswer(AdditionalAnswers.answerVoid(
                                 (Callback<Boolean> callback) -> callback.onResult(true)))
                .when(mTracker)
                .addOnInitializedCallback(Mockito.any());
        TrackerFactory.setTrackerForTests(mTracker);
    }

    @Test
    public void testShowFirstEnabledIPH() {
        // We have already watched the first tutorial, the second one should show up as IPH card.
        Tutorial testTutorial = mTestVideoTutorialService.getTestTutorials().get(1);
        Mockito.when(mTracker.shouldTriggerHelpUI(Mockito.anyString())).thenReturn(false, true);
        Mockito.when(mTracker.shouldTriggerHelpUIWithSnooze(Mockito.anyString()))
                .thenReturn(new TriggerDetails(false, false), new TriggerDetails(true, false));
        mVideoIPHManager = new TestNewTabPageVideoIPHManager(mViewStub, mProfile);
        Mockito.verify(mVideoIPHCoordinator).showVideoIPH(testTutorial);

        // Click on the IPH. The video player should be launched.
        mIPHClickListener.onResult(testTutorial);
        assertThat(mVideoIPHManager.videoPlayerLaunched, equalTo(true));
        assertThat(mVideoIPHManager.videoListLaunched, equalTo(false));
        Mockito.verify(mTracker, Mockito.times(1))
                .notifyEvent(VideoTutorialIPHUtils.getClickEvent(testTutorial.featureType));
    }

    @Test
    public void testShowSummaryIPH() {
        // We have already seen all the tutorials. Now summary card should show up.
        Mockito.when(mTracker.shouldTriggerHelpUI(Mockito.anyString()))
                .thenReturn(false, false, false, true);
        Mockito.when(mTracker.shouldTriggerHelpUIWithSnooze(Mockito.anyString()))
                .thenReturn(new TriggerDetails(false, false), new TriggerDetails(false, false),
                        new TriggerDetails(false, false), new TriggerDetails(true, false));

        mVideoIPHManager = new TestNewTabPageVideoIPHManager(mViewStub, mProfile);

        ArgumentCaptor<Tutorial> tutorialArgument = ArgumentCaptor.forClass(Tutorial.class);
        Mockito.verify(mVideoIPHCoordinator).showVideoIPH(tutorialArgument.capture());
        assertThat(tutorialArgument.getValue().featureType, equalTo(FeatureType.SUMMARY));

        // Click on the IPH. Video list activity should be launched.
        mIPHClickListener.onResult(tutorialArgument.getValue());
        assertThat(mVideoIPHManager.videoPlayerLaunched, equalTo(false));
        assertThat(mVideoIPHManager.videoListLaunched, equalTo(true));
    }

    @Test
    public void testDismissIPH() {
        // Show a tutorial IPH.
        Tutorial testTutorial = mTestVideoTutorialService.getTestTutorials().get(0);
        Mockito.when(mTracker.shouldTriggerHelpUI(Mockito.anyString())).thenReturn(true);
        Mockito.when(mTracker.shouldTriggerHelpUIWithSnooze(Mockito.anyString()))
                .thenReturn(new TriggerDetails(true, false));

        mVideoIPHManager = new TestNewTabPageVideoIPHManager(mViewStub, mProfile);
        Mockito.verify(mVideoIPHCoordinator).showVideoIPH(testTutorial);

        // Dismiss the IPH. The tracker should record this event.
        mIPHDismissListener.onResult(testTutorial);
        Mockito.verify(mTracker, Mockito.times(1))
                .notifyEvent(VideoTutorialIPHUtils.getDismissEvent(testTutorial.featureType));
        assertThat(mVideoIPHManager.videoPlayerLaunched, equalTo(false));
        assertThat(mVideoIPHManager.videoListLaunched, equalTo(false));
    }

    @Test
    public void testClickEventForFeatureTypes() {
        assertThat(VideoTutorialIPHUtils.getClickEvent(FeatureType.SUMMARY),
                equalTo(EventConstants.VIDEO_TUTORIAL_CLICKED_SUMMARY));
        assertThat(VideoTutorialIPHUtils.getClickEvent(FeatureType.CHROME_INTRO),
                equalTo(EventConstants.VIDEO_TUTORIAL_CLICKED_CHROME_INTRO));
        assertThat(VideoTutorialIPHUtils.getClickEvent(FeatureType.DOWNLOAD),
                equalTo(EventConstants.VIDEO_TUTORIAL_CLICKED_DOWNLOAD));
        assertThat(VideoTutorialIPHUtils.getClickEvent(FeatureType.SEARCH),
                equalTo(EventConstants.VIDEO_TUTORIAL_CLICKED_SEARCH));
        assertThat(VideoTutorialIPHUtils.getClickEvent(FeatureType.VOICE_SEARCH),
                equalTo(EventConstants.VIDEO_TUTORIAL_CLICKED_VOICE_SEARCH));
    }

    @Test
    public void testDismissEventForFeatureTypes() {
        assertThat(VideoTutorialIPHUtils.getDismissEvent(FeatureType.SUMMARY),
                equalTo(EventConstants.VIDEO_TUTORIAL_DISMISSED_SUMMARY));
        assertThat(VideoTutorialIPHUtils.getDismissEvent(FeatureType.CHROME_INTRO),
                equalTo(EventConstants.VIDEO_TUTORIAL_DISMISSED_CHROME_INTRO));
        assertThat(VideoTutorialIPHUtils.getDismissEvent(FeatureType.DOWNLOAD),
                equalTo(EventConstants.VIDEO_TUTORIAL_DISMISSED_DOWNLOAD));
        assertThat(VideoTutorialIPHUtils.getDismissEvent(FeatureType.SEARCH),
                equalTo(EventConstants.VIDEO_TUTORIAL_DISMISSED_SEARCH));
        assertThat(VideoTutorialIPHUtils.getDismissEvent(FeatureType.VOICE_SEARCH),
                equalTo(EventConstants.VIDEO_TUTORIAL_DISMISSED_VOICE_SEARCH));
    }

    @Test
    public void testNTPFeatureNameForFeatureTypes() {
        assertThat(VideoTutorialIPHUtils.getFeatureNameForNTP(FeatureType.SUMMARY),
                equalTo(FeatureConstants.VIDEO_TUTORIAL_NTP_SUMMARY_FEATURE));
        assertThat(VideoTutorialIPHUtils.getFeatureNameForNTP(FeatureType.CHROME_INTRO),
                equalTo(FeatureConstants.VIDEO_TUTORIAL_NTP_CHROME_INTRO_FEATURE));
        assertThat(VideoTutorialIPHUtils.getFeatureNameForNTP(FeatureType.DOWNLOAD),
                equalTo(FeatureConstants.VIDEO_TUTORIAL_NTP_DOWNLOAD_FEATURE));
        assertThat(VideoTutorialIPHUtils.getFeatureNameForNTP(FeatureType.SEARCH),
                equalTo(FeatureConstants.VIDEO_TUTORIAL_NTP_SEARCH_FEATURE));
        assertThat(VideoTutorialIPHUtils.getFeatureNameForNTP(FeatureType.VOICE_SEARCH),
                equalTo(FeatureConstants.VIDEO_TUTORIAL_NTP_VOICE_SEARCH_FEATURE));
        assertThat(VideoTutorialIPHUtils.getFeatureNameForNTP(FeatureType.INVALID), equalTo(null));
    }
}
