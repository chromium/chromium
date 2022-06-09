// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.util.Rational;

import androidx.annotation.RequiresApi;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.WebContentsUtils;

import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;

/**
 * Tests for PictureInPictureActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RequiresApi(Build.VERSION_CODES.O)
public class PictureInPictureActivityTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public JniMocker mMocker = new JniMocker();

    private static final long NATIVE_OVERLAY = 100L;
    private static final long PIP_TIMEOUT_MILLISECONDS = 10000L;

    @Mock
    private PictureInPictureActivity.Natives mNativeMock;

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
        startPictureInPictureActivity();

        // The LaunchIntoPipHelper should have been called.
        Assert.assertTrue(mBounds != null);
        Criteria.checkThat(mBounds, Matchers.is(mSourceRectHint));
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
    @MinAndroidSdkLevel(Build.VERSION_CODES.O)
    public void testMakeEnterPictureInPictureWithBadSourceRect() throws Throwable {
        mSourceRectHint.left = -1;
        startPictureInPictureActivity();
        // The pip helper should not be called with trivially bad bounds.
        Assert.assertTrue(mBounds == null);
    }

    private WebContents getWebContents() {
        return mActivityTestRule.getActivity().getCurrentWebContents();
    }

    private void testExitOn(Activity activity, Runnable runnable) throws Throwable {
        runnable.run();

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(activity == null || activity.isDestroyed(), Matchers.is(true));
        }, PIP_TIMEOUT_MILLISECONDS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    private PictureInPictureActivity startPictureInPictureActivity() throws Exception {
        PictureInPictureActivity activity =
                ActivityTestUtils.waitForActivity(InstrumentationRegistry.getInstrumentation(),
                        PictureInPictureActivity.class, new Callable<Void>() {
                            @Override
                            public Void call() throws TimeoutException {
                                TestThreadUtils.runOnUiThreadBlocking(
                                        ()
                                                -> PictureInPictureActivity.createActivity(
                                                        NATIVE_OVERLAY, mTab, mSourceRectHint.left,
                                                        mSourceRectHint.top,
                                                        mSourceRectHint.width(),
                                                        mSourceRectHint.height()));
                                return null;
                            }
                        });

        verify(mNativeMock, times(1)).onActivityStart(eq(NATIVE_OVERLAY), eq(activity), any());

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(activity.isInPictureInPictureMode(), Matchers.is(true));
        }, PIP_TIMEOUT_MILLISECONDS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        Rational ratio = activity.getAspectRatio();
        Criteria.checkThat(ratio,
                Matchers.is(new Rational(mSourceRectHint.width(), mSourceRectHint.height())));

        return activity;
    }
}
