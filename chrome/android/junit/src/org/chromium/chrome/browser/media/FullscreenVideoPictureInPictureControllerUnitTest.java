// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.PowerManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.shadows.ShadowPackageManager;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.ContextUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.MediaSession;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.mock.MockWebContents;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Test FullscreenVideoPictureInPictureController. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        sdk = Build.VERSION_CODES.O,
        shadows = {ShadowPackageManager.class, ShadowPostTask.class, ShadowSystemClock.class})
public class FullscreenVideoPictureInPictureControllerUnitTest {
    private static final int TAB_ID = 0;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Activity mActivity;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private Tab mTab;
    @Mock private MockWebContents mWebContents;
    @Mock private InfoBarContainer mInfoBarContainer;
    @Mock private MediaSession mMediaSession;
    @Mock private PowerManager mPowerManager;

    // Not a mock, since it's just a container and `final` anyway.
    private final UserDataHost mUserDataHost = new UserDataHost();

    private FullscreenVideoPictureInPictureController mController;

    @Captor private ArgumentCaptor<FullscreenManager.Observer> mFullscreenObserverCaptor;
    @Captor private ArgumentCaptor<WebContentsObserver> mWebContentsObserverCaptor;

    /** List of tasks that were posted, including with delay. Run with runUntilIdle(). */
    private List<Runnable> mRunnables = new ArrayList<>();

    /** Class to be tested, extended to allow us to provide some hooks. */
    class FullscreenVideoPictureInPictureControllerWithOverrides
            extends FullscreenVideoPictureInPictureController {
        public FullscreenVideoPictureInPictureControllerWithOverrides(
                Activity activity,
                ActivityTabProvider activityTabProvider,
                FullscreenManager fullscreenManager) {
            super(activity, activityTabProvider, fullscreenManager);
        }

        @Override
        InfoBarContainer getInfoBarContainerForTab(Tab tab) {
            return mInfoBarContainer;
        }

        @Override
        MediaSession getMediaSession() {
            return mMediaSession;
        }

        @Override
        void assertLibraryLoaderIsInitialized() {}
    }

    @Before
    public void setUp() {
        ShadowLog.stream = System.out;

        ShadowPostTask.setTestImpl(
                new ShadowPostTask.TestImpl() {
                    @Override
                    public void postDelayedTask(int taskTraits, Runnable task, long delay) {
                        mRunnables.add(task);
                    }
                });

        Context context = ContextUtils.getApplicationContext();
        ShadowPackageManager shadowPackageManager = Shadows.shadowOf(context.getPackageManager());
        shadowPackageManager.setSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE, true);

