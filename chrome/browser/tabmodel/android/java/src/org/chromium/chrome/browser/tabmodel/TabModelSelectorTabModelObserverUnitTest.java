// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Tests for the TabModelSelectorTabModelObserver. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabModelSelectorTabModelObserverUnitTest {
    @Mock private TabModelSelector mSelector;

    @Mock private TabModel mTabModel;

    private List<TabModel> mTabModels = new ArrayList<>();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mTabModels = new ArrayList<>();
        doReturn(mTabModels).when(mSelector).getModels();
    }

    @Test
    @SmallTest
    public void testAlreadyInitializedSelector() throws TimeoutException {
        // ARRANGE
        mTabModels.add(mTabModel);
        ArgumentCaptor<TabModelSelectorTabModelObserver> arg1 =
                ArgumentCaptor.forClass(TabModelSelectorTabModelObserver.class);

        // ACT
        final CallbackHelper registrationCompleteCallback = new CallbackHelper();
        TabModelSelectorTabModelObserver observer =
                new TabModelSelectorTabModelObserver(mSelector) {
                    @Override
                    protected void onRegistrationComplete() {
                        registrationCompleteCallback.notifyCalled();
                    }
                };

        // ASSERT
        registrationCompleteCallback.waitForCallback(0);
        verify(mTabModel).addObserver(arg1.capture());
        assertEquals(1, mSelector.getModels().size());
        assertSame(observer, arg1.getValue());
    }

    @Test
    @SmallTest
    public void testUninitializedSelector() throws TimeoutException {
        // ARRANGE
        ArgumentCaptor<TabModelSelectorObserver> arg1 =
                ArgumentCaptor.forClass(TabModelSelectorObserver.class);
        ArgumentCaptor<TabModelSelectorTabModelObserver> arg2 =
                ArgumentCaptor.forClass(TabModelSelectorTabModelObserver.class);

        // ACT
        final CallbackHelper registrationCompleteCallback = new CallbackHelper();
        TabModelSelectorTabModelObserver observer =
                new TabModelSelectorTabModelObserver(mSelector) {
                    @Override
                    protected void onRegistrationComplete() {
                        registrationCompleteCallback.notifyCalled();
                    }
                };
        mTabModels.add(mTabModel); // Ensure a (any) tab model is added after initialization.
        verify(mSelector).addObserver(arg1.capture());
        arg1.getValue().onChange();

        // ASSERT
        registrationCompleteCallback.waitForCallback(0);
        verify(mTabModel).addObserver(arg2.capture());
        assertEquals(1, mSelector.getModels().size());
        assertSame(observer, arg2.getValue());
    }

    @Test
    @SmallTest
    public void testDestroySelector() {
        // ARRANGE
        ArgumentCaptor<TabModelSelectorObserver> arg1 =
                ArgumentCaptor.forClass(TabModelSelectorObserver.class);
        TabModelSelectorTabModelObserver observer = new TabModelSelectorTabModelObserver(mSelector);

        // ACT
        verify(mSelector).addObserver(arg1.capture());
        mTabModels.add(mTabModel);
        observer.destroy();

        // ASSERT
        verify(mSelector).removeObserver(arg1.getValue());
        verify(mTabModel).removeObserver(observer);
    }
}
