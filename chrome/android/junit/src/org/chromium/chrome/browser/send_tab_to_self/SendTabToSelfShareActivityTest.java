// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridgeJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/** Tests for SendTabToSelfShareActivity */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SendTabToSelfShareActivityTest {
    @Rule
    public JniMocker mocker = new JniMocker();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock
    SendTabToSelfAndroidBridge.Natives mNativeMock;
    @Mock
    private Tab mTab;
    @Mock
    private WebContents mWebContents;

    @Before
    public void setUp() {
        mocker.mock(SendTabToSelfAndroidBridgeJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    @SmallTest
    public void testIsFeatureAvailable() {
        boolean expected = true;
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mNativeMock.isFeatureAvailable(eq(mWebContents))).thenReturn(expected);

        boolean actual = SendTabToSelfShareActivity.featureIsAvailable(mTab);
        Assert.assertEquals(expected, actual);
    }
}
