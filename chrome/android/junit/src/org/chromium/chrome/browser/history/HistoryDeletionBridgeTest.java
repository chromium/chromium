// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for HistoryDeletionBridge. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HistoryDeletionBridgeTest {
    @Rule public JniMocker mocker = new JniMocker();

    @Mock HistoryDeletionBridge.Natives mNativeMocks;

    @Mock HistoryDeletionBridge.Observer mHistoryDeletionBridgeObserverOne;

    @Mock HistoryDeletionBridge.Observer mHistoryDeletionBridgeObserverTwo;

    @Mock HistoryDeletionInfo mHistoryDeletionInfo;

    @Mock Profile mProfile;

    HistoryDeletionBridge mHistoryDeletionBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(HistoryDeletionBridgeJni.TEST_HOOKS, mNativeMocks);
        mHistoryDeletionBridge = new HistoryDeletionBridge(mProfile);
    }

    @Test
    public void testObserverMethodsCalled() {
        mHistoryDeletionBridge.addObserver(mHistoryDeletionBridgeObserverOne);
        mHistoryDeletionBridge.addObserver(mHistoryDeletionBridgeObserverTwo);
        mHistoryDeletionBridge.onURLsDeleted(mHistoryDeletionInfo);

        Mockito.verify(mHistoryDeletionBridgeObserverOne).onURLsDeleted(mHistoryDeletionInfo);
        Mockito.verify(mHistoryDeletionBridgeObserverTwo).onURLsDeleted(mHistoryDeletionInfo);
    }

    @Test
    public void testObserverRemovedDoesNotCall() {
        mHistoryDeletionBridge.addObserver(mHistoryDeletionBridgeObserverOne);
        mHistoryDeletionBridge.addObserver(mHistoryDeletionBridgeObserverTwo);
        mHistoryDeletionBridge.removeObserver(mHistoryDeletionBridgeObserverOne);
        mHistoryDeletionBridge.onURLsDeleted(mHistoryDeletionInfo);

        Mockito.verify(mHistoryDeletionBridgeObserverOne, Mockito.never())
                .onURLsDeleted(mHistoryDeletionInfo);
        Mockito.verify(mHistoryDeletionBridgeObserverTwo).onURLsDeleted(mHistoryDeletionInfo);
    }
}
