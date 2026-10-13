// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
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
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link WebContentsPrinter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebContentsPrinterUnitTest {
    private static final int BOUND_PROCESS_ID = 42;
    private static final int BOUND_FRAME_ID = 7;
    private static final long PACKED_BOUND_FRAME =
            ((long) BOUND_PROCESS_ID << 32) | (BOUND_FRAME_ID & 0xFFFFFFFFL);
    private static final int FD = 11;
    private static final int DPI = 300;
    private static final int WIDTH = 8500;
    private static final int HEIGHT = 11000;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContents mWebContents;
    @Mock private WebContentsPrinter.Natives mNativeMock;

    private WebContentsPrinter mPrinter;

    @Before
    public void setUp() {
        WebContentsPrinterJni.setInstanceForTesting(mNativeMock);
        mPrinter = new WebContentsPrinter(mWebContents);
    }

    @Test
    public void testFrameBindingAndTeardownLifecycle() {
        when(mNativeMock.initiatePrint(mWebContents, -1, -1, false)).thenReturn(PACKED_BOUND_FRAME);
        when(mNativeMock.print(
                        mWebContents,
                        BOUND_PROCESS_ID,
                        BOUND_FRAME_ID,
                        false,
                        FD,
                        DPI,
                        WIDTH,
                        HEIGHT,
                        null))
                .thenReturn(true);

        assertTrue(mPrinter.initiatePrint(-1, -1));
        assertTrue(mPrinter.print(-1, -1, FD, DPI, WIDTH, HEIGHT, null));
        verify(mNativeMock)
                .print(
                        mWebContents,
                        BOUND_PROCESS_ID,
                        BOUND_FRAME_ID,
                        false,
                        FD,
                        DPI,
                        WIDTH,
                        HEIGHT,
                        null);

        mPrinter.finishPrint(-1, -1);
        verify(mNativeMock).finishPrint(mWebContents, BOUND_PROCESS_ID, BOUND_FRAME_ID);

        // Bound coordinates are cleared by finishPrint().
        mPrinter.print(-1, -1, FD, DPI, WIDTH, HEIGHT, null);
        verify(mNativeMock).print(mWebContents, -1, -1, false, FD, DPI, WIDTH, HEIGHT, null);
    }

    @Test
    public void testExplicitCoordinatesTakePrecedenceOverBoundCoordinates() {
        when(mNativeMock.initiatePrint(mWebContents, -1, -1, false)).thenReturn(PACKED_BOUND_FRAME);
        assertTrue(mPrinter.initiatePrint(-1, -1));

        int explicitProcessId = 100;
        int explicitFrameId = 200;
        when(mNativeMock.print(
                        mWebContents,
                        explicitProcessId,
                        explicitFrameId,
                        false,
                        FD,
                        DPI,
                        WIDTH,
                        HEIGHT,
                        null))
                .thenReturn(true);

        assertTrue(
                mPrinter.print(explicitProcessId, explicitFrameId, FD, DPI, WIDTH, HEIGHT, null));
        verify(mNativeMock)
                .print(
                        mWebContents,
                        explicitProcessId,
                        explicitFrameId,
                        false,
                        FD,
                        DPI,
                        WIDTH,
                        HEIGHT,
                        null);

        // Passing only one valid coordinate must not mix explicit and bound IDs.
        mPrinter.print(explicitProcessId, -1, FD, DPI, WIDTH, HEIGHT, null);
        verify(mNativeMock)
                .print(
                        mWebContents,
                        BOUND_PROCESS_ID,
                        BOUND_FRAME_ID,
                        false,
                        FD,
                        DPI,
                        WIDTH,
                        HEIGHT,
                        null);

        mPrinter.finishPrint(explicitProcessId, explicitFrameId);
        verify(mNativeMock).finishPrint(mWebContents, explicitProcessId, explicitFrameId);
    }

    @Test
    public void testInitiatePrintFailureClearsBoundCoordinates() {
        when(mNativeMock.initiatePrint(mWebContents, -1, -1, false)).thenReturn(PACKED_BOUND_FRAME);
        assertTrue(mPrinter.initiatePrint(-1, -1));

        when(mNativeMock.initiatePrint(mWebContents, -1, -1, false)).thenReturn(-1L);
        assertFalse(mPrinter.initiatePrint(-1, -1));
        mPrinter.print(-1, -1, FD, DPI, WIDTH, HEIGHT, null);
        verify(mNativeMock).print(mWebContents, -1, -1, false, FD, DPI, WIDTH, HEIGHT, null);

        when(mNativeMock.initiatePrint(mWebContents, -1, -1, false)).thenReturn(PACKED_BOUND_FRAME);
        assertTrue(mPrinter.initiatePrint(-1, -1));
        when(mWebContents.isDestroyed()).thenReturn(true);
        assertFalse(mPrinter.initiatePrint(-1, -1));
        when(mWebContents.isDestroyed()).thenReturn(false);

        mPrinter.finishPrint(-1, -1);
        verify(mNativeMock).finishPrint(mWebContents, -1, -1);
    }
}
