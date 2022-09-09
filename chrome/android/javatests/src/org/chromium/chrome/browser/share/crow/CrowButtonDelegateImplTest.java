// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.crow;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.concurrent.ExecutionException;

/**
 * Tests of the CrowButtonDelegateImpl.
 * Requires native for GURL and canonicalization tests.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class CrowButtonDelegateImplTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    private CrowButtonDelegateImpl mCrowButtonDelegate;

    @Before
    public void setUp() throws ExecutionException {
        mCrowButtonDelegate = TestThreadUtils.runOnUiThreadBlocking(CrowButtonDelegateImpl::new);
    }

    @Test
    @SmallTest
    public void testBuildServerUrl() {
        final GURL serverUrl = new GURL("https://www.foo.com/v1/api?q=hi");
        // No query string, http rather than https.
        final GURL serverUrlWithoutQueryString = new GURL("http://www.foo.com/v1/api");
        final GURL shareUrl1 = new GURL("https://testSiteWeAreSharing.com/blog/entry");
        final GURL shareUrl2 = new GURL("https://testSiteWeAreSharing.com/?blog=1&entry=2");
        boolean allowMetrics = true;
        boolean isFollowing = false;

        assertEquals("",
                mCrowButtonDelegate.buildServerUrlInternal(
                        GURL.emptyGURL(), shareUrl1, shareUrl1, "", allowMetrics, isFollowing));

        // Baseline/common case.
        assertEquals(
                "https://www.foo.com/v1/api?q=hi&pageUrl=https%3A%2F%2Ftestsitewearesharing.com%2Fblog%2Fentry&entry=menu&relCanonUrl=https%3A%2F%2Ftestsitewearesharing.com%2Fblog%2Fentry&publicationId=pubId1&metrics=true",
                mCrowButtonDelegate.buildServerUrlInternal(
                        serverUrl, shareUrl1, shareUrl1, "pubId1", allowMetrics, isFollowing));

        // Sending a URL with urlparams of its own.
        assertEquals(
                "https://www.foo.com/v1/api?q=hi&pageUrl=https%3A%2F%2Ftestsitewearesharing.com%2F%3Fblog%3D1%26entry%3D2&entry=menu&relCanonUrl=https%3A%2F%2Ftestsitewearesharing.com%2F%3Fblog%3D1%26entry%3D2&publicationId=pubId2&metrics=true",
                mCrowButtonDelegate.buildServerUrlInternal(
                        serverUrl, shareUrl2, shareUrl2, "pubId2", allowMetrics, isFollowing));

        // Empty canonical URL is ok, passes as empty param.
        assertEquals(
                "https://www.foo.com/v1/api?q=hi&pageUrl=https%3A%2F%2Ftestsitewearesharing.com%2Fblog%2Fentry&entry=menu&relCanonUrl=&publicationId=pubId1&metrics=true",
                mCrowButtonDelegate.buildServerUrlInternal(serverUrl, shareUrl1, GURL.emptyGURL(),
                        "pubId1", allowMetrics, isFollowing));

        // Experimental URL can be passed with an empty set of params.
        assertEquals(
                "http://www.foo.com/v1/api?pageUrl=https%3A%2F%2Ftestsitewearesharing.com%2Fblog%2Fentry&entry=menu&relCanonUrl=https%3A%2F%2Ftestsitewearesharing.com%2Fblog%2Fentry&publicationId=pubId1&metrics=true",
                mCrowButtonDelegate.buildServerUrlInternal(serverUrlWithoutQueryString, shareUrl1,
                        shareUrl1, "pubId1", allowMetrics, isFollowing));

        // Metrics off and already following should be reflected.
        allowMetrics = false;
        isFollowing = true;
        assertEquals(
                "https://www.foo.com/v1/api?q=hi&pageUrl=https%3A%2F%2Ftestsitewearesharing.com%2Fblog%2Fentry&entry=menu&relCanonUrl=https%3A%2F%2Ftestsitewearesharing.com%2Fblog%2Fentry&publicationId=pubId1&metrics=false&following=true",
                mCrowButtonDelegate.buildServerUrlInternal(
                        serverUrl, shareUrl1, shareUrl1, "pubId1", allowMetrics, isFollowing));
    }

    @Test
    @SmallTest
    public void testParseDomainMap() {
        HashMap<String, String> map = mCrowButtonDelegate.parseDomainIdMap(
                "foo1.com^pubId1;fooStaging.com^pubId2;news.foo.com^pubId3");
        assertEquals(3, map.size());
        assertEquals("pubId1", map.get("foo1.com"));
        assertEquals("pubId2", map.get("fooStaging.com"));
        assertEquals("pubId3", map.get("news.foo.com"));
    }
}
