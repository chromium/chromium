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

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.app.Activity;
import android.app.RemoteAction;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.util.Rational;
import android.view.View;

import androidx.annotation.RequiresApi;
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
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.overlay_window.PlaybackState;
import org.chromium.content_public.browser.test.util.WebContentsUtils;
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
@RequiresApi(Build.VERSION_CODES.O)
public class PictureInPictureActivityTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public JniMocker mMocker = new JniMocker();

    private static final long NATIVE_OVERLAY = 100L;
    private static final long PIP_TIMEOUT_MILLISECONDS = 10000L;

    @Mock private PictureInPictureActivity.Natives mNativeMock;

    private Tab mTab;

    // Source rect hint that we'll provide as the video element position.
    private Rect mSourceRectHint = new Rect(100, 200, 300, 400);

    // Helper to capture the source rect hint bounds that PictureInPictureActivity would like to use
    // for `makeEnterIntoPip`, if any.
    private PictureInPictureActivity.LaunchIntoPipHelper mLaunchIntoPipHelper =
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
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
        mMocker.mock(PictureInPictureActivityJni.TEST_HOOKS, mNativeMock);
        mOriginalHelper = PictureInPictureActivity.setLaunchIntoPipHelper(mLaunchIntoPipHelper);
    }

    @After
    public void teardown() {
        // Restore the original helper.
        PictureInPictureActivity.setLaunchIntoPipHelper(mOriginalHelper);
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testStartActivity() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();

        // The LaunchIntoPipHelper should have been called.
        Assert.assertTrue(mBounds != null);
        Criteria.checkThat(mBounds, Matchers.is(mSourceRectHint));
        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testExitOnClose() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testExitOnCrash() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        testExitOn(activity, () -> WebContentsUtils.simulateRendererKilled(getWebContents()));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "b/353025645")
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testMediaActions() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        PictureInPictureActivity.MediaActionButtonsManager manager =
                activity.mMediaActionsButtonsManager;

        activity.updateVisibleActions(new int[] {MediaSessionAction.PLAY});
        activity.setPlaybackState(PlaybackState.PAUSED);
        ArrayList<RemoteAction> actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(actions.size(), 1);
        Assert.assertEquals(actions.get(0), manager.mPlay);

        activity.setPlaybackState(PlaybackState.PLAYING);
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(actions.get(0), manager.mPause);

        // Both next track and previous track button should be visible when only one of them is
        // enabled. The one that is not handled should be visible and disabled.
        activity.updateVisibleActions(
                new int[] {MediaSessionAction.PLAY, MediaSessionAction.PREVIOUS_TRACK});
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(actions.size(), 3);
        Assert.assertEquals(actions.get(0), manager.mPreviousTrack);
        Assert.assertEquals(actions.get(2), manager.mNextTrack);
        Assert.assertTrue(actions.get(0).isEnabled());
        Assert.assertFalse(actions.get(2).isEnabled());

        // Both next slide and previous slide button should be visible when only one of them is
        // enabled. The one that is not handled should be visible and disabled.
        activity.updateVisibleActions(
                new int[] {MediaSessionAction.PLAY, MediaSessionAction.PREVIOUS_SLIDE});
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(actions.size(), 3);
        Assert.assertEquals(actions.get(0), manager.mPreviousSlide);
        Assert.assertEquals(actions.get(2), manager.mNextSlide);
        Assert.assertTrue(actions.get(0).isEnabled());
        Assert.assertFalse(actions.get(2).isEnabled());

        // When all actions are not handled, there should be a dummy action presented to prevent
        // android picture-in-picture from using default MediaSession.
        activity.updateVisibleActions(new int[] {});
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(actions.size(), 1);
        Assert.assertFalse(actions.get(0).isEnabled());
        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testMediaActionsForVideoConferencing() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        PictureInPictureActivity.MediaActionButtonsManager manager =
                activity.mMediaActionsButtonsManager;

        activity.updateVisibleActions(new int[] {MediaSessionAction.TOGGLE_MICROPHONE});
        ArrayList<RemoteAction> actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(actions.size(), 1);
        Assert.assertEquals(actions.get(0), manager.mMicrophone.getAction());

        activity.updateVisibleActions(new int[] {MediaSessionAction.TOGGLE_CAMERA});
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(actions.size(), 1);
        Assert.assertEquals(actions.get(0), manager.mCamera.getAction());

        activity.updateVisibleActions(new int[] {MediaSessionAction.HANG_UP});
        actions = manager.getActionsForPictureInPictureParams();
        Assert.assertEquals(actions.size(), 1);
        Assert.assertEquals(actions.get(0), manager.mHangUp);
        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "b/353357051")
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testActionsInSync() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        PictureInPictureActivity.MediaActionButtonsManager manager =
                activity.mMediaActionsButtonsManager;

        activity.setPlaybackState(PlaybackState.PLAYING);
        activity.setMicrophoneMuted(false);
        activity.setCameraState(true);

        manager.mMicrophone.getAction().getActionIntent().send();
        manager.mCamera.getAction().getActionIntent().send();
        manager.mPause.getActionIntent().send();

        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .togglePlayPause(eq(NATIVE_OVERLAY), eq(false));
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .toggleMicrophone(eq(NATIVE_OVERLAY), eq(false));
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .toggleCamera(eq(NATIVE_OVERLAY), eq(false));

        activity.setPlaybackState(PlaybackState.PAUSED);
        activity.setMicrophoneMuted(true);
        activity.setCameraState(false);
        manager.mPlay.getActionIntent().send();
        manager.mMicrophone.getAction().getActionIntent().send();
        manager.mCamera.getAction().getActionIntent().send();

        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .togglePlayPause(eq(NATIVE_OVERLAY), eq(true));
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .toggleMicrophone(eq(NATIVE_OVERLAY), eq(true));
        verify(mNativeMock, timeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL).times(1))
                .toggleCamera(eq(NATIVE_OVERLAY), eq(true));

        testExitOn(activity, () -> activity.close());
    }

    @Test
    @MediumTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testNotifyNativeWhenTabClose() throws Throwable {
        PictureInPictureActivity activity = startPictureInPictureActivity();
        testExitOn(activity, () -> mTab.setClosing(/* closing= */ true));
        verify(mNativeMock, times(1)).destroy(NATIVE_OVERLAY);
    }

    private WebContents getWebContents() {
        return mActivityTestRule.getActivity().getCurrentWebContents();
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

    private PictureInPictureActivity startPictureInPictureActivity() throws Exception {
        PictureInPictureActivity activity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        PictureInPictureActivity.class,
                        new Callable<Void>() {
                            @Override
                            public Void call() throws TimeoutException {
                                ThreadUtils.runOnUiThreadBlocking(
                                        () ->
                                                PictureInPictureActivity.createActivity(
                                                        NATIVE_OVERLAY,
                                                        mTab,
                                                        mSourceRectHint.left,
                                                        mSourceRectHint.top,
                                                        mSourceRectHint.width(),
                                                        mSourceRectHint.height()));
                                return null;
                            }
                        });

        verify(mNativeMock, timeout(500).times(1))
                .onActivityStart(eq(NATIVE_OVERLAY), eq(activity), any());

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
}
