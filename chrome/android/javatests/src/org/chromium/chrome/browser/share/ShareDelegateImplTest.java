// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.SadTabRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.mock.MockRenderFrameHost;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.url.GURL;

import java.util.concurrent.ExecutionException;

/** Tests (requiring native) of the ShareDelegateImpl. */
@Batch(Batch.PER_CLASS)
@RunWith(BaseJUnit4ClassRunner.class)
public class ShareDelegateImplTest {
    @ClassRule
    public static final ChromeBrowserTestRule sBrowserTestRule = new ChromeBrowserTestRule();

    @Rule public final SadTabRule mSadTabRule = new SadTabRule();

    @Test
    @SmallTest
    public void testShouldFetchCanonicalUrl() throws ExecutionException {
        MockUrlTab mockTab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new MockUrlTab();
                        });
        MockWebContents mockWebContents = new MockWebContents();
        MockRenderFrameHost mockRenderFrameHost = new MockRenderFrameHost();
        mSadTabRule.setTab(mockTab);

        // Null webContents:
        Assert.assertFalse(ShareDelegateImpl.shouldFetchCanonicalUrl(mockTab));

        // Null render frame:
        mockTab.webContents = mockWebContents;
        Assert.assertFalse(ShareDelegateImpl.shouldFetchCanonicalUrl(mockTab));

        // Invalid/empty URL:
        mockWebContents.renderFrameHost = mockRenderFrameHost;
        Assert.assertFalse(ShareDelegateImpl.shouldFetchCanonicalUrl(mockTab));

        // Disabled if showing error page.
        mockTab.isShowingErrorPage = true;
        Assert.assertFalse(ShareDelegateImpl.shouldFetchCanonicalUrl(mockTab));
        mockTab.isShowingErrorPage = false;

        // Disabled if showing sad tab page.
        mSadTabRule.show(true);
        Assert.assertFalse(ShareDelegateImpl.shouldFetchCanonicalUrl(mockTab));
        mSadTabRule.show(false);
    }

    @Test
    @SmallTest
    public void testGetUrlToShare() {
        Assert.assertEquals("", ShareDelegateImpl.getUrlToShare(GURL.emptyGURL(), null));

        final GURL httpUrl = new GURL("http://blah.com");
        final GURL otherHttpUrl = new GURL("http://eek.com");
        final GURL httpsUrl = new GURL("https://blah.com");
        final GURL otherHttpsUrl = new GURL("https://eek.com");
        final GURL ftpUrl = new GURL("ftp://blah.com");
        final GURL contentUrl = new GURL("content://eek.com");

        // HTTP Display URL, HTTP Canonical URL -> HTTP Display URL
        Assert.assertEquals(
                httpUrl.getSpec(), ShareDelegateImpl.getUrlToShare(httpUrl, otherHttpUrl));
        // HTTP Display URL, HTTPS Canonical URL -> HTTP Display URL
        Assert.assertEquals(httpUrl.getSpec(), ShareDelegateImpl.getUrlToShare(httpUrl, httpsUrl));
        // HTTPS Display URL, HTTP Canonical URL -> HTTP Canonical URL
        Assert.assertEquals(httpUrl.getSpec(), ShareDelegateImpl.getUrlToShare(httpsUrl, httpUrl));

        // HTTPS Display URL, HTTPS Canonical URL -> HTTPS Canonical URL
        Assert.assertEquals(
                httpsUrl.getSpec(), ShareDelegateImpl.getUrlToShare(httpsUrl, httpsUrl));
        Assert.assertEquals(
                otherHttpsUrl.getSpec(), ShareDelegateImpl.getUrlToShare(httpsUrl, otherHttpsUrl));

        // HTTPS Display URL, FTP URL -> HTTPS Display URL
        Assert.assertEquals(httpsUrl.getSpec(), ShareDelegateImpl.getUrlToShare(httpsUrl, ftpUrl));
        // HTTPS Display URL, Content URL -> HTTPS Display URL
        Assert.assertEquals(
                httpsUrl.getSpec(), ShareDelegateImpl.getUrlToShare(httpsUrl, contentUrl));
    }

    private static class MockUrlTab extends MockTab {
        public WebContents webContents;
        public boolean isShowingErrorPage;

        public MockUrlTab() {
            super(INVALID_TAB_ID, ProfileManager.getLastUsedRegularProfile());
        }

        @Override
        public GURL getUrl() {
            return GURL.emptyGURL();
        }

        @Override
        public WebContents getWebContents() {
            return webContents;
        }

        @Override
        public boolean isShowingErrorPage() {
            return isShowingErrorPage;
        }
    }
}
