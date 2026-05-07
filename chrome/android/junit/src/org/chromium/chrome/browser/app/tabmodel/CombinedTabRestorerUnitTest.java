// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

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
import org.chromium.chrome.browser.tab.StorageLoadedData;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Unit tests for {@link CombinedTabRestorer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class CombinedTabRestorerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CombinedTabRestorer.CombinedTabRestorerDelegate mDelegate;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private StorageLoadedData mStorageLoadedData;

    private CombinedTabRestorer mRestorer;

    @Before
    public void setUp() {
        when(mStorageLoadedData.getLoadedTabStates())
                .thenReturn(new StorageLoadedData.LoadedTabState[0]);
    }

    private CombinedTabRestorer createRestorer(
            boolean restoreIncognitoTabs, boolean restoreRegularTabs) {
        return new CombinedTabRestorer(
                restoreIncognitoTabs,
                restoreRegularTabs,
                mDelegate,
                mTabCreatorManager,
                () -> null,
                mTabModelSelector,
                /* logRestoreDuration= */ false,
                /* isFromRecreating= */ false);
    }

    @Test
    public void testRestoreNone() {
        mRestorer = createRestorer(/* restoreIncognitoTabs= */ false, /* restoreRegularTabs= */ false);
    }

    @Test
    public void testRestoreOnlyIncognito() {
        mRestorer = createRestorer(/* restoreIncognitoTabs= */ true, /* restoreRegularTabs= */ false);

        mRestorer.onDataLoaded(mStorageLoadedData, /* incognito= */ true);

        verify(mDelegate).onLoadFinished(/* loadedTabCount= */ anyInt());
    }

    @Test
    public void testRestoreOnlyRegular() {
        mRestorer = createRestorer(/* restoreIncognitoTabs= */ false, /* restoreRegularTabs= */ true);

        mRestorer.onDataLoaded(mStorageLoadedData, /* incognito= */ false);

        verify(mDelegate).onLoadFinished(/* loadedTabCount= */ anyInt());
    }

    @Test
    public void testRestoreBoth() {
        mRestorer = createRestorer(/* restoreIncognitoTabs= */ true, /* restoreRegularTabs= */ true);

        mRestorer.onDataLoaded(mStorageLoadedData, /* incognito= */ false);
        verify(mDelegate, never()).onLoadFinished(/* loadedTabCount= */ anyInt());

        mRestorer.onDataLoaded(mStorageLoadedData, /* incognito= */ true);
        verify(mDelegate).onLoadFinished(/* loadedTabCount= */ anyInt());
    }
}
