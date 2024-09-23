// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.engagement;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.site_engagement.SiteEngagementService;
import org.chromium.content_public.browser.BrowserContextHandle;

/** Test for the Site Engagement Service Java binding. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SiteEngagementServiceTest {
    private static final String URL = "https://www.example.com";

    @Rule public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    /** Verify that setting the engagement score for a URL and reading it back it works. */
    @Test
    @SmallTest
    @Feature({"Engagement"})
    public void testSettingAndRetrievingScore() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowserContextHandle handle = ProfileManager.getLastUsedRegularProfile();
                    SiteEngagementService service =
                            SiteEngagementService.getForBrowserContext(handle);

                    Assert.assertEquals(0.0, service.getScore(URL), 0);
                    service.resetBaseScoreForUrl(URL, 5.0);
                    Assert.assertEquals(5.0, service.getScore(URL), 0);

                    service.resetBaseScoreForUrl(URL, 2.0);
                    Assert.assertEquals(2.0, service.getScore(URL), 0);
                });
    }

    /** Verify that repeatedly fetching and throwing away the SiteEngagementService works. */
    @Test
    @SmallTest
    @Feature({"Engagement"})
    public void testRepeatedlyGettingService() throws Throwable {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();

                    Assert.assertEquals(
                            0.0,
                            SiteEngagementService.getForBrowserContext(profile).getScore(URL),
                            0);
                    SiteEngagementService.getForBrowserContext(profile)
                            .resetBaseScoreForUrl(URL, 5.0);
                    Assert.assertEquals(
                            5.0,
                            SiteEngagementService.getForBrowserContext(profile).getScore(URL),
                            0);

                    SiteEngagementService.getForBrowserContext(profile)
                            .resetBaseScoreForUrl(URL, 2.0);
                    Assert.assertEquals(
                            2.0,
                            SiteEngagementService.getForBrowserContext(profile).getScore(URL),
                            0);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowserContextHandle handle = ProfileManager.getLastUsedRegularProfile();
                    SiteEngagementService service =
                            SiteEngagementService.getForBrowserContext(handle);
                    service.resetBaseScoreForUrl(URL, 0.0);
                });
    }
}
