// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ObserverList;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TabBrowserControlsOffsetHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabBrowserControlsOffsetHelperTest {
    private final UserDataHost mUserDataHost = new UserDataHost();

    @Mock public TabImpl mTab;
    @Mock public TabObserver mDispatchedTabObserver;

    private TabBrowserControlsOffsetHelper mHelper;
    private TabObserver mRegisteredTabObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Mockito.when(mTab.getUserDataHost()).thenReturn(mUserDataHost);

        ObserverList<TabObserver> observers = new ObserverList<>();
        observers.addObserver(mDispatchedTabObserver);
        Mockito.when(mTab.getTabObservers())
                .thenAnswer(invocation -> observers.rewindableIterator());

        ArgumentCaptor<TabObserver> observerArg = ArgumentCaptor.forClass(TabObserver.class);
        mHelper = TabBrowserControlsOffsetHelper.get(mTab);
        Mockito.verify(mTab).addObserver(observerArg.capture());
        mRegisteredTabObserver = observerArg.getValue();

        Assert.assertFalse(mHelper.offsetInitialized());
    }

    @Test
    public void testSetTopOffset() {
        int bottomValue = mHelper.bottomControlsOffset();

        mHelper.setOffsets(20, 50, 0, 0, 0);
        Mockito.verify(mDispatchedTabObserver)
                .onBrowserControlsOffsetChanged(mTab, 20, bottomValue, 50, 0, 0);
        Assert.assertTrue(mHelper.offsetInitialized());
        Assert.assertEquals(20, mHelper.topControlsOffset());
        Assert.assertEquals(50, mHelper.contentOffset());
        Assert.assertEquals(bottomValue, mHelper.bottomControlsOffset());

        // Different top offset, different content offset.
        mHelper.setOffsets(25, 55, 0, 0, 0);
        Mockito.verify(mDispatchedTabObserver)
                .onBrowserControlsOffsetChanged(mTab, 25, bottomValue, 55, 0, 0);
        Assert.assertTrue(mHelper.offsetInitialized());
        Assert.assertEquals(25, mHelper.topControlsOffset());
        Assert.assertEquals(55, mHelper.contentOffset());
        Assert.assertEquals(bottomValue, mHelper.bottomControlsOffset());

        // Different top offset, same content offset.
        mHelper.setOffsets(40, 55, 0, 0, 0);
        Mockito.verify(mDispatchedTabObserver)
                .onBrowserControlsOffsetChanged(mTab, 40, bottomValue, 55, 0, 0);
        Assert.assertTrue(mHelper.offsetInitialized());
        Assert.assertEquals(40, mHelper.topControlsOffset());
        Assert.assertEquals(55, mHelper.contentOffset());
        Assert.assertEquals(bottomValue, mHelper.bottomControlsOffset());

        // Same top offset, different content offset.
        mHelper.setOffsets(40, 60, 0, 0, 0);
        Mockito.verify(mDispatchedTabObserver)
                .onBrowserControlsOffsetChanged(mTab, 40, bottomValue, 60, 0, 0);
        Assert.assertTrue(mHelper.offsetInitialized());
        Assert.assertEquals(40, mHelper.topControlsOffset());
        Assert.assertEquals(60, mHelper.contentOffset());
        Assert.assertEquals(bottomValue, mHelper.bottomControlsOffset());

        // Same top offset, same content offset.  Duplicate values should not dispatch additional
        // change notifications.
        mHelper.setOffsets(40, 60, 0, 0, 0);
        Mockito.verifyNoMoreInteractions(mDispatchedTabObserver);
        Assert.assertTrue(mHelper.offsetInitialized());
        Assert.assertEquals(40, mHelper.topControlsOffset());
        Assert.assertEquals(60, mHelper.contentOffset());
        Assert.assertEquals(bottomValue, mHelper.bottomControlsOffset());
    }

    @Test
    public void testSetBottomOffset() {
        int topValue = mHelper.topControlsOffset();
        int contentValue = mHelper.contentOffset();

        mHelper.setOffsets(topValue, contentValue, 0, 37, 0);
        Mockito.verify(mDispatchedTabObserver)
                .onBrowserControlsOffsetChanged(mTab, topValue, 37, contentValue, 0, 0);
        Assert.assertTrue(mHelper.offsetInitialized());
        Assert.assertEquals(topValue, mHelper.topControlsOffset());
        Assert.assertEquals(contentValue, mHelper.contentOffset());
        Assert.assertEquals(37, mHelper.bottomControlsOffset());

        // Different bottom offset.
        mHelper.setOffsets(topValue, contentValue, 0, 42, 0);
        Mockito.verify(mDispatchedTabObserver)
                .onBrowserControlsOffsetChanged(mTab, topValue, 42, contentValue, 0, 0);
        Assert.assertTrue(mHelper.offsetInitialized());
        Assert.assertEquals(topValue, mHelper.topControlsOffset());
        Assert.assertEquals(contentValue, mHelper.contentOffset());
        Assert.assertEquals(42, mHelper.bottomControlsOffset());

        // Same bottom offset.  Duplicate values should not dispatch additional change
        // notifications.
        mHelper.setOffsets(topValue, contentValue, 0, 42, 0);
        Mockito.verifyNoMoreInteractions(mDispatchedTabObserver);
        Assert.assertTrue(mHelper.offsetInitialized());
        Assert.assertEquals(topValue, mHelper.topControlsOffset());
        Assert.assertEquals(contentValue, mHelper.contentOffset());
        Assert.assertEquals(42, mHelper.bottomControlsOffset());
    }

    @Test
    public void testTabCrashed() {
        mHelper.setOffsets(11, 12, 0, 13, 0);
        Mockito.verify(mDispatchedTabObserver)
                .onBrowserControlsOffsetChanged(mTab, 11, 13, 12, 0, 0);
        Assert.assertTrue(mHelper.offsetInitialized());
        Assert.assertEquals(11, mHelper.topControlsOffset());
        Assert.assertEquals(12, mHelper.contentOffset());
        Assert.assertEquals(13, mHelper.bottomControlsOffset());

        mRegisteredTabObserver.onCrash(mTab);
        Assert.assertFalse(mHelper.offsetInitialized());
        Assert.assertEquals(0, mHelper.topControlsOffset());
        Assert.assertEquals(0, mHelper.contentOffset());
        Assert.assertEquals(0, mHelper.bottomControlsOffset());
    }
}
