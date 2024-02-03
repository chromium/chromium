// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/** Tests for {@link TabAttributes}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabAssociatedAppTest {
    private static final String APP_ID = "magicApp";

    @Mock private Tab mTab;

    @Captor ArgumentCaptor<TabObserver> mTabObserverCaptor;

    // Hosts the TabAssociatedApp
    private final UserDataHost mUserDataHost = new UserDataHost();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        doNothing().when(mTab).addObserver(mTabObserverCaptor.capture());
    }

    @Test
    @SmallTest
    public void testDisassociatesOnInputEvent() {
        TabAssociatedApp tabAssociatedApp = TabAssociatedApp.from(mTab);
        tabAssociatedApp.setAppId(APP_ID);
        Assert.assertEquals(APP_ID, tabAssociatedApp.getAppId());

        // Simulate an event (without going through input system)
        tabAssociatedApp.onImeEvent();
        Assert.assertNull(tabAssociatedApp.getAppId());
    }

    @Test
    @SmallTest
    public void testDisassociatesOnOmniboxPageLoad() {
        TabAssociatedApp tabAssociatedApp = TabAssociatedApp.from(mTab);
        mTabObserverCaptor.getValue().onInitialized(mTab, APP_ID);
        Assert.assertEquals(APP_ID, tabAssociatedApp.getAppId());

        mTabObserverCaptor
                .getValue()
                .onLoadUrl(
                        mTab,
                        new LoadUrlParams("foobar.com", PageTransition.FROM_ADDRESS_BAR),
                        new LoadUrlResult(Tab.TabLoadStatus.DEFAULT_PAGE_LOAD, null));

        Assert.assertNull(tabAssociatedApp.getAppId());
    }

    @Test
    @SmallTest
    public void testDoesNotDisassociateOnNormalPageLoad() {
        TabAssociatedApp tabAssociatedApp = TabAssociatedApp.from(mTab);
        mTabObserverCaptor.getValue().onInitialized(mTab, APP_ID);
        Assert.assertEquals(APP_ID, tabAssociatedApp.getAppId());

        mTabObserverCaptor
                .getValue()
                .onLoadUrl(
                        mTab,
                        new LoadUrlParams("foobar.com", PageTransition.LINK),
                        new LoadUrlResult(Tab.TabLoadStatus.DEFAULT_PAGE_LOAD, null));

        Assert.assertEquals(APP_ID, tabAssociatedApp.getAppId());
    }
}
