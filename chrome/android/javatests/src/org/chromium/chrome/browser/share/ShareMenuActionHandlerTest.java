// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.util.SadTabRule;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.mock.MockRenderFrameHost;
import org.chromium.content_public.browser.test.mock.MockWebContents;

import java.util.concurrent.ExecutionException;

/**
 * Tests (requiring native) of the ShareMenuActionHandler.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ShareMenuActionHandlerTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public final SadTabRule mSadTabRule = new SadTabRule();

    @Test
    @SmallTest
    public void testShouldFetchCanonicalUrl() throws ExecutionException {
        MockTab mockTab = ThreadUtils.runOnUiThreadBlocking(() -> { return new MockTab(); });
        MockWebContents mockWebContents = new MockWebContents();
        MockRenderFrameHost mockRenderFrameHost = new MockRenderFrameHost();
        mSadTabRule.setTab(mockTab);

        // Null webContents:
        Assert.assertFalse(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));

        // Null render frame:
        mockTab.webContents = mockWebContents;
        Assert.assertFalse(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));

        // Invalid/empty URL:
        mockWebContents.renderFrameHost = mockRenderFrameHost;
        Assert.assertFalse(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));

        // Disabled if showing error page.
        mockTab.isShowingErrorPage = true;
        Assert.assertFalse(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));
        mockTab.isShowingErrorPage = false;

        // Disabled if showing interstitial page.
        mockTab.isShowingInterstitialPage = true;
        Assert.assertFalse(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));
        mockTab.isShowingInterstitialPage = false;

        // Disabled if showing sad tab page.
        mSadTabRule.show(true);
        Assert.assertFalse(ShareMenuActionHandler.shouldFetchCanonicalUrl(mockTab));
        mSadTabRule.show(false);
    }

    @Test
    @SmallTest
    public void testGetUrlToShare() {
        Assert.assertNull(ShareMenuActionHandler.getUrlToShare(null, null));
        Assert.assertEquals("", ShareMenuActionHandler.getUrlToShare("", null));

        final String httpUrl = "http://blah.com";
        final String otherHttpUrl = "http://eek.com";
        final String httpsUrl = "https://blah.com";
        final String otherHttpsUrl = "https://eek.com";
        final String ftpUrl = "ftp://blah.com";
        final String contentUrl = "content://eek.com";

        // HTTP Display URL, HTTP Canonical URL -> HTTP Display URL
        Assert.assertEquals(httpUrl, ShareMenuActionHandler.getUrlToShare(httpUrl, otherHttpUrl));
        // HTTP Display URL, HTTPS Canonical URL -> HTTP Display URL
        Assert.assertEquals(httpUrl, ShareMenuActionHandler.getUrlToShare(httpUrl, httpsUrl));
        // HTTPS Display URL, HTTP Canonical URL -> HTTP Canonical URL
        Assert.assertEquals(httpUrl, ShareMenuActionHandler.getUrlToShare(httpsUrl, httpUrl));

        // HTTPS Display URL, HTTPS Canonical URL -> HTTPS Canonical URL
        Assert.assertEquals(httpsUrl, ShareMenuActionHandler.getUrlToShare(httpsUrl, httpsUrl));
        Assert.assertEquals(
                otherHttpsUrl, ShareMenuActionHandler.getUrlToShare(httpsUrl, otherHttpsUrl));

        // HTTPS Display URL, FTP URL -> HTTPS Display URL
        Assert.assertEquals(httpsUrl, ShareMenuActionHandler.getUrlToShare(httpsUrl, ftpUrl));
        // HTTPS Display URL, Content URL -> HTTPS Display URL
        Assert.assertEquals(httpsUrl, ShareMenuActionHandler.getUrlToShare(httpsUrl, contentUrl));
    }

    private static class MockTab extends Tab {
        public WebContents webContents;
        public String url;
        public boolean isShowingErrorPage;
        public boolean isShowingInterstitialPage;

        public MockTab() {
            super(INVALID_TAB_ID, false, null);
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
