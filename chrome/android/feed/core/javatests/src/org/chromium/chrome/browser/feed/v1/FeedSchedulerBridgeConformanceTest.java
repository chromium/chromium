// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.chrome.browser.feed.library.api.client.requestmanager.RequestManager;
import org.chromium.chrome.browser.feed.library.testing.conformance.scheduler.SchedulerConformanceTest;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.Arrays;
import java.util.List;

/**
 * Conformance Tests for {@link FeedSchedulerBridge}.
 * The actual tests are implemented in SchedulerConformanceTest.
 */

// The @SmallTest class annotation is needed to allow the inherited @Test methods to run using
// build/android/test_runner.py.
@SmallTest
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@DisableFeatures({ChromeFeatureList.INTEREST_FEED_V2})
public final class FeedSchedulerBridgeConformanceTest extends SchedulerConformanceTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(false).name("withoutRequestManager"),
                    new ParameterSet().value(true).name("withRequestManager"));

    @Rule
    public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Mock
    private RequestManager mRequestManager;
    private boolean mUseRequestManager;

    public FeedSchedulerBridgeConformanceTest(boolean useRequestManager) {
        mUseRequestManager = useRequestManager;
    }

    @Before
    public void setUp() {
        // The scheduler is declared and tested in SchedulerConformanceTest.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mScheduler = new FeedSchedulerBridge(Profile.getLastUsedRegularProfile());
            if (mUseRequestManager) {
                ((FeedSchedulerBridge) mScheduler).initializeFeedDependencies(mRequestManager);
            }
        });
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> ((FeedSchedulerBridge) mScheduler).destroy());
        mScheduler = null;
    }
}
