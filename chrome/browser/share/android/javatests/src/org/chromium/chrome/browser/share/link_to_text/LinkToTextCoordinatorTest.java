// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Tests for {@link LinkToTextCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class LinkToTextCoordinatorTest {
    // Mock class for |LinkToTextCoordinator| that disables |requestSelector| call.
    private class MockLinkToTextCoordinator extends LinkToTextCoordinator {
        public MockLinkToTextCoordinator(Context context, Tab tab,
                ChromeOptionShareCallback chromeOptionShareCallback, String visibleUrl,
                String selectedText) {
            super(context, tab, chromeOptionShareCallback, visibleUrl, selectedText);
        }

        @Override
        public void requestSelector() {}
    };

    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Mock
    private ChromeOptionShareCallback mShareCallback;
    @Mock
    private WindowAndroid mWindow;
    @Mock
    private Tab mTab;
    @Mock
    private WebContents mWebContents;
    @Mock
    private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;

    private Activity mAcivity;
    private static final String SELECTED_TEXT = "selection";
    private static final String VISIBLE_URL = "www.example.com";

    @Before
    public void setUpTest() {
        mAcivity = Robolectric.setupActivity(Activity.class);
        MockitoAnnotations.initMocks(this);
        jniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDistillerUrlUtilsJniMock);
        when(DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(VISIBLE_URL))
                .thenReturn(VISIBLE_URL);

        doNothing().when(mShareCallback).showThirdPartyShareSheet(any(), any(), anyLong());
        Mockito.when(mTab.getWebContents()).thenReturn(mWebContents);
        Mockito.when(mTab.getWindowAndroid()).thenReturn(mWindow);
    }

    @Test
    @SmallTest
    public void getUrlToShareTest() {
        String selector = "selector";
        String expectedUrlToShare = "www.example.com#:~:text=selector";
        MockLinkToTextCoordinator coordinator = new MockLinkToTextCoordinator(
                mAcivity, mTab, mShareCallback, VISIBLE_URL, SELECTED_TEXT);
        Assert.assertEquals(expectedUrlToShare, coordinator.getUrlToShare(selector));
    }

    @Test
    @SmallTest
    public void getUrlToShareTest_URLWithFragment() {
        String selector = "selector";
        String expectedUrlToShare = "www.example.com#:~:text=selector";
        MockLinkToTextCoordinator coordinator = new MockLinkToTextCoordinator(
                mAcivity, mTab, mShareCallback, VISIBLE_URL + "#elementid", SELECTED_TEXT);
        Assert.assertEquals(expectedUrlToShare, coordinator.getUrlToShare(selector));
    }

    @Test
    @SmallTest
    public void getUrlToShareTest_EmptySelector() {
        String selector = "";
        String expectedUrlToShare = "www.example.com";
        MockLinkToTextCoordinator coordinator = new MockLinkToTextCoordinator(
                mAcivity, mTab, mShareCallback, VISIBLE_URL, SELECTED_TEXT);
        Assert.assertEquals(expectedUrlToShare, coordinator.getUrlToShare(selector));
    }

    @Test
    @SmallTest
    public void onSelectorReadyTest() {
        MockLinkToTextCoordinator coordinator = new MockLinkToTextCoordinator(
                mAcivity, mTab, mShareCallback, VISIBLE_URL, SELECTED_TEXT);
        // OnSelectorReady should call back the share sheet.
        coordinator.onSelectorReady("selector");
        verify(mShareCallback).showThirdPartyShareSheet(any(), any(), anyLong());
    }

    @Test
    @SmallTest
    public void onSelectorReadyTest_EmptySelector() {
        MockLinkToTextCoordinator coordinator = new MockLinkToTextCoordinator(
                mAcivity, mTab, mShareCallback, VISIBLE_URL, SELECTED_TEXT);
        // OnSelectorReady should call back the share sheet.
        coordinator.onSelectorReady("");
        verify(mShareCallback).showThirdPartyShareSheet(any(), any(), anyLong());
    }
}
