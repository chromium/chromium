// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prerender;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.graphics.Rect;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.Pair;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Callable;

/** Tests for {@link ExternalPrerenderHandler}.
 *
 *  NOTE: even though the tests refer to adding "Prerender", they actually exercise the default mode
 *  in the PrerenderManager. Currently it is {@code PRERENDER_MODE_NOSTATE_PREFETCH}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ExternalPrerenderHandlerTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String TEST_PAGE2 = "/chrome/test/data/android/about.html";

    private static final int PRERENDER_DELAY_MS = 500;
    private static final int ENSURE_COMPLETED_PRERENDER_RETRIES = 10;
    private static final int ENSURE_COMPLETED_PRERENDER_TIMEOUT_MS =
            ENSURE_COMPLETED_PRERENDER_RETRIES * PRERENDER_DELAY_MS;

    private ExternalPrerenderHandler mExternalPrerenderHandler;
    private Profile mProfile;
    private String mTestPage;
    private String mTestPage2;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() throws Exception {
        mExternalPrerenderHandler = new ExternalPrerenderHandler();

        final Callable<Profile> profileCallable = new Callable<Profile>() {
            @Override
            public Profile call() {
                return Profile.getLastUsedProfile();
            }
        };
        mProfile = TestThreadUtils.runOnUiThreadBlocking(profileCallable);

        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        mTestPage = mTestServer.getURL(TEST_PAGE);
        mTestPage2 = mTestServer.getURL(TEST_PAGE2);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mExternalPrerenderHandler.cancelCurrentPrerender());
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Prerender"})
    @SmallTest
    public void testAddPrerender() throws Exception {
        ensureStartedPrerenderForUrl(mTestPage);
        ensureCompletedPrefetchForUrl(mTestPage);
    }

    @Test
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Prerender"})
    @SmallTest
    public void testAddAndCancelPrerender() throws Exception {
        final WebContents webContents = ensureStartedPrerenderForUrl(mTestPage);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mExternalPrerenderHandler.cancelCurrentPrerender();
            Assert.assertFalse(
                    ExternalPrerenderHandler.hasPrerenderedUrl(mProfile, mTestPage, webContents));
        });
    }

    @Test
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @Feature({"Prerender"})
    @SmallTest
    public void testAddSeveralPrerenders() throws Exception {
        ensureStartedPrerenderForUrl(mTestPage);
        ensureCompletedPrefetchForUrl(mTestPage);
        ensureStartedPrerenderForUrl(mTestPage2);

        // Make sure that the second one didn't remove the first one.
        ensureCompletedPrefetchForUrl(mTestPage2);
    }

    private WebContents ensureStartedPrerenderForUrl(final String url) throws Exception {
        Callable<WebContents> addPrerenderCallable = new Callable<WebContents>() {
            @Override
            public WebContents call() {
                Pair<WebContents, WebContents> webContents =
                        mExternalPrerenderHandler.addPrerender(mProfile, url, "", new Rect(), true);

                Assert.assertNotNull(webContents);
                Assert.assertNotNull(webContents.first);
                Assert.assertNotNull(webContents.second);
                Assert.assertTrue(ExternalPrerenderHandler.hasPrerenderedUrl(
                        mProfile, url, webContents.first));
                return webContents.first;
            }
        };
        return TestThreadUtils.runOnUiThreadBlocking(addPrerenderCallable);
    }

    private void ensureCompletedPrefetchForUrl(final String url) {
        CriteriaHelper.pollUiThread(new Criteria("No Prefetch Happened") {
            @Override
            public boolean isSatisfied() {
                boolean has_prefetched =
                        ExternalPrerenderHandler.hasRecentlyPrefetchedUrlForTesting(mProfile, url);
                if (has_prefetched) {
                    ExternalPrerenderHandler.clearPrefetchInformationForTesting(mProfile);
                }
                return has_prefetched;
            }
        }, ENSURE_COMPLETED_PRERENDER_TIMEOUT_MS, PRERENDER_DELAY_MS);
    }
}
