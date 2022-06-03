// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tasks.tab_management.TabUiUnitTestUtils;
import org.chromium.content_public.browser.WebContents;

import java.nio.ByteBuffer;

/**
 * Unit tests for {@link HistoricalTabSaver}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HistoricalTabSaverUnitTest {
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    HistoricalTabSaver.Natives mHistoricalTabSaverJni;
    @Mock
    WebContentsStateBridge.Natives mWebContentsStateBridgeJni;

    @Mock
    public TabImpl mTabImplMock;
    @Mock
    public CriticalPersistedTabData mCriticalPersistedTabData;
    @Mock
    public WebContents mWebContentsMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(HistoricalTabSaverJni.TEST_HOOKS, mHistoricalTabSaverJni);
        mocker.mock(WebContentsStateBridgeJni.TEST_HOOKS, mWebContentsStateBridgeJni);
        TabUiUnitTestUtils.prepareTab(
                mTabImplMock, CriticalPersistedTabData.class, mCriticalPersistedTabData);
    }

    @Test
    public void testCreateHistoricalTab_NotFrozen_HistoricalTabCreated() {
        doReturn(false).when(mTabImplMock).isFrozen();
        doReturn(mWebContentsMock).when(mTabImplMock).getWebContents();

        HistoricalTabSaver.createHistoricalTab(mTabImplMock);

        verify(mHistoricalTabSaverJni).createHistoricalTabFromContents(eq(mWebContentsMock));
    }

    @Test
    public void testCreateHistoricalTab_Frozen_NullWebContentsState_HistoricalTabNotCreated() {
        doReturn(true).when(mTabImplMock).isFrozen();
        doReturn(null).when(mCriticalPersistedTabData).getWebContentsState();

        HistoricalTabSaver.createHistoricalTab(mTabImplMock);

        verify(mHistoricalTabSaverJni, never()).createHistoricalTabFromContents(any());
    }

    @Test
    public void testCreateHistoricalTab_Frozen_RestoreFailed_HistoricalTabNotCreated() {
        ByteBuffer buffer = ByteBuffer.allocate(1);
        WebContentsState webContentsState = new WebContentsState(buffer);
        webContentsState.setVersion(123);

        doReturn(true).when(mTabImplMock).isFrozen();
        doReturn(webContentsState).when(mCriticalPersistedTabData).getWebContentsState();
        doReturn(null)
                .when(mWebContentsStateBridgeJni)
                .restoreContentsFromByteBuffer(eq(buffer), eq(123), eq(true));

        HistoricalTabSaver.createHistoricalTab(mTabImplMock);

        verify(mHistoricalTabSaverJni, never()).createHistoricalTabFromContents(any());
    }

    @Test
    public void testCreateHistoricalTab_Frozen_Restored_HistoricalTabCreated() {
        ByteBuffer buffer = ByteBuffer.allocate(1);
        WebContentsState webContentsState = new WebContentsState(buffer);
        webContentsState.setVersion(123);

        doReturn(true).when(mTabImplMock).isFrozen();
        doReturn(webContentsState).when(mCriticalPersistedTabData).getWebContentsState();
        doReturn(mWebContentsMock)
                .when(mWebContentsStateBridgeJni)
                .restoreContentsFromByteBuffer(eq(buffer), eq(123), eq(true));

        HistoricalTabSaver.createHistoricalTab(mTabImplMock);

        verify(mHistoricalTabSaverJni).createHistoricalTabFromContents(eq(mWebContentsMock));
    }
}
