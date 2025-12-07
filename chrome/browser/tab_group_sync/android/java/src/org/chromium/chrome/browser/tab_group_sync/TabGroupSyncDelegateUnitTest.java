// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncDelegate.Deps;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

/** Unit tests for the {@link TabGroupSyncDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupSyncDelegateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    private TabGroupSyncDelegate mDelegate;
    private @Mock TabWindowManager mTabWindowManager;

    @Before
    public void setUp() {
        TabGroupSyncDelegate.Deps deps = new Deps(mTabWindowManager);
        mDelegate = TabGroupSyncDelegate.create(5, deps);
        verify(mTabWindowManager).addObserver(eq(mDelegate));
    }

    @After
    public void tearDown() {
        mDelegate.destroy();
        verify(mTabWindowManager).removeObserver(eq(mDelegate));
    }

    @Test
    public void testBasic() {}
}
