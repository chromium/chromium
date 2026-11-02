// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.provider;

import static org.junit.Assert.assertEquals;
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
import org.chromium.chrome.browser.TabDiscardState;
import org.chromium.chrome.browser.TabDiscardStateJni;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.io.StringWriter;

/** Unit tests for {@link ChromeBrowserProviderImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeBrowserProviderImplUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabDiscardState.Natives mNativeMock;
    @Mock private ChromeBrowserInitializer mBrowserInitializer;

    private ChromeBrowserProvider mProvider;

    @Before
    public void setUp() {
        mProvider = new ChromeBrowserProvider();
        TabDiscardStateJni.setInstanceForTesting(mNativeMock);
        ChromeBrowserInitializer.setForTesting(mBrowserInitializer);
    }

    @Test
    public void testDumpWhenBrowserNotStarted() {
        when(mBrowserInitializer.isFullBrowserInitialized()).thenReturn(false);

        StringWriter stringWriter = new StringWriter();
        PrintWriter printWriter = new PrintWriter(stringWriter);
        mProvider.dump(new FileDescriptor(), printWriter, new String[0]);

        assertEquals("Browser not started.\n", stringWriter.toString());
        verify(mNativeMock, never()).getTabDiscardsDump();
    }

    @Test
    public void testDumpWhenBrowserStarted() {
        when(mBrowserInitializer.isFullBrowserInitialized()).thenReturn(true);
        String expectedDump =
                "Tab Discards Info (1 tabs):\n"
                        + "  tab_id=1 visibility=hidden loading_state=loaded"
                        + " lifecycle_state=hidden can_discard=true cannot_discard_reasons=[]"
                        + " last_active=10s main_frame_pid=1234 subframe_pids=[5678]\n";
        when(mNativeMock.getTabDiscardsDump()).thenReturn(expectedDump);

        StringWriter stringWriter = new StringWriter();
        PrintWriter printWriter = new PrintWriter(stringWriter);
        mProvider.dump(new FileDescriptor(), printWriter, new String[0]);

        assertEquals(expectedDump, stringWriter.toString());
    }
}
