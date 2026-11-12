// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.TriState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link PageZoomIphController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PageZoomIphControllerTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private AppMenuHandler mAppMenuHandler;
    @Mock private UserEducationHelper mUserEducationHelper;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;

    @Captor private ArgumentCaptor<IphCommand> mIphCommandCaptor;

    private final View mToolbarMenuButton = new View(ApplicationProvider.getApplicationContext());
    private PageZoomIphController mPageZoomIphController;

    @Before
    public void setUp() {
        PageZoomUtils.setHasAnySavedZoomLevelsForTesting(TriState.FALSE);
        PageZoomUtils.setUsesDefaultPageZoomLevelForTesting(TriState.TRUE);
        mPageZoomIphController =
                new PageZoomIphController(
                        mAppMenuHandler, mToolbarMenuButton, mUserEducationHelper);
    }

    @Test
    public void testPageZoomIphShown() {
        mPageZoomIphController.showColdStartIph();
        verify(mUserEducationHelper).requestShowIph(mIphCommandCaptor.capture());

        IphCommand command = mIphCommandCaptor.getValue();
        Assert.assertEquals(
                "IphCommand feature should match.",
                FeatureConstants.PAGE_ZOOM_FEATURE,
                command.featureName);
        Assert.assertEquals(
                "IphCommand stringId should match.",
                R.string.page_zoom_iph_message,
                command.stringId);

        command.onShowCallback.run();
        verify(mAppMenuHandler).setMenuHighlight(R.id.page_zoom_id);

        command.onDismissCallback.run();
        verify(mAppMenuHandler).clearMenuHighlight();
    }

    @Test
    public void testShouldShowForTab_nullTabOrWebContents() {
        Assert.assertFalse(PageZoomIphController.shouldShowForTab(null));

        when(mTab.getWebContents()).thenReturn(null);
        Assert.assertFalse(PageZoomIphController.shouldShowForTab(mTab));
    }

    @Test
    public void testShouldShowForTab_webPage() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        Assert.assertTrue(PageZoomIphController.shouldShowForTab(mTab));

        // Non-NTP WebUI pages (such as chrome://history/) remain eligible.
        when(mTab.getUrl()).thenReturn(new GURL("chrome://history/"));
        Assert.assertTrue(PageZoomIphController.shouldShowForTab(mTab));
    }

    @Test
    public void testShouldShowForTab_nativePage() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.isNativePage()).thenReturn(true);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);
        Assert.assertFalse(PageZoomIphController.shouldShowForTab(mTab));
    }

    @Test
    public void testShouldShowForTab_webUiNtp() {
        // The WebUI NTP has WebContents and is not a native page.
        when(mTab.getWebContents()).thenReturn(mWebContents);
        // chrome://new-tab-page/ (committed WebUI NTP URL).
        when(mTab.getUrl()).thenReturn(new GURL(UrlConstants.NEW_TAB_PAGE_URL_LEGACY));
        Assert.assertFalse(PageZoomIphController.shouldShowForTab(mTab));

        // chrome://newtab/ (virtual NTP URL).
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        Assert.assertFalse(PageZoomIphController.shouldShowForTab(mTab));

        // chrome://new-tab-page-third-party/ (third-party WebUI NTP URL).
        when(mTab.getUrl())
                .thenReturn(
                        new GURL(
                                UrlConstants.CHROME_URL_PREFIX
                                        + UrlConstants.NEW_TAB_PAGE_THIRD_PARTY_HOST));
        Assert.assertFalse(PageZoomIphController.shouldShowForTab(mTab));
    }

    @Test
    public void testShouldShowForTab_hasUsedPageZoomFeature() {
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.EXAMPLE_URL);

        PageZoomUtils.setHasAnySavedZoomLevelsForTesting(TriState.TRUE);
        Assert.assertFalse(PageZoomIphController.shouldShowForTab(mTab));

        PageZoomUtils.setHasAnySavedZoomLevelsForTesting(TriState.FALSE);
        Assert.assertTrue(PageZoomIphController.shouldShowForTab(mTab));

        PageZoomUtils.setUsesDefaultPageZoomLevelForTesting(TriState.FALSE);
        Assert.assertFalse(PageZoomIphController.shouldShowForTab(mTab));

        PageZoomUtils.setUsesDefaultPageZoomLevelForTesting(TriState.TRUE);
        Assert.assertTrue(PageZoomIphController.shouldShowForTab(mTab));

        PageZoomUtils.setShouldAlwaysShowZoomMenuItem(false);
        Assert.assertFalse(PageZoomIphController.shouldShowForTab(mTab));
    }
}
