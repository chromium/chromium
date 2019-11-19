// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.SadTabRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.mock.MockRenderFrameHost;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.ExecutionException;

/**
 * Tests (requiring native) of the ShareSheetMediator.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ShareSheetMediatorTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public final SadTabRule mSadTabRule = new SadTabRule();

    @Test
    @SmallTest
    public void testShouldFetchCanonicalUrl() throws ExecutionException {
        MockUrlTab mockTab =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return new MockUrlTab(); });
        MockWebContents mockWebContents = new MockWebContents();
        MockRenderFrameHost mockRenderFrameHost = new MockRenderFrameHost();
        mSadTabRule.setTab(mockTab);

        // Null webContents:
        Assert.assertFalse(ShareSheetMediator.shouldFetchCanonicalUrl(mockTab));

        // Null render frame:
        mockTab.webContents = mockWebContents;
        Assert.assertFalse(ShareSheetMediator.shouldFetchCanonicalUrl(mockTab));

        // Invalid/empty URL:
        mockWebContents.renderFrameHost = mockRenderFrameHost;
        Assert.assertFalse(ShareSheetMediator.shouldFetchCanonicalUrl(mockTab));

        // Disabled if showing error page.
        mockTab.isShowingErrorPage = true;
        Assert.assertFalse(ShareSheetMediator.shouldFetchCanonicalUrl(mockTab));
        mockTab.isShowingErrorPage = false;

        // Disabled if showing interstitial page.
        mockTab.isShowingInterstitialPage = true;
        Assert.assertFalse(ShareSheetMediator.shouldFetchCanonicalUrl(mockTab));
        mockTab.isShowingInterstitialPage = false;

        // Disabled if showing sad tab page.
        mSadTabRule.show(true);
        Assert.assertFalse(ShareSheetMediator.shouldFetchCanonicalUrl(mockTab));
        mSadTabRule.show(false);
    }

    @Test
    @SmallTest
    public void testGetUrlToShare() {
        Assert.assertNull(ShareSheetMediator.getUrlToShare(null, null));
        Assert.assertEquals("", ShareSheetMediator.getUrlToShare("", null));

        final String httpUrl = "http://blah.com";
        final String otherHttpUrl = "http://eek.com";
        final String httpsUrl = "https://blah.com";
        final String otherHttpsUrl = "https://eek.com";
        final String ftpUrl = "ftp://blah.com";
        final String contentUrl = "content://eek.com";

        // HTTP Display URL, HTTP Canonical URL -> HTTP Display URL
        Assert.assertEquals(httpUrl, ShareSheetMediator.getUrlToShare(httpUrl, otherHttpUrl));
        // HTTP Display URL, HTTPS Canonical URL -> HTTP Display URL
        Assert.assertEquals(httpUrl, ShareSheetMediator.getUrlToShare(httpUrl, httpsUrl));
        // HTTPS Display URL, HTTP Canonical URL -> HTTP Canonical URL
        Assert.assertEquals(httpUrl, ShareSheetMediator.getUrlToShare(httpsUrl, httpUrl));

        // HTTPS Display URL, HTTPS Canonical URL -> HTTPS Canonical URL
        Assert.assertEquals(httpsUrl, ShareSheetMediator.getUrlToShare(httpsUrl, httpsUrl));
        Assert.assertEquals(
                otherHttpsUrl, ShareSheetMediator.getUrlToShare(httpsUrl, otherHttpsUrl));

        // HTTPS Display URL, FTP URL -> HTTPS Display URL
        Assert.assertEquals(httpsUrl, ShareSheetMediator.getUrlToShare(httpsUrl, ftpUrl));
        // HTTPS Display URL, Content URL -> HTTPS Display URL
        Assert.assertEquals(httpsUrl, ShareSheetMediator.getUrlToShare(httpsUrl, contentUrl));
    }

    private static class MockUrlTab extends MockTab {
        public WebContents webContents;
        public String url;
        public boolean isShowingErrorPage;
        public boolean isShowingInterstitialPage;

        public MockUrlTab() {
            super(INVALID_TAB_ID, false);
        }

        @Override
        public String getUrl() {
            return url;
        }

        @Override
        public WebContents getWebContents() {
            return webContents;
        }

        @Override
        public boolean isShowingErrorPage() {
            return isShowingErrorPage;
        }

        @Override
        public boolean isShowingInterstitialPage() {
            return isShowingInterstitialPage;
        }
    }
}
