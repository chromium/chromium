// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link TabPrinter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabPrinterUnitTest {
    private static final int PROCESS_ID = 5;
    private static final int FRAME_ID = 7;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private WebContentsPrinter.Natives mNativeMock;

    @Before
    public void setUp() {
        WebContentsPrinterJni.setInstanceForTesting(mNativeMock);
        when(mTab.isInitialized()).thenReturn(true);
        when(mTab.getWebContents()).thenReturn(mWebContents);
    }

    @Test
    public void testFinishPrintWithoutInitiatePrintReachesNative() {
        new TabPrinter(mTab).finishPrint(PROCESS_ID, FRAME_ID);
        verify(mNativeMock).finishPrint(mWebContents, PROCESS_ID, FRAME_ID);
    }

    @Test
    public void testFinishPrintAfterFailedInitiatePrintReachesNative() {
        when(mNativeMock.initiatePrint(mWebContents, PROCESS_ID, FRAME_ID, false)).thenReturn(-1L);
        TabPrinter printer = new TabPrinter(mTab);
        assertFalse(printer.initiatePrint(PROCESS_ID, FRAME_ID));

        printer.finishPrint(PROCESS_ID, FRAME_ID);
        verify(mNativeMock).finishPrint(mWebContents, PROCESS_ID, FRAME_ID);
    }

    @Test
    public void testFinishPrintWithUninitializedTabIsNoOp() {
        when(mTab.isInitialized()).thenReturn(false);
        new TabPrinter(mTab).finishPrint(PROCESS_ID, FRAME_ID);
        verify(mNativeMock, never()).finishPrint(any(), anyInt(), anyInt());
        verify(mNativeMock, never()).initiatePrint(any(), anyInt(), anyInt(), anyBoolean());
    }
}
