// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;

/** Unit tests for {@link TabListObserverManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListObserverManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabListLayoutDelegate mLayoutDelegate;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab;

    private TabListObserverManager mObserverManager;

    @Before
    public void setUp() {
        mObserverManager = new TabListObserverManager(mLayoutDelegate);
    }

    @Test
    public void testAddAndRemoveTabGroupObserver() {
        mObserverManager.addTabGroupObserver(mTabModel);
        verify(mTabModel).addTabGroupObserver(mLayoutDelegate);

        mObserverManager.removeTabGroupObserver(mTabModel);
        verify(mTabModel).removeTabGroupObserver(mLayoutDelegate);
    }

    @Test
    public void testRemoveTabGroupObserver_NullTabModel() {
        mObserverManager.removeTabGroupObserver(null);
        verifyNoInteractions(mLayoutDelegate);
    }

    @Test
    public void testAddAndRemoveTabObserver() {
        mObserverManager.addTabObserver(mTab);
        verify(mTab).addObserver(mLayoutDelegate);

        mObserverManager.removeTabObserver(mTab);
        verify(mTab).removeObserver(mLayoutDelegate);
    }

    @Test
    public void testRemoveTabObserver_NullTab() {
        mObserverManager.removeTabObserver(null);
        verifyNoInteractions(mLayoutDelegate);
    }

    @Test
    public void testDestroy() {
        mObserverManager.addTabObserver(mTab);
        mObserverManager.addTabGroupObserver(mTabModel);

        mObserverManager.destroy();

        verify(mTab).removeObserver(mLayoutDelegate);
        verify(mTabModel).removeTabGroupObserver(mLayoutDelegate);
    }
}
