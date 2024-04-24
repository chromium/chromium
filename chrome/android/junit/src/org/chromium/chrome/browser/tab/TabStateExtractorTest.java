// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.Referrer;
import org.chromium.url.Origin;

import java.nio.ByteBuffer;

/** Tests for {@link TabStateExtractor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabStateExtractorTest {
    private static final int REFERRER_POLICY = 123;
    private static final String TITLE = "test_title";
    private static final String URL = "test_url";
    private static final String REFERRER_URL = "referrer_url";

    @Rule public JniMocker mocker = new JniMocker();

    @Mock private WebContentsStateBridge.Natives mWebContentsBridgeJni;
    @Mock private Tab mTabMock;
    @Mock private WebContents mWebContentsMock;
    @Mock private Origin mMockOrigin;

    private ByteBuffer mByteBuffer = ByteBuffer.allocateDirect(1);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(WebContentsStateBridgeJni.TEST_HOOKS, mWebContentsBridgeJni);

        doReturn(new UserDataHost()).when(mTabMock).getUserDataHost();
    }

    @Test
    @SmallTest
    public void testGetWebContentsState_notPending() {
        doReturn(null).when(mTabMock).getPendingLoadParams();
        doReturn(mWebContentsMock).when(mTabMock).getWebContents();
        doReturn(mByteBuffer)
                .when(mWebContentsBridgeJni)
                .getContentsStateAsByteBuffer(eq(mWebContentsMock));

        WebContentsState result = TabStateExtractor.getWebContentsState(mTabMock);

        assertNotNull(result);
        assertEquals(WebContentsState.CONTENTS_STATE_CURRENT_VERSION, result.version());
        assertEquals(mByteBuffer, result.buffer());
    }

    @Test
    @SmallTest
    public void testGetWebContentsState_pending() {
        LoadUrlParams loadUrlParams = new LoadUrlParams(URL);
        loadUrlParams.setReferrer(new Referrer(REFERRER_URL, REFERRER_POLICY));
        loadUrlParams.setInitiatorOrigin(mMockOrigin);
        doReturn(loadUrlParams).when(mTabMock).getPendingLoadParams();
        doReturn(TITLE).when(mTabMock).getTitle();
        doReturn(true).when(mTabMock).isIncognito();
        doReturn(mByteBuffer)
                .when(mWebContentsBridgeJni)
                .createSingleNavigationStateAsByteBuffer(
                        eq(TITLE),
                        eq(URL),
                        eq(REFERRER_URL),
                        eq(REFERRER_POLICY),
                        eq(mMockOrigin),
                        eq(true));

        WebContentsState result = TabStateExtractor.getWebContentsState(mTabMock);

        assertNotNull(result);
        assertEquals(WebContentsState.CONTENTS_STATE_CURRENT_VERSION, result.version());
        assertEquals(mByteBuffer, result.buffer());
    }
}
