// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import static org.chromium.chrome.browser.ui.native_page.NativePageTest.INVALID_URLS;
import static org.chromium.chrome.browser.ui.native_page.NativePageTest.VALID_URLS;
import static org.chromium.chrome.browser.ui.native_page.NativePageTest.isValidInIncognito;

import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.chrome.browser.ui.native_page.NativePage.NativePageType;
import org.chromium.chrome.browser.ui.native_page.NativePageTest.UrlCombo;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 * Tests public methods in NativePageFactory.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NativePageFactoryTest {
    private NativePageFactory mNativePageFactory;

    private static class MockNativePage implements NativePage {
        public final @NativePageType int type;
        public int updateForUrlCalls;

        public MockNativePage(@NativePageType int type) {
            this.type = type;
        }

        @Override
        public void updateForUrl(String url) {
            updateForUrlCalls++;
        }

        @Override
        public String getUrl() {
            return null;
        }

        @Override
        public String getHost() {
            switch (type) {
                case NativePageType.NTP:
                    return UrlConstants.NTP_HOST;
                case NativePageType.BOOKMARKS:
                    return UrlConstants.BOOKMARKS_HOST;
                case NativePageType.RECENT_TABS:
                    return UrlConstants.RECENT_TABS_HOST;
                case NativePageType.HISTORY:
                    return UrlConstants.HISTORY_HOST;
                default:
                    Assert.fail("Unexpected NativePageType: " + type);
                    return null;
            }
        }

        @Override
        public void destroy() {}

        @Override
        public String getTitle() {
            return null;
        }

        @Override
        public int getBackgroundColor() {
            return 0;
        }

        @Override
        public boolean needsToolbarShadow() {
            return true;
        }

        @Override
        public View getView() {
            return null;
        }
    }

    private static class MockNativePageBuilder extends NativePageFactory.NativePageBuilder {
        private MockNativePageBuilder() {
            super(null, null, null, null, null, null, null, null, null, null, null, null);
        }

        @Override
        public NativePage buildNewTabPage(Tab tab, String url) {
            return new MockNativePage(NativePageType.NTP);
        }

        @Override
        public NativePage buildBookmarksPage(Tab tab) {
            return new MockNativePage(NativePageType.BOOKMARKS);
        }

        @Override
        public NativePage buildRecentTabsPage(Tab tab) {
            return new MockNativePage(NativePageType.RECENT_TABS);
        }

        @Override
        public NativePage buildHistoryPage(Tab tab, String url) {
            return new MockNativePage(NativePageType.HISTORY);
        }
    }

    @Before
    public void setUp() {
        mNativePageFactory =
                new NativePageFactory(null, null, null, null, null, null, null, null, null, null, null);
        mNativePageFactory.setNativePageBuilderForTesting(new MockNativePageBuilder());
    }

    /**
     * Ensures that NativePageFactory.createNativePageForURL() returns a native page of the right
     * type and reuses the candidate page if it's the right type.
     */
    @Test
    public void testCreateNativePage() {
        @NativePageType
        int[] candidateTypes = new int[] {NativePageType.NONE, NativePageType.NTP,
                NativePageType.BOOKMARKS, NativePageType.RECENT_TABS, NativePageType.HISTORY};
        for (boolean isIncognito : new boolean[] {true, false}) {
            for (UrlCombo urlCombo : VALID_URLS) {
                if (isIncognito && !isValidInIncognito(urlCombo)) continue;
                for (@NativePageType int candidateType : candidateTypes) {
                    MockNativePage candidate = candidateType == NativePageType.NONE ? null
                            : new MockNativePage(candidateType);
                    MockNativePage page =
                            (MockNativePage) mNativePageFactory.createNativePageForURL(
                                    urlCombo.url, candidate, null, isIncognito);
                    String debugMessage = String.format(
                            "Failed test case: isIncognito=%s, urlCombo={%s,%s}, candidateType=%s",
                            isIncognito, urlCombo.url, urlCombo.expectedType, candidateType);
                    Assert.assertNotNull(debugMessage, page);
                    Assert.assertEquals(debugMessage, 1, page.updateForUrlCalls);
                    Assert.assertEquals(debugMessage, urlCombo.expectedType, page.type);
                    if (candidateType == urlCombo.expectedType) {
                        Assert.assertSame(debugMessage, candidate, page);
                    } else {
                        Assert.assertNotSame(debugMessage, candidate, page);
                    }
                }
            }
        }
    }

    /**
     * Ensures that NativePageFactory.createNativePageForURL() returns null for URLs that don't
     * correspond to a native page.
     */
    @Test
    public void testCreateNativePageWithInvalidUrl() {
        for (UrlCombo urlCombo : VALID_URLS) {
            if (!isValidInIncognito(urlCombo)) {
                Assert.assertNull(urlCombo.url,
                        mNativePageFactory.createNativePageForURL(urlCombo.url, null, null, true));
            }
        }
        for (boolean isIncognito : new boolean[] {true, false}) {
            for (String invalidUrl : INVALID_URLS) {
                Assert.assertNull(invalidUrl,
                        mNativePageFactory.createNativePageForURL(
                                invalidUrl, null, null, isIncognito));
            }
        }
    }
}
