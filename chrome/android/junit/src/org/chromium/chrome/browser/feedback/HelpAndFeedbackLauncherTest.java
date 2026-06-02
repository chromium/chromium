// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;

/** Tests for {@link HelpAndFeedbackLauncher}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HelpAndFeedbackLauncherTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private FeedbackPolicyManager mFeedbackPolicyManager;

    @Before
    public void setUp() {
        FeedbackPolicyManager.setInstanceForTesting(mFeedbackPolicyManager);
    }

    @After
    public void tearDown() {
        FeedbackPolicyManager.setInstanceForTesting(null);
    }

    @Test
    public void testGetHelpMenuStringRes_FeedbackAllowed() {
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(true);
        Assert.assertEquals(R.string.menu_help, HelpAndFeedbackLauncher.getHelpMenuStringRes());
    }

    @Test
    public void testGetHelpMenuStringRes_FeedbackDisallowed() {
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(false);
        Assert.assertEquals(
                R.string.menu_help_no_feedback, HelpAndFeedbackLauncher.getHelpMenuStringRes());
    }
}
