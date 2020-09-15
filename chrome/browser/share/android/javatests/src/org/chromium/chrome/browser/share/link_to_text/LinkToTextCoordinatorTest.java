// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.link_to_text;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.ui.base.WindowAndroid;

/**
 * Tests for {@link LinkToTextCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class LinkToTextCoordinatorTest {
    @Mock
    private ChromeOptionShareCallback mShareCallback;
    @Mock
    private WindowAndroid mWindow;

    private Activity mAcivity;
    private static final String SELECTED_TEXT = "selection";
    private static final String VISIBLE_URL = "www.example.com";

    @Before
    public void setUpTest() {
        mAcivity = Robolectric.setupActivity(Activity.class);
        MockitoAnnotations.initMocks(this);
        doNothing()
                .when(mShareCallback)
                .showThirdPartyShareSheetWithMessage(anyString(), any(), any(), anyLong());
    }

    @Test
    @SmallTest
    public void getTextToShareTest() {
        String selector = "selector";
        String expectedTextToShare = "\"selection\"\nwww.example.com#:~:text=selector";
        LinkToTextCoordinator coordinator = new LinkToTextCoordinator(
                mAcivity, mWindow, mShareCallback, VISIBLE_URL, SELECTED_TEXT);
        Assert.assertEquals(expectedTextToShare, coordinator.getTextToShare(selector));
    }

    @Test
    @SmallTest
    public void getTextToShareTest_URLWithFragment() {
        String selector = "selector";
        String expectedTextToShare = "\"selection\"\nwww.example.com#:~:text=selector";
        LinkToTextCoordinator coordinator = new LinkToTextCoordinator(
                mAcivity, mWindow, mShareCallback, VISIBLE_URL + "#elementid", SELECTED_TEXT);
        Assert.assertEquals(expectedTextToShare, coordinator.getTextToShare(selector));
    }

    @Test
    @SmallTest
    public void getTextToShareTest_EmptySelector() {
        String selector = "";
        String expectedTextToShare = "\"selection\"\nwww.example.com";
        LinkToTextCoordinator coordinator = new LinkToTextCoordinator(
                mAcivity, mWindow, mShareCallback, VISIBLE_URL, SELECTED_TEXT);
        Assert.assertEquals(expectedTextToShare, coordinator.getTextToShare(selector));
    }

    @Test
    @SmallTest
    public void onSelectorReadyTest() {
        LinkToTextCoordinator coordinator = new LinkToTextCoordinator(
                mAcivity, mWindow, mShareCallback, VISIBLE_URL, SELECTED_TEXT);
        // OnSelectorReady should call back the share sheet.
        verify(mShareCallback)
                .showThirdPartyShareSheetWithMessage(anyString(), any(), any(), anyLong());
    }
}
