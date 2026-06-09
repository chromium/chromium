// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import static org.junit.Assume.assumeTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UnguessableToken;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.media.VideoOverlayActivity;
import org.chromium.chrome.browser.media.VideoOverlayActivityJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;
import org.chromium.content_public.browser.overlay_window.PlaybackState;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/** Tests for ImmersiveVideoPlaybackActivity. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ImmersiveVideoPlaybackActivityTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String NATIVE_TOKEN_KEY =
            "org.chromium.chrome.browser.media.VideoOverlayActivity.NativeToken";
    private static final String WEB_CONTENTS_KEY =
            "org.chromium.chrome.browser.media.VideoOverlayActivity.WebContents";

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final long NATIVE_OVERLAY = 100L;
    private static final long TIMEOUT_MILLISECONDS = 10000L;
    private static final long DURATION_MS = 1000L;
    private static final long POSITION_MS = 500L;
    private static final float PLAYBACK_RATE = 1.0f;
    private static final int VIDEO_WIDTH = 640;
    private static final int VIDEO_HEIGHT = 360;
    private static final @ImmersiveStereoMode int STEREO_MODE = ImmersiveStereoMode.MONO;
    private static final @ImmersiveProjectionType int PROJECTION_TYPE =
            ImmersiveProjectionType.QUAD;

    private final UnguessableToken mNativeWindowToken = UnguessableToken.createForTesting();

    @Mock private VideoOverlayActivity.Natives mNativeMock;
    @Mock private ImmersiveVideoPlaybackCoordinator mCoordinatorMock;

    private Tab mTab;
    private boolean mIsRealXr;

    @Before
    public void setUp() {
        mIsRealXr = DeviceInfo.isXr();
        mActivityTestRule.startOnBlankPage();
        mTab = mActivityTestRule.getActivityTab();
        VideoOverlayActivityJni.setInstanceForTesting(mNativeMock);
        when(mNativeMock.onActivityStart(eq(mNativeWindowToken), any(), any()))
                .thenReturn(NATIVE_OVERLAY);
        DeviceInfo.setIsXrForTesting(true);

        ImmersiveVideoPlaybackActivity.setPlaybackCoordinatorForTesting(mCoordinatorMock);
    }

    /** Tests that the real activity starts successfully on a device with actual XR capabilities. */
    @Test
    @MediumTest
    public void testStartActivityReal() throws Throwable {
        assumeTrue(mIsRealXr);
        ImmersiveVideoPlaybackActivity.setPlaybackCoordinatorForTesting(null);
        ImmersiveVideoPlaybackActivity activity = startImmersiveVideoPlaybackActivity();
        Assert.assertNotNull(activity);
        testExitOn(activity, () -> activity.close());
    }

    /** Tests that the activity starts successfully when using the mocked coordinator. */
    @Test
    @MediumTest
    public void testStartActivityMock() throws Throwable {
        ImmersiveVideoPlaybackActivity activity = startImmersiveVideoPlaybackActivity();
        Assert.assertNotNull(activity);
        testExitOn(activity, () -> activity.close());
    }

    /**
     * Tests that the activity handles non-XR devices correctly by bypassing initialization and
     * self-terminating.
     */
    @Test
    @MediumTest
    public void testStartActivityNonXr() throws Throwable {
        DeviceInfo.setIsXrForTesting(false);
        ImmersiveVideoPlaybackActivity activity = launchImmersiveVideoPlaybackActivity();
        verify(mNativeMock, never()).onActivityStart(any(), any(), any());
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(activity.isDestroyed(), Matchers.is(true)));
    }

    @Test
    @MediumTest
    public void testPendingStateDoesNotCrash() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ImmersiveVideoPlaybackActivity activity = new ImmersiveVideoPlaybackActivity();
                    activity.setPlaybackState(PlaybackState.PLAYING);
                    activity.updateVideoSize(VIDEO_WIDTH, VIDEO_HEIGHT);
                    activity.setImmersiveVideoOptions(STEREO_MODE, PROJECTION_TYPE);
                    activity.setMediaPosition(DURATION_MS, POSITION_MS, PLAYBACK_RATE);

                    ImmersiveVideoPlaybackActivity.PendingState pendingState =
                            activity.getPendingStateForTesting();
                    Assert.assertEquals(
                            "Playback state mismatch",
                            PlaybackState.PLAYING,
                            pendingState.mPlaybackState.intValue());
                    Assert.assertEquals(
                            "Video width mismatch",
                            VIDEO_WIDTH,
                            pendingState.mVideoWidth.intValue());
                    Assert.assertEquals(
                            "Video height mismatch",
                            VIDEO_HEIGHT,
                            pendingState.mVideoHeight.intValue());
                    Assert.assertEquals(
                            "Stereo mode mismatch",
                            STEREO_MODE,
                            pendingState.mStereoMode.intValue());
                    Assert.assertEquals(
                            "Projection type mismatch",
                            PROJECTION_TYPE,
                            pendingState.mProjectionType.intValue());
                    Assert.assertEquals(
                            "Duration mismatch", DURATION_MS, pendingState.mDurationMs.longValue());
                    Assert.assertEquals(
                            "Position mismatch", POSITION_MS, pendingState.mPositionMs.longValue());
                    Assert.assertEquals(
                            "Playback rate mismatch",
                            PLAYBACK_RATE,
                            pendingState.mPlaybackRate.floatValue(),
                            0.0f);
                });
    }

    /** Tests that setImmersiveVideoOptions correctly forwards calls to the mocked coordinator. */
    @Test
    @MediumTest
    public void testSetImmersiveVideoOptionsForwardsToCoordinatorMock() throws Throwable {
        ImmersiveVideoPlaybackActivity activity = startImmersiveVideoPlaybackActivity();

        activity.setImmersiveVideoOptions(STEREO_MODE, PROJECTION_TYPE);
        verify(mCoordinatorMock).updateVideoLayout(STEREO_MODE, PROJECTION_TYPE);

        testExitOn(activity, () -> activity.close());
    }

    /** Tests that setMediaPosition correctly forwards calls to the mocked coordinator. */
    @Test
    @MediumTest
    public void testSetMediaPositionForwardsToCoordinatorMock() throws Throwable {
        ImmersiveVideoPlaybackActivity activity = startImmersiveVideoPlaybackActivity();

        activity.setMediaPosition(DURATION_MS, POSITION_MS, PLAYBACK_RATE);
        verify(mCoordinatorMock).updateMediaPosition(DURATION_MS, POSITION_MS, PLAYBACK_RATE);

        testExitOn(activity, () -> activity.close());
    }

    /** Tests that setPlaybackState correctly forwards calls to the mocked coordinator. */
    @Test
    @MediumTest
    public void testSetPlaybackStateForwardsToCoordinatorMock() throws Throwable {
        ImmersiveVideoPlaybackActivity activity = startImmersiveVideoPlaybackActivity();

        activity.setPlaybackState(PlaybackState.PLAYING);
        verify(mCoordinatorMock).updatePlaybackState(true);

        activity.setPlaybackState(PlaybackState.PAUSED);
        verify(mCoordinatorMock).updatePlaybackState(false);

        testExitOn(activity, () -> activity.close());
    }

    /** Tests that updateVideoSize correctly forwards calls to the mocked coordinator. */
    @Test
    @MediumTest
    public void testUpdateVideoSizeForwardsToCoordinatorMock() throws Throwable {
        ImmersiveVideoPlaybackActivity activity = startImmersiveVideoPlaybackActivity();

        activity.updateVideoSize(VIDEO_WIDTH, VIDEO_HEIGHT);
        verify(mCoordinatorMock).updatePlayerSize(VIDEO_WIDTH, VIDEO_HEIGHT);

        testExitOn(activity, () -> activity.close());
    }

    private ImmersiveVideoPlaybackActivity launchImmersiveVideoPlaybackActivity() throws Exception {
        ImmersiveVideoPlaybackActivity activity =
                ActivityTestUtils.launchActivityWithTimeout(
                        InstrumentationRegistry.getInstrumentation(),
                        ImmersiveVideoPlaybackActivity.class,
                        new Callable<>() {
                            @Override
                            public Void call() throws TimeoutException {
                                ThreadUtils.runOnUiThreadBlocking(
                                        () -> {
                                            Context context = ContextUtils.getApplicationContext();
                                            var window = mTab.getWindowAndroid();
                                            if (window != null) {
                                                context = window.getActivity().get();
                                            }
                                            Intent intent =
                                                    new Intent(
                                                            context,
                                                            ImmersiveVideoPlaybackActivity.class);
                                            intent.putExtra(NATIVE_TOKEN_KEY, mNativeWindowToken);
                                            intent.putExtra(
                                                    WEB_CONTENTS_KEY, mTab.getWebContents());
                                            context.startActivity(intent);
                                        });
                                return null;
                            }
                        },
                        TIMEOUT_MILLISECONDS);

        return activity;
    }

    private ImmersiveVideoPlaybackActivity startImmersiveVideoPlaybackActivity() throws Exception {
        ImmersiveVideoPlaybackActivity activity = launchImmersiveVideoPlaybackActivity();
        ActivityTestUtils.waitForFirstLayout(activity);

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(activity.isNativeHandleInitialized(), Matchers.is(true));
                    Criteria.checkThat(
                            activity.isCoordinatorInitializedForTesting(), Matchers.is(true));
                },
                TIMEOUT_MILLISECONDS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        return activity;
    }

    private void testExitOn(Activity activity, Runnable runnable) throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(() -> runnable.run());

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity == null || activity.isDestroyed(), Matchers.is(true));
                },
                TIMEOUT_MILLISECONDS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }
}
