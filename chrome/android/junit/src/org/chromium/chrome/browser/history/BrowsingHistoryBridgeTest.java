// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;

/** Unit tests for BrowsingHistoryBridge. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BrowsingHistoryBridgeTest {
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    BrowsingHistoryBridge.Natives mNativeMocks;

    @Mock
    private Profile mProfile;

    BrowsingHistoryBridge mBrowsingHistoryBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(BrowsingHistoryBridgeJni.TEST_HOOKS, mNativeMocks);
        mBrowsingHistoryBridge = new BrowsingHistoryBridge(mProfile);
    }

    @Test
    public void testWhenDeletingBrowsingHistoryItems_MetricsEmitted() {
        HistogramWatcher histogramWatcher = HistogramWatcher.newSingleRecordWatcher(
                "Privacy.DeleteBrowsingData.Action", DeleteBrowsingDataAction.HISTORY_PAGE_ENTRIES);
        mBrowsingHistoryBridge.removeItems();

        // Verify DeleteBrowsingDataAction metric is recorded.
        histogramWatcher.assertExpected();
    }
}
