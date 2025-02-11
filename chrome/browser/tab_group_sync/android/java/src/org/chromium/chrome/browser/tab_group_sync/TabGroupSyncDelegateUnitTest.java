// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for the {@link TabGroupSyncDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupSyncDelegateUnitTest {
    private TabGroupSyncDelegate mDelegate;

    @Before
    public void setUp() {
        mDelegate = TabGroupSyncDelegate.create(5);
    }

    @After
    public void tearDown() {
        mDelegate.destroy();
    }

    @Test
    public void testBasic() {}
}