        when(mActivity.getSystemService(Context.ACTIVITY_SERVICE))
                .thenReturn((ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE));
        when(mActivity.getSystemService(Context.POWER_SERVICE)).thenReturn(mPowerManager);
        when(mActivity.getPackageManager()).thenReturn(context.getPackageManager());
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mPowerManager.isInteractive()).thenReturn(true);

        mController =
                new FullscreenVideoPictureInPictureControllerWithOverrides(
                        mActivity, mActivityTabProvider, mFullscreenManager);
    }

    /** Enter pip, and advance the clock so that it is willing to exit. */
    private void enterPip() {
        mController.onEnteredPictureInPictureMode();
        ShadowSystemClock.advanceBy(
                FullscreenVideoPictureInPictureController.MIN_EXIT_DELAY_MILLIS + 10L,
                TimeUnit.MILLISECONDS);
    }

    /** Set up the mocks to claim that there is / is not full screen video playback */
    private void setHasFullscreenVideo(boolean hasVideo) {
        when(mWebContents.hasActiveEffectivelyFullscreenVideo()).thenReturn(hasVideo);
        when(mWebContents.isPictureInPictureAllowedForFullscreenVideo()).thenReturn(hasVideo);
    }

    /** Run any runnables, including any delayed ones. */
    private void runUntilIdle() {
        // In case the tasks post more tasks, start a new list.
        List<Runnable> runnables = mRunnables;
        mRunnables = new ArrayList<>();

        for (Runnable r : runnables) {
            r.run();
        }
    }

    /** Verify that full screen video will try to enter PiP */
    @Test
    public void attemptPictureInPictureSuccessfully() {
        setHasFullscreenVideo(true);
        mController.attemptPictureInPicture();
        verify(mActivity, times(1)).enterPictureInPictureMode(any());
    }

    /** Verify that lack of full screen video results in no pip */
    @Test
    public void pictureInPictureFailsWithoutVideo() {
        setHasFullscreenVideo(false);
        mController.attemptPictureInPicture();
        verify(mActivity, times(0)).enterPictureInPictureMode(any());
    }

    /** After starting pip, dismiss should move the task to back if it's been long enough. */
    @Test
    public void pictureInPictureIsDismissedAfterEnoughTime() {
        enterPip();
        verify(mFullscreenManager).addObserver(mFullscreenObserverCaptor.capture());
        mFullscreenObserverCaptor.getValue().onExitFullscreen(mTab);
        verify(mActivity, times(1)).moveTaskToBack(true);
        // Verify that a second call does not dismiss a second time.
        mFullscreenObserverCaptor.getValue().onExitFullscreen(mTab);
        verify(mActivity, times(1)).moveTaskToBack(true);
    }

    /**
     * After starting pip, dismiss should not move the task to back if it hasn't been long enough.
     */
    @Test
    public void pictureInPictureIsNotDismissedImmediately() {
        // Do not call `enterPip()`, because we do not want to advance the clock.
        mController.onEnteredPictureInPictureMode();
        verify(mFullscreenManager).addObserver(mFullscreenObserverCaptor.capture());
        // Leave the clock at 0, which should cause it to post rather than call back.
        mFullscreenObserverCaptor.getValue().onExitFullscreen(mTab);
        verify(mActivity, times(0)).moveTaskToBack(true);
        // Advance the clock so that it has been long enough, and run all delayed tasks.
        ShadowSystemClock.advanceBy(
                FullscreenVideoPictureInPictureController.MIN_EXIT_DELAY_MILLIS + 10L,
                TimeUnit.MILLISECONDS);
        runUntilIdle();
        verify(mActivity, times(1)).moveTaskToBack(true);
    }

    /** Stash will pause the video, then restart it when un-stashed. */
    @Test
    public void pictureInPicturePausesAndResumesWhenStashed() {
        mController.onEnteredPictureInPictureMode();
        verify(mWebContents).addObserver(mWebContentsObserverCaptor.capture());

        // Stash while media is playing.
        mWebContentsObserverCaptor.getValue().mediaStartedPlaying();
        mController.onStashReported(true);
        verify(mMediaSession, times(1)).suspend();
        mWebContentsObserverCaptor.getValue().mediaStoppedPlaying();

        // Un-stash while media is still paused.
        mController.onStashReported(false);
        ShadowSystemClock.advanceBy(
                FullscreenVideoPictureInPictureController.UNSTASH_DELAY_MILLIS + 10L,
                TimeUnit.MILLISECONDS);
        runUntilIdle();
        verify(mMediaSession, times(1)).resume();
    }

    /**
     * Stash will neither pause on stash nor resume on unstash video that's paused when the pip
     * window is stashed.
     */
    @Test
    public void pictureInPictureDoesNotChangeAlreadyPausedVideoOnStash() {
        mController.onEnteredPictureInPictureMode();
        verify(mWebContents).addObserver(mWebContentsObserverCaptor.capture());
        // Make sure that the video is paused.
        mWebContentsObserverCaptor.getValue().mediaStoppedPlaying();

        // Stashing paused video should do nothing.
        mController.onStashReported(true);
        verify(mMediaSession, times(0)).suspend();

        // Un-stash should also do nothing.
        mController.onStashReported(false);
        ShadowSystemClock.advanceBy(
                FullscreenVideoPictureInPictureController.UNSTASH_DELAY_MILLIS + 10L,
                TimeUnit.MILLISECONDS);
        runUntilIdle();
        verify(mMediaSession, times(0)).resume();
    }

    /** If video starts playing during a normal stash, unstash should no-op. */
    @Test
    public void pictureInPictureDoesNotResumeOnUnstashIfAlreadyPlaying() {
        mController.onEnteredPictureInPictureMode();
        verify(mWebContents).addObserver(mWebContentsObserverCaptor.capture());
        // Stash normally.
        mWebContentsObserverCaptor.getValue().mediaStartedPlaying();
        mController.onStashReported(true);
        verify(mMediaSession, times(1)).suspend();
        mWebContentsObserverCaptor.getValue().mediaStoppedPlaying();

        // Restart playback while still stashed.
        mWebContentsObserverCaptor.getValue().mediaStartedPlaying();

        // Un-stash should do nothing since there's nothing to do.
        mController.onStashReported(false);
        ShadowSystemClock.advanceBy(
                FullscreenVideoPictureInPictureController.UNSTASH_DELAY_MILLIS + 10L,
                TimeUnit.MILLISECONDS);
        runUntilIdle();
        verify(mMediaSession, times(0)).resume();
    }

    @Test
    public void pictureInPictureDoesNotHideIfScreenIsOff() {
        enterPip();
        verify(mWebContents).addObserver(mWebContentsObserverCaptor.capture());
        mWebContentsObserverCaptor.getValue().mediaStartedPlaying();
        when(mPowerManager.isInteractive()).thenReturn(false);

        // Expect that there will be no attempt to exit pip yet, because the screen is off.
        // Instead, it will be deferred until it unlocks and we get an `onStart` about it.  We call
        // `onResume` simply as a convenient way to indicate that the pip window should be closed.
        mController.onResume();
        verify(mActivity, times(0)).moveTaskToBack(true);
        // The media should be paused, though, just as if pip had closed.
        verify(mMediaSession, times(1)).suspend();

        // When the device is unlocked, we will get `onStart`.  This should cause pip to close
        // because it's still deferred from the `onResume` call, above.
        when(mPowerManager.isInteractive()).thenReturn(true);
        mController.onStart();
        verify(mActivity, times(1)).moveTaskToBack(true);
    }

    @Test
    public void pictureInPictureDoesNotHideForOnStart() {
        // onStart shouldn't exit pip in the normal case; 'started' is the typical stage for pip.
        enterPip();

        mController.onStart();
        verify(mActivity, times(0)).moveTaskToBack(true);
    }

    @Test
    public void pictureInPictureDoesHideForOnResume() {
        // onResume should hide pip since the screen is on.  While `onResume` is not typically sent
        // while the app is in pip, in practice there can be `onResume / onPause` sometimes.  In
        // those cases, we want to exit pip because we also left fullscreen, but that's not always
        // reliable.  So, `onResume` by itself also should exit pip.
        enterPip();

        mController.onResume();
        verify(mActivity, times(1)).moveTaskToBack(true);
    }
}
