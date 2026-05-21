// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.concurrent.TimeUnit;

/** Tests for {@link ActorTaskTimeoutParameters}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT)
public class ActorTaskTimeoutParametersTest {
    private static final int THIRTY_MIN_MS = (int) TimeUnit.MINUTES.toMillis(30);
    private static final int FIVE_MIN_MS = (int) TimeUnit.MINUTES.toMillis(5);
    private static final int ONE_MIN_MS = (int) TimeUnit.MINUTES.toMillis(1);

    @Test
    public void testDefaultValues() {
        assertEquals(THIRTY_MIN_MS, ActorTaskTimeoutParameters.getRunningTimeoutMs());
        assertEquals(FIVE_MIN_MS, ActorTaskTimeoutParameters.getPausedTimeoutMs());
        assertEquals(FIVE_MIN_MS, ActorTaskTimeoutParameters.getNeedsUserInputTimeoutMs());
        assertEquals(FIVE_MIN_MS, ActorTaskTimeoutParameters.getWarningTimeoutMs());
    }

    @Test
    public void testFinchOverrides() {
        FeatureOverrides.newBuilder()
                .param(
                        ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT,
                        "running_timeout_ms",
                        ONE_MIN_MS)
                .param(
                        ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT,
                        "paused_timeout_ms",
                        ONE_MIN_MS)
                .param(
                        ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT,
                        "needs_user_input_timeout_ms",
                        ONE_MIN_MS)
                .param(
                        ChromeFeatureList.ANDROID_ACTOR_TASK_TIMEOUT,
                        "warning_timeout_ms",
                        ONE_MIN_MS)
                .apply();

        assertEquals(ONE_MIN_MS, ActorTaskTimeoutParameters.getRunningTimeoutMs());
        assertEquals(ONE_MIN_MS, ActorTaskTimeoutParameters.getPausedTimeoutMs());
        assertEquals(ONE_MIN_MS, ActorTaskTimeoutParameters.getNeedsUserInputTimeoutMs());
        assertEquals(ONE_MIN_MS, ActorTaskTimeoutParameters.getWarningTimeoutMs());
    }
}
