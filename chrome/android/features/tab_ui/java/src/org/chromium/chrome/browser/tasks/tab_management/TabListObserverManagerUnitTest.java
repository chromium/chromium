// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabModel;

/** Unit tests for {@link TabListObserverManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabListObserverManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabListLayoutDelegate mLayoutDelegate;
    @Mock private TabModel mTabModel;

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
}
