// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.prerender;

import android.graphics.Rect;

import org.junit.Assert;

import org.chromium.chrome.browser.TabLoadStatus;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.Callable;

/**
 * Utility class for common methods to test prerendering.
 */
public class PrerenderTestHelper {
    private static final int UI_DELAY_MS = 100;
    private static final int WAIT_FOR_RESPONSE_MS = 10000;
    private static final int SHORT_TIMEOUT_MS = 200;

    private static boolean hasTabPrerenderedUrl(final Tab tab, final String url) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return nativeHasPrerenderedUrl(tab.getWebContents(), url);
            }
        });
    }

    /**
     * Waits for the specified URL to be prerendered.
     * Returns true if it succeeded, false if it was not prerendered before the timeout.
     * shortTimeout should be set to true when expecting this function to return false, as to
     * make the tests run faster.
     */
    public static boolean waitForPrerenderUrl(
            final Tab tab, final String url, boolean shortTimeout) {
        try {
            CriteriaHelper.pollInstrumentationThread(new Criteria() {
                @Override
                public boolean isSatisfied() {
                    return hasTabPrerenderedUrl(tab, url);
                }
            }, shortTimeout ? SHORT_TIMEOUT_MS : WAIT_FOR_RESPONSE_MS, UI_DELAY_MS);
        } catch (AssertionError e) {
            // TODO(tedchoc): This is horrible and should never timeout to determine success.
        }

        return hasTabPrerenderedUrl(tab, url);
    }

    /**
     * Prerenders a url.
     *
     * @param testUrl Url to prerender
     * @param tab The tab to add the prerender to.
     */
    public static ExternalPrerenderHandler prerenderUrl(final String testUrl, Tab tab) {
        final Tab currentTab = tab;
        final Coordinates coord = Coordinates.createFor(currentTab.getWebContents());
        ExternalPrerenderHandler prerenderHandler =
                TestThreadUtils.runOnUiThreadBlockingNoException(
                        new Callable<ExternalPrerenderHandler>() {
                            @Override
                            public ExternalPrerenderHandler call() {
                                ExternalPrerenderHandler prerenderHandler =
                                        new ExternalPrerenderHandler();
                                Rect bounds = new Rect(0, 0, coord.getContentWidthPixInt(),
                                        coord.getContentHeightPixInt());
                                boolean didPrerender =
                                        prerenderHandler.addPrerender(currentTab.getProfile(),
                                                currentTab.getWebContents(), testUrl, null, bounds,
                                                true)
                                        != null;
                                Assert.assertTrue(
                                        "Failed to prerender test url: " + testUrl, didPrerender);
                                return prerenderHandler;
                            }
                        });

        Assert.assertTrue("URL was not prerendered.",
                PrerenderTestHelper.waitForPrerenderUrl(currentTab, testUrl, false));

        return prerenderHandler;
    }

    /**
     * Checks if the load url result is prerendered.
     *
     * @param result Result from a page load.
     * @return Whether the result param indicates a prerendered url.
     */
    public static boolean isLoadUrlResultPrerendered(int result) {
        return result == TabLoadStatus.FULL_PRERENDERED_PAGE_LOAD
                || result == TabLoadStatus.PARTIAL_PRERENDERED_PAGE_LOAD;
    }

    private static native boolean nativeHasPrerenderedUrl(WebContents webContents, String url);
}
