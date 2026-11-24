// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Environment;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.test.util.SadTabRule;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;
import org.chromium.url.Origin;

import java.io.File;
import java.util.concurrent.atomic.AtomicReference;

/** Unit tests for OfflinePageUtils. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {OfflinePageUtilsUnitTest.WrappedEnvironment.class})
public class OfflinePageUtilsUnitTest {
    private static final String UNTRUSTED_PAGE_FILE_URL = "file:///sdcard/Download/page.mhtml";

    @Mock public Profile.Natives mMockProfileNatives;

    @Mock private File mMockDataDirectory;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private OfflinePageBridge mOfflinePageBridge;
    @Mock private OfflinePageUtils.Internal mOfflinePageUtils;
    @Mock private Origin mOpaqueOrigin;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public final SadTabRule mSadTabRule = new SadTabRule();

    @Before
    public void setUp() {
        ProfileJni.setInstanceForTesting(mMockProfileNatives);
        WrappedEnvironment.setDataDirectoryForTest(mMockDataDirectory);
        Origin.setOpaqueOriginFactoryForTesting(() -> mOpaqueOrigin);

        // Setting up a mock tab. These are the values common to most tests, but individual
        // tests might easily overwrite them.
        doReturn(false).when(mTab).isShowingErrorPage();
        doReturn(new UserDataHost()).when(mTab).getUserDataHost();
        doReturn(true).when(mTab).isInitialized();
        mSadTabRule.setTab(mTab);
        doNothing().when(mTab).addObserver(any(TabObserver.class));
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(false).when(mWebContents).isDestroyed();
        doReturn(false).when(mWebContents).isIncognito();

        doNothing()
                .when(mOfflinePageBridge)
                .savePage(
                        eq(mWebContents),
                        any(ClientId.class),
                        any(OfflinePageBridge.SavePageCallback.class));

        doReturn(mOfflinePageBridge)
                .when(mOfflinePageUtils)
                .getOfflinePageBridge((Profile) isNull());
        OfflinePageUtils.setInstanceForTesting(mOfflinePageUtils);
    }

    @Test
    @Feature({"OfflinePages"})
    public void testGetFreeSpaceInBytes() {
        when(mMockDataDirectory.getUsableSpace()).thenReturn(1234L);
        assertEquals(1234L, OfflinePageUtils.getFreeSpaceInBytes());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testGetTotalSpaceInBytes() {
        when(mMockDataDirectory.getTotalSpace()).thenReturn(56789L);
        assertEquals(56789L, OfflinePageUtils.getTotalSpaceInBytes());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testSaveBookmarkOffline() {
        OfflinePageUtils.saveBookmarkOffline(new BookmarkId(42, BookmarkType.NORMAL), mTab);
        verify(mOfflinePageBridge, times(1))
                .savePage(
                        eq(mWebContents),
                        any(ClientId.class),
                        any(OfflinePageBridge.SavePageCallback.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testSaveBookmarkOffline_inputValidation() {
        OfflinePageUtils.saveBookmarkOffline(null, mTab);
        // Save page not called because bookmarkId is null.
        verify(mOfflinePageBridge, times(0))
                .savePage(
                        eq(mWebContents),
                        any(ClientId.class),
                        any(OfflinePageBridge.SavePageCallback.class));

        BookmarkId bookmarkId = new BookmarkId(42, BookmarkType.NORMAL);
        doReturn(true).when(mTab).isShowingErrorPage();
        OfflinePageUtils.saveBookmarkOffline(bookmarkId, mTab);
        // Save page not called because tab is showing an error page.
        verify(mOfflinePageBridge, times(0))
                .savePage(
                        eq(mWebContents),
                        any(ClientId.class),
                        any(OfflinePageBridge.SavePageCallback.class));

        doReturn(false).when(mTab).isShowingErrorPage();
        mSadTabRule.show(true);
        OfflinePageUtils.saveBookmarkOffline(bookmarkId, mTab);
        // Save page not called because tab is showing a sad tab.
        verify(mOfflinePageBridge, times(0))
                .savePage(
                        eq(mWebContents),
                        any(ClientId.class),
                        any(OfflinePageBridge.SavePageCallback.class));

        mSadTabRule.show(false);
        doReturn(null).when(mTab).getWebContents();
        OfflinePageUtils.saveBookmarkOffline(bookmarkId, mTab);
        // Save page not called because tab returns null web contents.
        verify(mOfflinePageBridge, times(0))
                .savePage(
                        eq(mWebContents),
                        any(ClientId.class),
                        any(OfflinePageBridge.SavePageCallback.class));

        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(true).when(mWebContents).isDestroyed();
        OfflinePageUtils.saveBookmarkOffline(bookmarkId, mTab);
        // Save page not called because web contents is destroyed.
        verify(mOfflinePageBridge, times(0))
                .savePage(
                        eq(mWebContents),
                        any(ClientId.class),
                        any(OfflinePageBridge.SavePageCallback.class));

        doReturn(false).when(mWebContents).isDestroyed();
        doReturn(true).when(mWebContents).isIncognito();
        OfflinePageUtils.saveBookmarkOffline(bookmarkId, mTab);
        // Save page not called because web contents is incognito.
        verify(mOfflinePageBridge, times(0))
                .savePage(
                        eq(mWebContents),
                        any(ClientId.class),
                        any(OfflinePageBridge.SavePageCallback.class));
    }

    @Test
    @Feature({"OfflinePages"})
    public void testReloadUntrustedPage_loadsOnlineUrl() {
        setUpUntrustedOfflinePage(JUnitTestGURLs.EXAMPLE_URL);

        AtomicReference<LoadUrlParams> loadedParams = new AtomicReference<>();
        OfflinePageUtils.reload(mWebContents, loadedParams::set);

        // Reloading the local archive URL would keep showing the offline page, and the
        // archive-reported URL must not receive browser-initiated navigation privileges.
        LoadUrlParams params = loadedParams.get();
        assertEquals(JUnitTestGURLs.EXAMPLE_URL.getSpec(), params.getUrl());
        assertTrue(params.getIsRendererInitiated());
        assertEquals(mOpaqueOrigin, params.getInitiatorOrigin());
        assertEquals(PageTransition.LINK, params.getTransitionType());
    }

    @Test
    @Feature({"OfflinePages"})
    public void testReloadUntrustedPage_noOnlineUrl() {
        setUpUntrustedOfflinePage(GURL.emptyGURL());

        AtomicReference<LoadUrlParams> loadedParams = new AtomicReference<>();
        OfflinePageUtils.reload(mWebContents, loadedParams::set);

        LoadUrlParams params = loadedParams.get();
        assertEquals(UNTRUSTED_PAGE_FILE_URL, params.getUrl());
        assertFalse(params.getIsRendererInitiated());
        assertNull(params.getInitiatorOrigin());
        assertEquals(
                PageTransition.RELOAD | PageTransition.FROM_ADDRESS_BAR,
                params.getTransitionType());
    }

    private void setUpUntrustedOfflinePage(GURL onlineUrl) {
        OfflinePageItem offlinePage = mock(OfflinePageItem.class);
        doReturn(UNTRUSTED_PAGE_FILE_URL).when(offlinePage).getUrl();
        doReturn(offlinePage).when(mOfflinePageBridge).getOfflinePage(mWebContents);
        doReturn(onlineUrl).when(mOfflinePageBridge).getUntrustedOnlineUrl(mWebContents);
        doReturn(false).when(mOfflinePageUtils).isShowingTrustedOfflinePage(mWebContents);
    }

    /** A shadow/wrapper of android.os.Environment that allows injecting a test directory. */
    @Implements(Environment.class)
    public static class WrappedEnvironment {
        private static File sDataDirectory;

        public static void setDataDirectoryForTest(File testDirectory) {
            sDataDirectory = testDirectory;
        }

        @Implementation
        public static File getDataDirectory() {
            return sDataDirectory;
        }
    }
}
