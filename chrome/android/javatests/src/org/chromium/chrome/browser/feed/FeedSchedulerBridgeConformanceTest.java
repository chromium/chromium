// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.testing.conformance.scheduler.SchedulerConformanceTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.rules.RuleChain;
import org.junit.runner.Description;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

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
public final class FeedSchedulerBridgeConformanceTest extends SchedulerConformanceTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            Arrays.asList(new ParameterSet().value(false).name("withoutRequestManager"),
                    new ParameterSet().value(true).name("withRequestManager"));

    @Rule
    public RuleChain mChain =
            RuleChain.outerRule(new ChromeBrowserTestRule()).around(new UiThreadTestRule() {
                @Override
                protected boolean shouldRunOnUiThread(Description description) {
                    // FeedSchedulerBridge requires the used methods to be called on the UI Thread.
                    return true;
                }
            });

    @Mock
    private RequestManager mRequestManager;
    private boolean mUseRequestManager;

    public FeedSchedulerBridgeConformanceTest(boolean useRequestManager) {
        mUseRequestManager = useRequestManager;
    }

    @Before
    public void setUp() {
        // The scheduler is declared and tested in SchedulerConformanceTest.
        scheduler = new FeedSchedulerBridge(Profile.getLastUsedProfile());
        if (mUseRequestManager) {
            ((FeedSchedulerBridge) scheduler).initializeFeedDependencies(mRequestManager);
        }
    }

    @After
    public void tearDown() {
        ((FeedSchedulerBridge) scheduler).destroy();
        scheduler = null;
    }
}
