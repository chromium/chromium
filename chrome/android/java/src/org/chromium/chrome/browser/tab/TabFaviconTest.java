// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;

import androidx.annotation.ColorInt;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ObserverList;
import org.chromium.base.Promise;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TabFavicon}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabFaviconTest {
    private static final int IDEAL_SIZE = 4;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabFavicon.Natives mTabFaviconJni;
    @Mock private TabImpl mTab;
    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private WebContents mWebContents;
    @Mock private WebContents mNewWebContents;
    @Mock private FaviconHelper mFaviconHelper;
    @Mock private RenderFrameHost mMainFrame;
    @Mock private NavigationHandle mNavigation;
    @Mock private TabObserver mTabObserver;

    private UserDataHost mUserDataHost;
    private TabFavicon mTabFavicon;
    private Bitmap mCachedFavicon;

    @Before
    public void setUp() {
        TabFaviconJni.setInstanceForTesting(mTabFaviconJni);
        doReturn(12345L).when(mTabFaviconJni).init(any(), anyInt());

        mUserDataHost = new UserDataHost();
        doReturn(mUserDataHost).when(mTab).getUserDataHost();
        doReturn(mContext).when(mTab).getContext();
        doReturn(mResources).when(mContext).getResources();
        doReturn(IDEAL_SIZE).when(mResources).getDimensionPixelSize(anyInt());
        doReturn(false).when(mTab).isNativePage();
        doReturn(true).when(mTab).isInitialized();
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(null).when(mTab).getPendingLoadParams();
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mTab).getUrl();
        doReturn(new ObserverList<>()).when(mTab).getTabObservers();

        mTabFavicon = TabFavicon.from(mTab);
    }

    private static Bitmap makeBitmap(int size, @ColorInt int color) {
        return makeBitmap(size, size, color);
    }

    private static Bitmap makeBitmap(int width, int height, @ColorInt int color) {
        Bitmap image = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        image.eraseColor(color);
        return image;
    }

    private void onFaviconAvailable(Bitmap bitmap) {
        // Mimic the behavior of the native call, where `TabFavicon#shouldUpdateFaviconForBrowserUi`
        // is checked first before sending the bitmap into Java layer.
        if (mTabFavicon.shouldUpdateFaviconForBrowserUi(bitmap.getWidth(), bitmap.getHeight())) {
            mTabFavicon.onFaviconAvailable(
                    bitmap, JUnitTestGURLs.EXAMPLE_URL, /* isFallback= */ false);
        }
    }

    /**
     * Sets up a tab showing chrome://newtab with a cached favicon for the document at {@code
     * JUnitTestGURLs.URL_1}, and a navigation that committed a new document ({@code
     * JUnitTestGURLs.URL_2}) under the same visible URL. The WebUI NTP gate is satisfied: desktop
     * device and Google as the default search engine (the feature flag is enabled per test).
     */
    private void setUpNewDocumentUnderSameVisibleUrl() {
        DeviceInfo.setIsDesktopForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.IS_DSE_GOOGLE, true);

        ObserverList<TabObserver> observers = new ObserverList<>();
        observers.addObserver(mTabObserver);
        doReturn(observers).when(mTab).getTabObservers();
        doReturn(JUnitTestGURLs.NTP_URL).when(mTab).getUrl();
        doReturn(mMainFrame).when(mWebContents).getMainFrame();
        doReturn(JUnitTestGURLs.URL_1).when(mMainFrame).getLastCommittedURL();

        mTabFavicon.onFaviconAvailable(
                makeBitmap(IDEAL_SIZE, Color.GREEN), JUnitTestGURLs.URL_1, /* isFallback= */ false);
        mCachedFavicon = mTabFavicon.getFavicon();
        assertNotNull(mCachedFavicon);

        doReturn(true).when(mNavigation).hasCommitted();
        doReturn(false).when(mNavigation).isSameDocument();
        doReturn(JUnitTestGURLs.URL_2).when(mNavigation).getUrl();
    }

    private void assertFaviconKept() {
        assertSame(mCachedFavicon, mTabFavicon.getFavicon());
        verify(mTabObserver, never()).onFaviconUpdated(mTab, null, null);
    }

    @Test
    public void testOnFaviconAvailable_ReturnsBitmap() {
        assertNull(TabFavicon.getBitmap(mTab));

        onFaviconAvailable(makeBitmap(1, Color.GREEN));
        Bitmap bitmap = TabFavicon.getBitmap(mTab);
        assertNotNull(bitmap);
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0));
    }

    @Test
    public void testOnFaviconAvailable_IdealSize() {
        onFaviconAvailable(makeBitmap(1, Color.RED));
        onFaviconAvailable(makeBitmap(IDEAL_SIZE, Color.GREEN));
        onFaviconAvailable(makeBitmap(IDEAL_SIZE / 2, Color.YELLOW));
        onFaviconAvailable(makeBitmap(IDEAL_SIZE * 2, Color.BLUE));

        Bitmap bitmap = TabFavicon.getBitmap(mTab);
        assertNotNull(bitmap);
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0));
    }

    @Test
    public void testOnFaviconAvailable_SameSize() {
        // Inspired from https://crbug.com/40234963. Some pages purposefully switch favicons to try
        // to update the current favicon. This will typically result in the same sized favicon being
        // sent. So it's important that we update in this case.
        onFaviconAvailable(makeBitmap(1, Color.RED));
        onFaviconAvailable(makeBitmap(1, Color.GREEN));

        Bitmap bitmap = TabFavicon.getBitmap(mTab);
        assertNotNull(bitmap);
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0));
    }

    @Test
    public void testOnFaviconAvailable_DensityChangeInvalidatesCache() {
        // 1. Initial favicon at IDEAL_SIZE.
        onFaviconAvailable(makeBitmap(IDEAL_SIZE, Color.GREEN));
        Bitmap bitmap = TabFavicon.getBitmap(mTab);
        assertNotNull(bitmap);
        assertEquals(IDEAL_SIZE, bitmap.getWidth());
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0));

        // 2. Change density (ideal size doubles).
        int newIdealSize = IDEAL_SIZE * 2;
        doReturn(newIdealSize).when(mResources).getDimensionPixelSize(anyInt());

        // 3. Requesting bitmap should now bypass cache because cached size (IDEAL_SIZE) !=
        // newIdealSize.
        // It should return whatever native provides (we'll mock native to return a new larger
        // bitmap).
        Bitmap newBitmap = makeBitmap(newIdealSize, Color.BLUE);
        doReturn(newBitmap).when(mTabFaviconJni).getFavicon(anyLong());

        Bitmap result = TabFavicon.getBitmap(mTab);
        assertNotNull(result);
        assertEquals(newIdealSize, result.getWidth());
        assertEquals(Color.BLUE, result.getPixel(0, 0));
    }

    @Test
    public void testGetBitmap_frozenTabWithPendingLoad() {
        // A frozen tab can have a pending load but no WebContents.
        doReturn(null).when(mTab).getWebContents();
        doReturn(new LoadUrlParams("foo.com")).when(mTab).getPendingLoadParams();

        assertNull(TabFavicon.getBitmap(mTab));
    }

    @Test
    public void testGetFavicon_ColdTabCacheOptimization() {
        // Cache a favicon for the tab
        onFaviconAvailable(makeBitmap(IDEAL_SIZE, Color.GREEN));

        // Make WebContents null to simulate a cold tab
        doReturn(null).when(mTab).getWebContents();

        // When URL matches, it should return the cached favicon even if WebContents is null!
        Bitmap bitmap = mTabFavicon.getFavicon(true);
        assertNotNull(bitmap);
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0));
    }

    @Test
    public void testNonFallbackWins() {
        // 1. Store a live favicon
        mTabFavicon.onFaviconAvailable(
                makeBitmap(IDEAL_SIZE, Color.GREEN),
                JUnitTestGURLs.EXAMPLE_URL,
                /* isFallback= */ false);
        Bitmap bitmap = mTabFavicon.getFavicon();
        assertNotNull(bitmap);
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0));

        // 2. Receive a fallback database favicon. Since we already have a live favicon, it should
        // NOT overwrite it!
        mTabFavicon.onFaviconAvailable(
                makeBitmap(IDEAL_SIZE, Color.RED),
                JUnitTestGURLs.EXAMPLE_URL,
                /* isFallback= */ true);
        bitmap = mTabFavicon.getFavicon();
        assertNotNull(bitmap);
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0)); // Still green!
    }

    @Test
    public void testFallbackWinsIfNoCachedIcon() {
        // 1. Store a fallback favicon when cache is empty
        mTabFavicon.onFaviconAvailable(
                makeBitmap(IDEAL_SIZE, Color.RED),
                JUnitTestGURLs.EXAMPLE_URL,
                /* isFallback= */ true);
        Bitmap bitmap = mTabFavicon.getFavicon(true);
        assertNotNull(bitmap);
        assertEquals(Color.RED, bitmap.getPixel(0, 0)); // Red!
    }

    @Test
    public void testGetFaviconOrFallback_AsyncDBFetch() {
        mTabFavicon.mFaviconHelper = mFaviconHelper;
        ArgumentCaptor<FaviconHelper.FaviconImageCallback> callbackCaptor =
                ArgumentCaptor.forClass(FaviconHelper.FaviconImageCallback.class);
        doReturn(true)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        any(), any(), anyInt(), anyBoolean(), callbackCaptor.capture());

        Promise<Bitmap> promise = mTabFavicon.getFaviconOrFallback();
        assertNotNull(promise);
        assertTrue(promise.isPending());

        Bitmap fallbackBitmap = makeBitmap(IDEAL_SIZE, Color.BLUE);
        callbackCaptor.getValue().onFaviconAvailable(fallbackBitmap, JUnitTestGURLs.EXAMPLE_URL);

        assertTrue(promise.isFulfilled());
        Bitmap result = promise.getResult();
        assertNotNull(result);
        assertEquals(Color.BLUE, result.getPixel(0, 0));
    }

    @Test
    public void testGetFaviconOrFallback_RejectsOnTabDestroyed() {
        mTabFavicon.mFaviconHelper = mFaviconHelper;
        doReturn(true)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), anyBoolean(), any());

        Promise<Bitmap> promise1 = mTabFavicon.getFaviconOrFallback();
        Promise<Bitmap> promise2 = mTabFavicon.getFaviconOrFallback();
        assertNotNull(promise1);
        assertNotNull(promise2);
        assertTrue(promise1.isPending());
        assertTrue(promise2.isPending());

        mTabFavicon.destroyInternal();

        assertTrue(promise1.isRejected());
        assertTrue(promise2.isRejected());
    }

    @Test
    public void testGetFaviconOrFallback_RejectsIfNoFaviconAndCachedIconStale() {
        mTabFavicon.mFaviconHelper = mFaviconHelper;

        // 1. Cache a favicon for EXAMPLE_URL
        mTabFavicon.onFaviconAvailable(
                makeBitmap(IDEAL_SIZE, Color.GREEN),
                JUnitTestGURLs.EXAMPLE_URL,
                /* isFallback= */ false);

        // Verify it is cached
        assertEquals(Color.GREEN, mTabFavicon.getFavicon().getPixel(0, 0));

        // 2. Navigate to URL_1 (different URL)
        doReturn(JUnitTestGURLs.URL_1).when(mTab).getUrl();

        // Mock DB fetch to return null (no favicon)
        ArgumentCaptor<FaviconHelper.FaviconImageCallback> callbackCaptor =
                ArgumentCaptor.forClass(FaviconHelper.FaviconImageCallback.class);
        doReturn(true)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        any(), any(), anyInt(), anyBoolean(), callbackCaptor.capture());

        // 3. Call getFaviconOrFallback
        Promise<Bitmap> promise = mTabFavicon.getFaviconOrFallback();
        assertNotNull(promise);
        assertTrue(promise.isPending());

        // Trigger DB callback with null (no favicon found)
        callbackCaptor.getValue().onFaviconAvailable(null, null);

        // 4. Verify promise is rejected (should NOT return the green favicon)
        assertTrue(promise.isRejected());
    }

    @Test
    public void testGetFaviconOrFallback_RejectsIfUrlChangesDuringFetch() {
        mTabFavicon.mFaviconHelper = mFaviconHelper;

        // Mock DB fetch to capture callback
        ArgumentCaptor<FaviconHelper.FaviconImageCallback> callbackCaptor =
                ArgumentCaptor.forClass(FaviconHelper.FaviconImageCallback.class);
        doReturn(true)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        any(), any(), anyInt(), anyBoolean(), callbackCaptor.capture());

        // 1. Call getFaviconOrFallback while at EXAMPLE_URL
        Promise<Bitmap> promise = mTabFavicon.getFaviconOrFallback();
        assertNotNull(promise);
        assertTrue(promise.isPending());

        // 2. Navigate to URL_1 before DB fetch completes
        doReturn(JUnitTestGURLs.URL_1).when(mTab).getUrl();

        // 3. Trigger DB callback with a valid image (e.g. Blue)
        Bitmap dbBitmap = makeBitmap(IDEAL_SIZE, Color.BLUE);
        callbackCaptor.getValue().onFaviconAvailable(dbBitmap, JUnitTestGURLs.EXAMPLE_URL);

        // 4. Verify promise is rejected due to URL change
        assertTrue(promise.isRejected());

        // 5. Verify cache is not polluted (getFavicon for URL_1 should be null)
        assertNull(mTabFavicon.getFavicon());
    }

    @Test
    public void testGetNativePtrForTab_CreatesInstanceIfNeeded() {
        TabImpl newTab = mock(TabImpl.class);
        UserDataHost newUserDataHost = new UserDataHost();
        doReturn(newUserDataHost).when(newTab).getUserDataHost();
        doReturn(mContext).when(newTab).getContext();
        doReturn(mResources).when(mContext).getResources();
        doReturn(IDEAL_SIZE).when(mResources).getDimensionPixelSize(anyInt());
        doReturn(true).when(newTab).isInitialized();

        long nativePtr = TabFavicon.getNativePtrForTab(newTab);
        assertEquals(12345L, nativePtr);
    }

    @Test
    public void testGetBitmapWithFallback_NoInstanceReturnsNull() {
        TabImpl newTab = mock(TabImpl.class);
        UserDataHost newUserDataHost = new UserDataHost();
        doReturn(newUserDataHost).when(newTab).getUserDataHost();
        doReturn(true).when(newTab).isInitialized();

        assertNull(TabFavicon.getBitmapWithFallback(newTab, false));
    }

    @Test
    public void testConstructor_InitsWebContentsIfPresent() {
        verify(mTabFaviconJni).setWebContents(12345L, mWebContents);
    }

    @Test
    public void testGetBitmapWithFallback_NativePageReturnsNull() {
        doReturn(true).when(mTab).isNativePage();

        Bitmap bitmap = TabFavicon.getBitmapWithFallback(mTab, /* allowFallback= */ false);
        assertNull(bitmap);
    }

    @Test
    public void testGetFaviconOrFallback_NativePageRejected() {
        doReturn(true).when(mTab).isNativePage();

        Promise<Bitmap> promise = mTabFavicon.getFaviconOrFallback();
        assertNotNull(promise);
        assertTrue(promise.isRejected());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_STARTUP_TAB_OPTIMIZATIONS)
    public void testOnContentChanged_deduplicatesCallsWhenOptimizationsEnabled() {
        // setUp() already called setWebContents once during construction via
        // onContentChanged(mTab).
        verify(mTabFaviconJni, times(1)).setWebContents(12345L, mWebContents);

        mTabFavicon.onContentChanged(mTab);
        verify(mTabFaviconJni, times(1)).setWebContents(12345L, mWebContents);

        doReturn(mNewWebContents).when(mTab).getWebContents();
        mTabFavicon.onContentChanged(mTab);
        verify(mTabFaviconJni, times(1)).setWebContents(12345L, mNewWebContents);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CLANK_STARTUP_TAB_OPTIMIZATIONS)
    public void testOnContentChanged_callsEveryTimeWhenOptimizationsDisabled() {
        // setUp() called initWebContents directly without updating TabWebContentsUserData.
        verify(mTabFaviconJni, times(1)).setWebContents(12345L, mWebContents);

        mTabFavicon.onContentChanged(mTab);
        verify(mTabFaviconJni, times(2)).setWebContents(12345L, mWebContents);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_WEB_UI_NTP_ANDROID)
    public void testNewDocument_UnderSameVisibleUrlInvalidatesFavicon() {
        setUpNewDocumentUnderSameVisibleUrl();

        mTabFavicon.maybeInvalidateFaviconForNewDocument(mNavigation);

        assertNull(mTabFavicon.getFavicon());
        verify(mTabObserver).onFaviconUpdated(mTab, null, null);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_WEB_UI_NTP_ANDROID)
    public void testNewDocument_SameDocumentNavigationKeepsFavicon() {
        setUpNewDocumentUnderSameVisibleUrl();
        doReturn(true).when(mNavigation).isSameDocument();

        mTabFavicon.maybeInvalidateFaviconForNewDocument(mNavigation);

        assertFaviconKept();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_WEB_UI_NTP_ANDROID)
    public void testNewDocument_UncommittedNavigationKeepsFavicon() {
        setUpNewDocumentUnderSameVisibleUrl();
        doReturn(false).when(mNavigation).hasCommitted();

        mTabFavicon.maybeInvalidateFaviconForNewDocument(mNavigation);

        assertFaviconKept();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_WEB_UI_NTP_ANDROID)
    public void testNewDocument_SameDocumentUrlKeepsFavicon() {
        setUpNewDocumentUnderSameVisibleUrl();
        doReturn(JUnitTestGURLs.URL_1).when(mNavigation).getUrl();

        mTabFavicon.maybeInvalidateFaviconForNewDocument(mNavigation);

        assertFaviconKept();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_WEB_UI_NTP_ANDROID)
    public void testNewDocument_VisibleUrlChangeIsLeftToPageUrlChanged() {
        setUpNewDocumentUnderSameVisibleUrl();
        // A navigation that also changes the visible URL is already handled by pageUrlChanged(),
        // so observers must not be notified a second time.
        doReturn(JUnitTestGURLs.URL_2).when(mTab).getUrl();

        mTabFavicon.maybeInvalidateFaviconForNewDocument(mNavigation);

        verify(mTabObserver, never()).onFaviconUpdated(mTab, null, null);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_WEB_UI_NTP_ANDROID)
    public void testNewDocument_NoCachedFaviconIsNoOp() {
        setUpNewDocumentUnderSameVisibleUrl();
        mTabFavicon.maybeInvalidateFaviconForNewDocument(mNavigation);
        verify(mTabObserver).onFaviconUpdated(mTab, null, null);

        // A second navigation with nothing cached must not notify again.
        mTabFavicon.maybeInvalidateFaviconForNewDocument(mNavigation);

        verify(mTabObserver, times(1)).onFaviconUpdated(mTab, null, null);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_WEB_UI_NTP_ANDROID)
    public void testNewDocument_FaviconFetchedAfterInvalidationIsServedAgain() {
        setUpNewDocumentUnderSameVisibleUrl();
        mTabFavicon.maybeInvalidateFaviconForNewDocument(mNavigation);
        assertNull(mTabFavicon.getFavicon());

        doReturn(JUnitTestGURLs.URL_2).when(mMainFrame).getLastCommittedURL();
        mTabFavicon.onFaviconAvailable(
                makeBitmap(IDEAL_SIZE, Color.BLUE), JUnitTestGURLs.URL_2, /* isFallback= */ false);

        assertNotNull(mTabFavicon.getFavicon());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.USE_WEB_UI_NTP_ANDROID)
    public void testNewDocument_WebUiNtpDisabledKeepsFavicon() {
        setUpNewDocumentUnderSameVisibleUrl();

        mTabFavicon.maybeInvalidateFaviconForNewDocument(mNavigation);

        assertFaviconKept();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.USE_WEB_UI_NTP_ANDROID)
    public void testNewDocument_NonDesktopDeviceKeepsFavicon() {
        setUpNewDocumentUnderSameVisibleUrl();
        DeviceInfo.setIsDesktopForTesting(false);

        mTabFavicon.maybeInvalidateFaviconForNewDocument(mNavigation);

        assertFaviconKept();
    }
}
