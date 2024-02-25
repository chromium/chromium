// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.findinpage;

import android.view.View;
import android.view.ViewStub;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.WindowAndroid;

/** Test for {@link FindToolbarManagerTest}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FindToolbarManagerTest {
    private FindToolbarManager mFindToolbarManager;

    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;
    @Mock private ViewStub mViewStub;
    @Mock private FindToolbar mFindToolbar;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Mockito.doReturn(mTab).when(mTabModelSelector).getCurrentTab();
        Mockito.doReturn(mFindToolbar).when(mViewStub).inflate();

        mFindToolbarManager =
                new FindToolbarManager(
                        mViewStub,
                        mTabModelSelector,
                        Mockito.mock(WindowAndroid.class),
                        null,
                        null);
    }

    @Test
    public void testObserverMethods() {
        FindToolbarObserver observer1 = Mockito.mock(FindToolbarObserver.class);
        FindToolbarObserver observer2 = Mockito.mock(FindToolbarObserver.class);

        mFindToolbarManager.addObserver(observer1);
        mFindToolbarManager.addObserver(observer2);
        mFindToolbarManager.showToolbar();

        ArgumentCaptor<FindToolbarObserver> captor =
                ArgumentCaptor.forClass(FindToolbarObserver.class);
        Mockito.verify(mFindToolbar).setObserver(captor.capture());

        FindToolbarObserver aggObserver = captor.getValue();
        aggObserver.onFindToolbarHidden();
        Mockito.verify(observer1).onFindToolbarHidden();
        Mockito.verify(observer2).onFindToolbarHidden();
        aggObserver.onFindToolbarShown();
        Mockito.verify(observer1).onFindToolbarShown();
        Mockito.verify(observer2).onFindToolbarShown();

        mFindToolbarManager.removeObserver(observer2);
        aggObserver.onFindToolbarHidden();
        Mockito.verify(observer1, Mockito.times(2)).onFindToolbarHidden();
        Mockito.verify(observer2, Mockito.times(1)).onFindToolbarHidden();
        aggObserver.onFindToolbarShown();
        Mockito.verify(observer1, Mockito.times(2)).onFindToolbarShown();
        Mockito.verify(observer2, Mockito.times(1)).onFindToolbarShown();
    }

    @Test
    public void testIsShowing() {
        Assert.assertFalse(mFindToolbarManager.isShowing());

        mFindToolbarManager.showToolbar();
        Mockito.doReturn(View.GONE).when(mFindToolbar).getVisibility();
        Assert.assertFalse(mFindToolbarManager.isShowing());

        Mockito.doReturn(View.VISIBLE).when(mFindToolbar).getVisibility();
        Assert.assertTrue(mFindToolbarManager.isShowing());
    }

    @Test
    public void testSetFindQuery() {
        mFindToolbarManager.showToolbar();
        mFindToolbarManager.setFindQuery("foo");
        Mockito.verify(mFindToolbar).setFindQuery("foo");
    }
}
