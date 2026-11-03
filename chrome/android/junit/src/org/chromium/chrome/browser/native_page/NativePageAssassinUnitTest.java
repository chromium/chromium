// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.native_page;

import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;

/** Unit tests for {@link NativePageAssassin}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NativePageAssassinUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab1;
    @Mock private Tab mTab2;

    @Test
    public void testFreezeAllHiddenPages() {
        NativePageAssassin assassin = new NativePageAssassin();
        assassin.tabHidden(mTab1);
        assassin.tabHidden(mTab2);

        assassin.freezeAllHiddenPages();

        verify(mTab1).freezeNativePage();
        verify(mTab2).freezeNativePage();
    }

    @Test
    public void testSetDisableFreezeAllHiddenPagesForTesting() {
        NativePageAssassin assassin = new NativePageAssassin();
        NativePageAssassin.setDisableFreezeAllHiddenPagesForTesting(true);
        assassin.tabHidden(mTab1);
        assassin.tabHidden(mTab2);

        assassin.freezeAllHiddenPages();

        verify(mTab1, never()).freezeNativePage();
        verify(mTab2, never()).freezeNativePage();
    }
}
