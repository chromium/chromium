// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.chrome.browser.media.PictureInPictureActivity.PICTURE_IN_PICTURE_ACTION_HISTOGRAM;

import android.app.Activity;
import android.app.RemoteAction;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.Rational;
import android.view.View;

import androidx.lifecycle.Lifecycle;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.UnguessableToken;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.media.PictureInPictureActivity.PictureInPictureButtonAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.overlay_window.PlaybackState;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
import org.chromium.media.MediaFeatures;
import org.chromium.media_session.mojom.MediaSessionAction;
import org.chromium.ui.test.util.DeviceRestriction;

import java.util.ArrayList;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/** Tests for PictureInPictureActivity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
public class PictureInPictureActivityTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final long NATIVE_OVERLAY = 100L;
    private static final long PIP_TIMEOUT_MILLISECONDS = 10000L;

    // Token that the native side will convert to `NATIVE_OVERLAY`
    private final UnguessableToken mNativeWindowToken = UnguessableToken.createForTesting();

    @Mock private PictureInPictureActivity.Natives mNativeMock;

    private Tab mTab;

    // Source rect hint that we'll provide as the video element position.
    private final Rect mSourceRectHint = new Rect(100, 200, 300, 400);

    // Helper to capture the source rect hint bounds that PictureInPictureActivity would like to use
    // for `makeEnterIntoPip`, if any.
    private final PictureInPictureActivity.LaunchIntoPipHelper mLaunchIntoPipHelper =
            new PictureInPictureActivity.LaunchIntoPipHelper() {
                @Override
                public Bundle build(Context activityContext, Rect bounds) {
                    // Store the bounds in the parent class for easy reference.
                    mBounds = bounds;
                    return null;
                }
            };

    // Helper that we replace with `mLaunchIntoPipHelper`, so that we can restore it.
    private PictureInPictureActivity.LaunchIntoPipHelper mOriginalHelper;

    // Most recently provided bounds, if any.
    private Rect mBounds;

    @Before
    public void setUp() {
        mActivityTestRule.startOnBlankPage();
        mTab = mActivityTestRule.getActivityTab();
        PictureInPictureActivityJni.setInstanceForTesting(mNativeMock);
        mOriginalHelper = PictureInPictureActivity.setLaunchIntoPipHelper(mLaunchIntoPipHelper);
        when(mNativeMock.onActivityStart(eq(mNativeWindowToken), any(), any()))
                .thenReturn(NATIVE_OVERLAY);
    }

    @After
    public void teardown() {
        // Restore the original helper.
        PictureInPictureActivity.setLaunchIntoPipHelper(mOriginalHelper);
    }

    @Test
    @MediumTest
    public void testStartActivity() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();

        // The LaunchIntoPipHelper should have been called.
        Assert.assertTrue(mBounds != null);
        Criteria.checkThat(mBounds, Matchers.is(mSourceRectHint));
        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    public void testExitOnClose() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    public void testExitOnCrash() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        testExitOn(activity, () -> WebContentsUtils.simulateRendererKilled(getWebContents()));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "b/353025645")
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testMakeEnterPictureInPictureWithBadSourceRect() throws Throwable {
        mSourceRectHint.left = -1;
        PictureInPictureActivity activity = startPictureInPictureActivity();
        // The pip helper should not be called with trivially bad bounds.
        Assert.assertTrue(mBounds == null);
        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testExitOnBackToTab() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        Configuration newConfig = activity.getResources().getConfiguration();
        testExitOn(
                activity,
                () ->
                        activity.onPictureInPictureModeChanged(
                                /* isInPictureInPictureMode= */ false, newConfig));
        verify(mNativeMock, times(1)).onBackToTab(NATIVE_OVERLAY);
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testResize() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        // Resize to some reasonable size, and verify that native is told about it.
        final int reasonableSize = 10;
        View view = activity.getViewForTesting();
        ThreadUtils.runOnUiThreadBlocking(() -> view.layout(0, 0, reasonableSize, reasonableSize));
        verify(mNativeMock, times(1))
                .onViewSizeChanged(NATIVE_OVERLAY, reasonableSize, reasonableSize);
        // An unreasonably large size should not generate a resize event.
        final int unreasonableSize = activity.getWindowAndroid().getDisplay().getDisplayWidth();
        ThreadUtils.runOnUiThreadBlocking(
                () -> view.layout(0, 0, unreasonableSize, unreasonableSize));
        verify(mNativeMock, times(0)).onViewSizeChanged(anyInt(), anyInt(), anyInt());
        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testMediaActions() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        PictureInPictureActivity.MediaActionButtonsManager manager =
                activity.mMediaActionsButtonsManager;

        activity.updateVisibleActions(new int[] {MediaSessionAction.PLAY});
        activity.setPlaybackState(PlaybackState.PAUSED);
        ArrayList<RemoteAction> actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(1, actions.size());
        Assert.assertEquals(manager.mPlay, actions.get(0));

        activity.setPlaybackState(PlaybackState.PLAYING);
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(manager.mPause, actions.get(0));

        // Both next track and previous track button should be visible when only one of them is
        // enabled. The one that is not handled should be visible and disabled.
        activity.updateVisibleActions(
                new int[] {MediaSessionAction.PLAY, MediaSessionAction.PREVIOUS_TRACK});
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(3, actions.size());
        Assert.assertEquals(manager.mPreviousTrack, actions.get(0));
        Assert.assertEquals(manager.mNextTrack, actions.get(2));
        Assert.assertTrue(actions.get(0).isEnabled());
        Assert.assertFalse(actions.get(2).isEnabled());

        // Both next slide and previous slide button should be visible when only one of them is
        // enabled. The one that is not handled should be visible and disabled.
        activity.updateVisibleActions(
                new int[] {MediaSessionAction.PLAY, MediaSessionAction.PREVIOUS_SLIDE});
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(3, actions.size());
        Assert.assertEquals(manager.mPreviousSlide, actions.get(0));
        Assert.assertEquals(manager.mNextSlide, actions.get(2));
        Assert.assertTrue(actions.get(0).isEnabled());
        Assert.assertFalse(actions.get(2).isEnabled());

        // When all actions are not handled, there should be a dummy action presented to prevent
        // android picture-in-picture from using default MediaSession.
        activity.updateVisibleActions(new int[] {});
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(1, actions.size());
        Assert.assertFalse(actions.get(0).isEnabled());
        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testMediaActionsForVideoConferencing() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        PictureInPictureActivity.MediaActionButtonsManager manager =
                activity.mMediaActionsButtonsManager;

        activity.updateVisibleActions(new int[] {MediaSessionAction.TOGGLE_MICROPHONE});
        ArrayList<RemoteAction> actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(1, actions.size());
        Assert.assertEquals(actions.get(0), manager.mMicrophone.getAction());

        activity.updateVisibleActions(new int[] {MediaSessionAction.TOGGLE_CAMERA});
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(1, actions.size());
        Assert.assertEquals(actions.get(0), manager.mCamera.getAction());

        activity.updateVisibleActions(new int[] {MediaSessionAction.HANG_UP});
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(1, actions.size());
        Assert.assertEquals(actions.get(0), manager.mHangUp);
        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testMediaActionsForTrackControl() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        PictureInPictureActivity.MediaActionButtonsManager manager =
                activity.mMediaActionsButtonsManager;

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PICTURE_IN_PICTURE_ACTION_HISTOGRAM,
                        PictureInPictureButtonAction.NEXT_TRACK);
        activity.updateVisibleActions(new int[] {MediaSessionAction.NEXT_TRACK});
        manager.mNextTrack.getActionIntent().send();
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .nextTrack(eq(NATIVE_OVERLAY));
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PICTURE_IN_PICTURE_ACTION_HISTOGRAM,
                        PictureInPictureButtonAction.PREVIOUS_TRACK);
        activity.updateVisibleActions(new int[] {MediaSessionAction.PREVIOUS_TRACK});
        manager.mPreviousTrack.getActionIntent().send();
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .previousTrack(eq(NATIVE_OVERLAY));
        histogramWatcher.assertExpected();

        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testMediaActionsForSlideControl() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        PictureInPictureActivity.MediaActionButtonsManager manager =
                activity.mMediaActionsButtonsManager;

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PICTURE_IN_PICTURE_ACTION_HISTOGRAM,
                        PictureInPictureButtonAction.NEXT_SLIDE);
        activity.updateVisibleActions(new int[] {MediaSessionAction.NEXT_SLIDE});
        manager.mNextSlide.getActionIntent().send();
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .nextSlide(eq(NATIVE_OVERLAY));
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PICTURE_IN_PICTURE_ACTION_HISTOGRAM,
                        PictureInPictureButtonAction.PREVIOUS_SLIDE);
        activity.updateVisibleActions(new int[] {MediaSessionAction.PREVIOUS_SLIDE});
        manager.mPreviousSlide.getActionIntent().send();
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .previousSlide(eq(NATIVE_OVERLAY));
        histogramWatcher.assertExpected();

        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @EnableFeatures(MediaFeatures.AUTO_PICTURE_IN_PICTURE_ANDROID)
    public void testMediaActionHide() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        PictureInPictureActivity.MediaActionButtonsManager manager =
                activity.mMediaActionsButtonsManager;

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PICTURE_IN_PICTURE_ACTION_HISTOGRAM, PictureInPictureButtonAction.HIDE);
        activity.updateVisibleActions(new int[] {MediaSessionAction.EXIT_PICTURE_IN_PICTURE});
        manager.mHide.getActionIntent().send();
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL))
                .hide(eq(NATIVE_OVERLAY));
        histogramWatcher.assertExpected();

        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testIconAccessibilityString() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        PictureInPictureActivity.MediaActionButtonsManager manager =
                activity.mMediaActionsButtonsManager;

        Assert.assertEquals(
                manager.mPlay.getContentDescription(),
                activity.getString(R.string.accessibility_play));
        Assert.assertEquals(
                manager.mPause.getContentDescription(),
                activity.getString(R.string.accessibility_pause));
        Assert.assertEquals(
                manager.mReplay.getContentDescription(),
                activity.getString(R.string.accessibility_replay));
        Assert.assertEquals(
                manager.mHangUp.getContentDescription(),
                activity.getString(R.string.accessibility_hang_up));
        Assert.assertEquals(
                manager.mPreviousTrack.getContentDescription(),
                activity.getString(R.string.accessibility_previous_track));
        Assert.assertEquals(
                manager.mNextTrack.getContentDescription(),
                activity.getString(R.string.accessibility_next_track));
        Assert.assertEquals(
                manager.mPreviousSlide.getContentDescription(),
                activity.getString(R.string.accessibility_previous_slide));
        Assert.assertEquals(
                manager.mNextSlide.getContentDescription(),
                activity.getString(R.string.accessibility_next_slide));
        Assert.assertEquals(
                manager.mHide.getContentDescription(),
                activity.getString(R.string.accessibility_listen_in_the_background));

        activity.setMicrophoneMuted(false);
        Assert.assertEquals(
                manager.mMicrophone.getAction().getContentDescription(),
                activity.getString(R.string.accessibility_mute_microphone));
        activity.setMicrophoneMuted(true);
        Assert.assertEquals(
                manager.mMicrophone.getAction().getContentDescription(),
                activity.getString(R.string.accessibility_unmute_microphone));

        activity.setCameraState(true);
        Assert.assertEquals(
                manager.mCamera.getAction().getContentDescription(),
                activity.getString(R.string.accessibility_turn_off_camera));
        activity.setCameraState(false);
        Assert.assertEquals(
                manager.mCamera.getAction().getContentDescription(),
                activity.getString(R.string.accessibility_turn_on_camera));

        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @EnableFeatures(MediaFeatures.AUTO_PICTURE_IN_PICTURE_ANDROID)
    public void testActionTrimmingPriority() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        activity.setMaxNumActionsForTesting(3);
        PictureInPictureActivity.MediaActionButtonsManager manager =
                activity.mMediaActionsButtonsManager;

        // With 3 actions available, no trimming should happen.
        activity.updateVisibleActions(
                new int[] {
                    MediaSessionAction.EXIT_PICTURE_IN_PICTURE,
                    MediaSessionAction.PREVIOUS_TRACK,
                    MediaSessionAction.NEXT_TRACK
                });
        ArrayList<RemoteAction> actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals("All 3 actions should be visible", 3, actions.size());
        Assert.assertEquals(manager.mHide, actions.get(0));
        Assert.assertEquals(manager.mPreviousTrack, actions.get(1));
        Assert.assertEquals(manager.mNextTrack, actions.get(2));

        // With 4 actions, Previous Track should be trimmed.
        activity.updateVisibleActions(
                new int[] {
                    MediaSessionAction.EXIT_PICTURE_IN_PICTURE,
                    MediaSessionAction.PREVIOUS_TRACK,
                    MediaSessionAction.PLAY,
                    MediaSessionAction.NEXT_TRACK
                });
        activity.setPlaybackState(PlaybackState.PLAYING);
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals("Should be trimmed to 3 actions", 3, actions.size());
        Assert.assertEquals(manager.mHide, actions.get(0));
        Assert.assertEquals(manager.mPause, actions.get(1)); // PLAYING state means Pause is shown
        Assert.assertEquals(manager.mNextTrack, actions.get(2));
        Assert.assertFalse(actions.contains(manager.mPreviousTrack));

        // With 5 actions, Previous Track and Previous Slide should be trimmed.
        activity.updateVisibleActions(
                new int[] {
                    MediaSessionAction.EXIT_PICTURE_IN_PICTURE,
                    MediaSessionAction.PREVIOUS_TRACK,
                    MediaSessionAction.PREVIOUS_SLIDE,
                    MediaSessionAction.NEXT_SLIDE,
                    MediaSessionAction.NEXT_TRACK
                });
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals("Should be trimmed to 3 actions", 3, actions.size());
        Assert.assertEquals(manager.mHide, actions.get(0));
        Assert.assertEquals(manager.mNextTrack, actions.get(1));
        Assert.assertEquals(manager.mNextSlide, actions.get(2));
        Assert.assertFalse(actions.contains(manager.mPreviousTrack));
        Assert.assertFalse(actions.contains(manager.mPreviousSlide));

        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testActionsInSync() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        PictureInPictureActivity.MediaActionButtonsManager manager =
                activity.mMediaActionsButtonsManager;

        activity.setPlaybackState(PlaybackState.PLAYING);
        activity.setMicrophoneMuted(false);
        activity.setCameraState(true);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PICTURE_IN_PICTURE_ACTION_HISTOGRAM,
                        PictureInPictureButtonAction.TOGGLE_MICROPHONE);
        manager.mMicrophone.getAction().getActionIntent().send();
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .toggleMicrophone(eq(NATIVE_OVERLAY), eq(false));
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PICTURE_IN_PICTURE_ACTION_HISTOGRAM,
                        PictureInPictureButtonAction.TOGGLE_CAMERA);
        manager.mCamera.getAction().getActionIntent().send();
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .toggleCamera(eq(NATIVE_OVERLAY), eq(false));
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PICTURE_IN_PICTURE_ACTION_HISTOGRAM, PictureInPictureButtonAction.PAUSE);
        manager.mPause.getActionIntent().send();
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .togglePlayPause(eq(NATIVE_OVERLAY), eq(false));
        histogramWatcher.assertExpected();

        activity.setPlaybackState(PlaybackState.PAUSED);
        activity.setMicrophoneMuted(true);
        activity.setCameraState(false);

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PICTURE_IN_PICTURE_ACTION_HISTOGRAM, PictureInPictureButtonAction.PLAY);
        manager.mPlay.getActionIntent().send();
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .togglePlayPause(eq(NATIVE_OVERLAY), eq(true));
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PICTURE_IN_PICTURE_ACTION_HISTOGRAM,
                        PictureInPictureButtonAction.TOGGLE_MICROPHONE);
        manager.mMicrophone.getAction().getActionIntent().send();
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .toggleMicrophone(eq(NATIVE_OVERLAY), eq(true));
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PICTURE_IN_PICTURE_ACTION_HISTOGRAM,
                        PictureInPictureButtonAction.TOGGLE_CAMERA);
        manager.mCamera.getAction().getActionIntent().send();
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .toggleCamera(eq(NATIVE_OVERLAY), eq(true));
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        PICTURE_IN_PICTURE_ACTION_HISTOGRAM, PictureInPictureButtonAction.HANG_UP);
        manager.mHangUp.getActionIntent().send();
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .hangUp(eq(NATIVE_OVERLAY));
        histogramWatcher.assertExpected();

        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testNotifyNativeWhenTabClose() throws Throwable {
        mActivityTestRule.skipWindowAndTabStateCleanup();

        PictureInPictureActivity activity = startPictureInPictureActivity();
        testExitOn(activity, () -> mTab.setClosing(/* closing= */ true));
        verify(mNativeMock, times(1)).destroyStartedByJava(NATIVE_OVERLAY);
    }

    @Test
    @MediumTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testPipWindowExitsIfTokenDoesNotExist() throws Throwable {
        // If the window token doesn't produce a native window, then the activity should exit.
        when(mNativeMock.onActivityStart(eq(mNativeWindowToken), any(), any())).thenReturn(0L);
        PictureInPictureActivity activity = launchPictureInPictureActivity();

        // The activity should be destroyed, because its native window is gone.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getLifecycle().getCurrentState(),
                            Matchers.is(Lifecycle.State.DESTROYED));
                },
                PIP_TIMEOUT_MILLISECONDS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        verify(mNativeMock, times(1)).onActivityStart(eq(mNativeWindowToken), eq(activity), any());
        // Nothing should be destroyed, because there was no native window.
        verify(mNativeMock, times(0)).destroyStartedByJava(anyInt());
    }

    private WebContents getWebContents() {
        return mActivityTestRule.getWebContents();
    }

    private void testExitOn(Activity activity, Runnable runnable) throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(() -> runnable.run());

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity == null || activity.isDestroyed(), Matchers.is(true));
                },
                PIP_TIMEOUT_MILLISECONDS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    // Launch a pip activity and wait for it to successfully start.
    private PictureInPictureActivity startPictureInPictureActivity() throws Exception {
        PictureInPictureActivity activity = launchPictureInPictureActivity();
        ActivityTestUtils.waitForFirstLayout(activity);

        verify(mNativeMock, timeout(500).times(1))
                .onActivityStart(eq(mNativeWindowToken), eq(activity), any());

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(activity.isInPictureInPictureMode(), Matchers.is(true));
                },
                PIP_TIMEOUT_MILLISECONDS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        Rational ratio = activity.getAspectRatio();
        Criteria.checkThat(
                ratio,
                Matchers.is(new Rational(mSourceRectHint.width(), mSourceRectHint.height())));

        return activity;
    }

    // Launch a pip activity, but don't wait for it to finish starting.
    private PictureInPictureActivity launchPictureInPictureActivity() throws Exception {
        PictureInPictureActivity activity =
                ActivityTestUtils.launchActivityWithTimeout(
                        InstrumentationRegistry.getInstrumentation(),
                        PictureInPictureActivity.class,
                        new Callable<>() {
                            @Override
                            public Void call() throws TimeoutException {
                                ThreadUtils.runOnUiThreadBlocking(
                                        () ->
                                                PictureInPictureActivity.createActivity(
                                                        mNativeWindowToken,
                                                        mTab,
                                                        mSourceRectHint.left,
                                                        mSourceRectHint.top,
                                                        mSourceRectHint.width(),
                                                        mSourceRectHint.height()));
                                return null;
                            }
                        },
                        PIP_TIMEOUT_MILLISECONDS);

        return activity;
    }
}
