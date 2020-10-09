// Copyright 2020 The Chromium Authors. All rights reserved.
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
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.video_tutorials.FeatureType;
import org.chromium.chrome.browser.video_tutorials.Tutorial;
import org.chromium.chrome.browser.video_tutorials.VideoTutorialServiceFactory;
import org.chromium.chrome.browser.video_tutorials.iph.VideoIPHCoordinator;
import org.chromium.chrome.browser.video_tutorials.test.TestVideoTutorialService;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.feature_engagement.Tracker;

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
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);
        Mockito.when(mViewStub.getContext()).thenReturn(mContext);

        Map<String, Boolean> features = new HashMap<>();
        features.put(ChromeFeatureList.VIDEO_TUTORIALS, true);
        ChromeFeatureList.setTestFeatures(features);

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
        Mockito.when(mTracker.shouldTriggerHelpUI(Mockito.anyString())).thenReturn(false, true);
        mVideoIPHManager = new TestNewTabPageVideoIPHManager(mViewStub, mProfile);
        Mockito.verify(mVideoIPHCoordinator)
                .showVideoIPH(mTestVideoTutorialService.getTestTutorials().get(1));

        // Click on the IPH. video player should be launched.
        mIPHClickListener.onResult(mTestVideoTutorialService.getTestTutorials().get(1));
        assertThat(mVideoIPHManager.videoPlayerLaunched, equalTo(true));
        assertThat(mVideoIPHManager.videoListLaunched, equalTo(false));
    }

    @Test
    public void testShowSummaryIPH() {
        // We have already seen all the tutorials. Now summary card should show up.
        Mockito.when(mTracker.shouldTriggerHelpUI(Mockito.anyString()))
                .thenReturn(false, false, false, true);
        mVideoIPHManager = new TestNewTabPageVideoIPHManager(mViewStub, mProfile);

        ArgumentCaptor<Tutorial> tutorialArgument = ArgumentCaptor.forClass(Tutorial.class);
        Mockito.verify(mVideoIPHCoordinator).showVideoIPH(tutorialArgument.capture());
        assertThat(tutorialArgument.getValue().featureType, equalTo(FeatureType.SUMMARY));

        // Click on the IPH. Video list activity should be launched.
        mIPHClickListener.onResult(tutorialArgument.getValue());
        assertThat(mVideoIPHManager.videoPlayerLaunched, equalTo(false));
        assertThat(mVideoIPHManager.videoListLaunched, equalTo(true));
    }
}
