// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

import java.util.Arrays;
import java.util.List;

/** Tests which activity types enable unmuted autoplay. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class CustomTabAutoplayUnmutedTest {
    @ClassParameter
    public static List<ParameterSet> sClassParams =
            Arrays.asList(
                    new ParameterSet().value(ActivityType.WEBAPP).name("Webapp"),
                    new ParameterSet().value(ActivityType.WEB_APK).name("WebApk"),
                    new ParameterSet()
                            .value(ActivityType.TRUSTED_WEB_ACTIVITY)
                            .name("TrustedWebActivity"));

    private @ActivityType int mActivityType;

    @Rule public final ChromeActivityTestRule<?> mActivityTestRule;

    public CustomTabAutoplayUnmutedTest(@ActivityType int activityType) {
        mActivityType = activityType;
        mActivityTestRule = CustomTabActivityTypeTestUtils.createActivityTestRule(activityType);
    }

    /** Test that unmuted autoplay is only enabled for WebAPKs. */
    @Test
    @LargeTest
    public void testAutoplayUnmuted() throws Exception {
        String testPageUrl =
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/media/video-autoplay.html");
        CustomTabActivityTypeTestUtils.launchActivity(
                mActivityType, mActivityTestRule, testPageUrl);
        WebContents webContents = mActivityTestRule.getActivity().getActivityTab().getWebContents();
        String result = JavaScriptUtils.runJavascriptWithAsyncResult(webContents, "tryPlayback()");
        boolean expectUnmutedAutoplay =
                (mActivityType == ActivityType.WEB_APK
                        || mActivityType == ActivityType.TRUSTED_WEB_ACTIVITY);
        assertEquals(expectUnmutedAutoplay, result.equals("true"));
    }
}
