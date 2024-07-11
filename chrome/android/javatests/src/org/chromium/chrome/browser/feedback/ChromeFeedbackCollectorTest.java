// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.feedback.ChromeFeedbackCollector.InitParams;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

import java.util.List;

@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ChromeFeedbackCollectorTest {
    private static final String FEEDBACK_URL = "https://google.com";
    private static final String FEEDBACK_CONSTANT = "feedbackContext";
    @Rule public ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    @Mock Activity mActivity;

    ChromeFeedbackCollector mCollector;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    @SmallTest
    @Feature({"Feedback"})
    public void testRegularProfile() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    InitParams params = new InitParams(profile, FEEDBACK_URL, FEEDBACK_CONSTANT);
                    mCollector =
                            new ChromeFeedbackCollector(
                                    mActivity, null, null, null, params, null, profile);
                });

        Assert.assertTrue(
                "FamilyInfoFeedbackSource should be present.", containsFamilyFeedbackSource());
    }

    @Test
    @SmallTest
    @Feature({"Feedback"})
    public void testIncognitoProfile() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile =
                            ProfileManager.getLastUsedRegularProfile().getPrimaryOTRProfile(true);
                    InitParams params = new InitParams(profile, FEEDBACK_URL, FEEDBACK_CONSTANT);
                    mCollector =
                            new ChromeFeedbackCollector(
                                    mActivity, null, null, null, params, null, profile);
                });

        // FamilyInfoFeedbackSource relies on IdentityManager which is not available for the
        // incognito profile. See https://crbug.com/1340320.
        Assert.assertFalse(
                "FamilyInfoFeedbackSource should not be present.", containsFamilyFeedbackSource());
    }

    private boolean containsFamilyFeedbackSource() {
        List<AsyncFeedbackSource> asyncSources = mCollector.getAsyncFeedbackSourcesForTesting();
        for (AsyncFeedbackSource source : asyncSources) {
            if (source instanceof FamilyInfoFeedbackSource) return true;
        }
        return false;
    }
}
