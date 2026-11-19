// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.reader_mode;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.web_distiller.content.DistillablePageUtils;
import org.chromium.components.web_distiller.content.DistillablePageUtilsJni;
import org.chromium.components.web_distiller.core.WebDistillerUrlUtilsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Unit tests for {@link TabDistillabilityProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabDistillabilityProviderTest {

    private static final GURL URL_1 = new GURL("http://www.test1.com");
    private static final GURL URL_2 = new GURL("http://www.test2.com");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DistillablePageUtils.Natives mDistillablePageUtilsJni;
    @Mock private WebDistillerUrlUtilsJni mWebDistillerUrlUtilsJni;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private Profile mProfile;

    private final UserDataHost mUserDataHost = new UserDataHost();
    private TabDistillabilityProvider mProvider;

    @Before
    public void setUp() {
        DistillablePageUtilsJni.setInstanceForTesting(mDistillablePageUtilsJni);
        WebDistillerUrlUtilsJni.setInstanceForTesting(mWebDistillerUrlUtilsJni);

        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.getUrl()).thenReturn(URL_1);
        when(mProfile.isOffTheRecord()).thenReturn(false);

        mProvider = TabDistillabilityProvider.from(mTab);
        verify(mDistillablePageUtilsJni).setDelegate(any(), eq(mProvider));
        // Reset the mocks so tests don't have to take this registration into account.
        Mockito.reset(mDistillablePageUtilsJni);
    }

    @Test
    public void finishNavigationWithSameUrlDoesNothing() {
        // Setup the distillation result, and verify loading the same page is a no-op.
        mProvider.onIsPageDistillableResult(
                /* url= */ URL_1,
                /* isDistillable= */ true,
                /* isLast= */ true,
                /* isLongArticle= */ false,
                /* isMobileOptimized= */ true);
        assertTrue(mProvider.isDistillabilityDetermined());

        mProvider.onDidFinishNavigationInPrimaryMainFrame(mTab, null);
        assertTrue(mProvider.isDistillabilityDetermined());
    }

    @Test
    public void finishNavigationWithSameUrlDifferentFragmentDoesNothing() {
        GURL url1Fragment = new GURL("http://www.test1.com#fragment");
        // Setup the distillation result, and verify loading the same page is a no-op.
        mProvider.onIsPageDistillableResult(
                /* url= */ url1Fragment,
                /* isDistillable= */ true,
                /* isLast= */ true,
                /* isLongArticle= */ false,
                /* isMobileOptimized= */ true);
        assertTrue(mProvider.isDistillabilityDetermined());

        mProvider.onDidFinishNavigationInPrimaryMainFrame(mTab, null);
        assertTrue(mProvider.isDistillabilityDetermined());
    }

    @Test
    public void finishNavigationOnUrlWithDifferentResult() {
        // Setup the distillation result, and verify loading the different page invalidates.
        mProvider.onIsPageDistillableResult(
                /* url= */ URL_1,
                /* isDistillable= */ true,
                /* isLast= */ true,
                /* isLongArticle= */ false,
                /* isMobileOptimized= */ true);
        assertTrue(mProvider.isDistillabilityDetermined());

        when(mTab.getUrl()).thenReturn(URL_2);
        mProvider.onDidFinishNavigationInPrimaryMainFrame(mTab, null);
        assertFalse(mProvider.isDistillabilityDetermined());
    }

    @Test
    public void differentWebContentsReregistersDelegate() {
        WebContents webContents = Mockito.mock(WebContents.class);
        when(mTab.getWebContents()).thenReturn(webContents);
        mProvider.onContentChanged(mTab);
        verify(mDistillablePageUtilsJni).setDelegate(eq(mWebContents), eq(null));
        verify(mDistillablePageUtilsJni).setDelegate(eq(webContents), eq(mProvider));

        mUserDataHost.destroy();
        verify(mDistillablePageUtilsJni).setDelegate(eq(webContents), eq(null));
    }

    @Test
    public void testLazyInitialization() {
        // Start with null WebContents (simulating frozen tab)
        mUserDataHost.removeUserData(TabDistillabilityProvider.USER_DATA_KEY).destroy();
        Mockito.reset(mDistillablePageUtilsJni);
        when(mTab.getWebContents()).thenReturn(null);

        mProvider = TabDistillabilityProvider.from(mTab);

        // Provider should exist, but delegate should not be set
        verify(mDistillablePageUtilsJni, Mockito.never()).setDelegate(any(), any());

        // Simulate WebContents being set (tab restored)
        when(mTab.getWebContents()).thenReturn(mWebContents);
        mProvider.onContentChanged(mTab);

        // Delegate should now be set
        verify(mDistillablePageUtilsJni).setDelegate(eq(mWebContents), eq(mProvider));
    }

    @Test
    public void testCleanupOnFreeze() {
        // Start with active WebContents
        // (setUp already constructed mProvider and registered delegate)
        Mockito.reset(mDistillablePageUtilsJni);

        // Simulate WebContents being cleared (tab frozen)
        when(mTab.getWebContents()).thenReturn(null);
        mProvider.onContentChanged(mTab);

        // Delegate should be cleared on the old WebContents
        verify(mDistillablePageUtilsJni).setDelegate(eq(mWebContents), eq(null));
    }

    @Test
    public void testOnContentChanged_sameWebContentsDoesNotResetState() {
        mProvider.onIsPageDistillableResult(
                /* url= */ URL_1,
                /* isDistillable= */ true,
                /* isLast= */ true,
                /* isLongArticle= */ false,
                /* isMobileOptimized= */ true);
        assertTrue(mProvider.isDistillabilityDetermined());
        assertTrue(mProvider.isDistillable());

        mProvider.onContentChanged(mTab);

        assertTrue(mProvider.isDistillabilityDetermined());
        assertTrue(mProvider.isDistillable());
        verify(mDistillablePageUtilsJni, Mockito.never()).setDelegate(any(), any());
    }

    @Test
    public void testNavigationToAndFromDistilledPagePreservesState() {
        mProvider.onIsPageDistillableResult(
                /* url= */ URL_1,
                /* isDistillable= */ true,
                /* isLast= */ true,
                /* isLongArticle= */ false,
                /* isMobileOptimized= */ true);
        assertTrue(mProvider.isDistillabilityDetermined());

        GURL distillerUrl = new GURL("chrome-distiller://www.test1.com");
        when(mTab.getUrl()).thenReturn(distillerUrl);
        when(mWebDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);
        when(mWebDistillerUrlUtilsJni.getOriginalUrlFromDistillerUrl(any())).thenReturn(URL_1);

        mProvider.onDidFinishNavigationInPrimaryMainFrame(mTab, null);
        assertTrue(mProvider.isDistillabilityDetermined());
    }
}
