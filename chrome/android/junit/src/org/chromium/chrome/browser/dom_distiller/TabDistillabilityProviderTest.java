// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

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
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.dom_distiller.content.DistillablePageUtils;
import org.chromium.components.dom_distiller.content.DistillablePageUtilsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Unit tests for {@link TabDistillabilityProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabDistillabilityProviderTest {

    private static final GURL URL_1 = new GURL("http://www.test1.com");
    private static final GURL URL_2 = new GURL("http://www.test2.com");

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private DistillablePageUtils.Natives mDistillablePageUtilsJni;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private Profile mProfile;

    private TabDistillabilityProvider mProvider;

    @Before
    public void setUp() {
        DistillablePageUtilsJni.setInstanceForTesting(mDistillablePageUtilsJni);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mTab.getUrl()).thenReturn(URL_1);
        when(mProfile.isOffTheRecord()).thenReturn(false);

        mProvider = new TabDistillabilityProvider(mTab);
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
        GURL url_1_fragment = new GURL("http://www.test1.com#fragment");
        // Setup the distillation result, and verify loading the same page is a no-op.
        mProvider.onIsPageDistillableResult(
                /* url= */ url_1_fragment,
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
        when(mTab.getUrl()).thenReturn(URL_2);
        mProvider.onDidFinishNavigationInPrimaryMainFrame(mTab, null);
        verify(mDistillablePageUtilsJni).setDelegate(any(), eq(mProvider));
    }
}
