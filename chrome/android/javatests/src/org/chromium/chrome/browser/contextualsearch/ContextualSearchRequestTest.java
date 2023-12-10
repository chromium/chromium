// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;

/**
 * Class responsible for testing the ContextualSearchRequest. TODO(donnd): Switch to a pure-java
 * test.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ContextualSearchRequestTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    ContextualSearchRequest mRequest;
    ContextualSearchRequest mNormalPriorityOnlyRequest;

    @Before
    public void setUp() throws Exception {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            mRequest =
                                    new ContextualSearchRequest(
                                            sActivityTestRule.getProfile(false),
                                            "barack obama",
                                            "barack",
                                            "",
                                            true,
                                            null,
                                            null);
                            mNormalPriorityOnlyRequest =
                                    new ContextualSearchRequest(
                                            sActivityTestRule.getProfile(false),
                                            "woody allen",
                                            "allen",
                                            "",
                                            false,
                                            null,
                                            null);
                        });
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testIsUsingLowPriority() {
        Assert.assertTrue(mRequest.isUsingLowPriority());
        Assert.assertFalse(mNormalPriorityOnlyRequest.isUsingLowPriority());
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testHasFailed() {
        Assert.assertFalse(mRequest.getHasFailed());
        mRequest.setHasFailed();
        Assert.assertTrue(mRequest.getHasFailed());
        Assert.assertFalse(mNormalPriorityOnlyRequest.getHasFailed());
        mNormalPriorityOnlyRequest.setHasFailed();
        Assert.assertTrue(mNormalPriorityOnlyRequest.getHasFailed());
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testSetNormalPriority() {
        Assert.assertTrue(mRequest.isUsingLowPriority());
        mRequest.setNormalPriority();
        Assert.assertFalse(mRequest.isUsingLowPriority());
        Assert.assertFalse(mNormalPriorityOnlyRequest.isUsingLowPriority());
    }

    @Test
    @SmallTest
    @Feature({"ContextualSearch"})
    public void testServerProvidedUrls() {
        String serverUrlFull = "https://www.google.com/search?obama&ctxs=2";
        String serverUrlPreload = "https://www.google.com/s?obama&ctxs=2&pf=c&sns=1";
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            mRequest =
                                    new ContextualSearchRequest(
                                            sActivityTestRule.getProfile(false),
                                            "",
                                            "",
                                            "",
                                            true,
                                            serverUrlFull,
                                            serverUrlPreload);
                            mNormalPriorityOnlyRequest =
                                    new ContextualSearchRequest(
                                            sActivityTestRule.getProfile(false),
                                            "",
                                            "",
                                            "",
                                            false,
                                            serverUrlFull,
                                            null);
                        });
        Assert.assertTrue(mRequest.isUsingLowPriority());
        Assert.assertEquals(serverUrlPreload, mRequest.getSearchUrl());
        mRequest.setNormalPriority();
        Assert.assertFalse(mRequest.isUsingLowPriority());
        Assert.assertFalse(mNormalPriorityOnlyRequest.isUsingLowPriority());
        Assert.assertEquals(serverUrlFull, mRequest.getSearchUrl());
        Assert.assertEquals(serverUrlFull, mNormalPriorityOnlyRequest.getSearchUrl());
    }
}
