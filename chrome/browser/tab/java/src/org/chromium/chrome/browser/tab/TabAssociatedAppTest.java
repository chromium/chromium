// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.content_public.browser.ImeAdapter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;

import java.util.concurrent.atomic.AtomicReference;

/** Tests for {@link TabAttributes}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabAssociatedAppTest {
    private static final String APP_ID = "magicApp";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;

    @Captor ArgumentCaptor<TabObserver> mTabObserverCaptor;

    // Hosts the TabAssociatedApp
    private final UserDataHost mUserDataHost = new UserDataHost();

    @Before
    public void setUp() {
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        doNothing().when(mTab).addObserver(mTabObserverCaptor.capture());
    }

    @Test
    public void testDisassociatesOnInputEvent() {
        TabAssociatedApp tabAssociatedApp = TabAssociatedApp.from(mTab);
        tabAssociatedApp.setAppId(APP_ID);
        Assert.assertEquals(APP_ID, tabAssociatedApp.getAppId());

        // Simulate an event (without going through input system)
        tabAssociatedApp.onImeEvent();
        Assert.assertNull(tabAssociatedApp.getAppId());
    }

    @Test
    public void testDisassociatesOnOmniboxPageLoad() {
        TabAssociatedApp tabAssociatedApp = TabAssociatedApp.from(mTab);
        tabAssociatedApp.setAppId(APP_ID);
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
    public void testDoesNotDisassociateOnNormalPageLoad() {
        TabAssociatedApp tabAssociatedApp = TabAssociatedApp.from(mTab);
        tabAssociatedApp.setAppId(APP_ID);
        Assert.assertEquals(APP_ID, tabAssociatedApp.getAppId());

        mTabObserverCaptor
                .getValue()
                .onLoadUrl(
                        mTab,
                        new LoadUrlParams("foobar.com", PageTransition.LINK),
                        new LoadUrlResult(Tab.TabLoadStatus.DEFAULT_PAGE_LOAD, null));

        Assert.assertEquals(APP_ID, tabAssociatedApp.getAppId());
    }

    @Test
    public void testImeObserverAttachedOnDemandWhenAppIdSet() {
        when(mTab.getLaunchType()).thenReturn(TabLaunchType.FROM_EXTERNAL_APP);
        Assert.assertTrue(TabAssociatedApp.isOpenedFromExternalApp(mTab));
        Assert.assertNull(TabAssociatedApp.getAppId(mTab));
        verify(mTab, never()).addObserver(any());

        AtomicReference<ImeAdapter> imeAdapter1Ref = stubImeAdapterLookup(mWebContents);

        // Attach while WebContents is initially null, set app ID, then swap in mWebContents.
        TabAssociatedApp tabAssociatedApp = TabAssociatedApp.from(mTab);
        tabAssociatedApp.setAppId(APP_ID);
        Assert.assertNull(imeAdapter1Ref.get());

        when(mTab.getWebContents()).thenReturn(mWebContents);
        mTabObserverCaptor.getValue().onContentChanged(mTab);
        Assert.assertNotNull(imeAdapter1Ref.get());
        verify(imeAdapter1Ref.get()).addEventObserver(tabAssociatedApp);

        // Omnibox navigation clears mId and detaches the IME observer immediately.
        mTabObserverCaptor
                .getValue()
                .onLoadUrl(
                        mTab,
                        new LoadUrlParams("foobar.com", PageTransition.FROM_ADDRESS_BAR),
                        new LoadUrlResult(Tab.TabLoadStatus.DEFAULT_PAGE_LOAD, null));
        Assert.assertNull(tabAssociatedApp.getAppId());
        verify(imeAdapter1Ref.get()).removeEventObserver(tabAssociatedApp);

        // Re-setting app ID re-attaches the IME observer, and swapping WebContents migrates it.
        tabAssociatedApp.setAppId(APP_ID);
        verify(imeAdapter1Ref.get(), times(2)).addEventObserver(tabAssociatedApp);

        WebContents webContents2 = mock(WebContents.class);
        AtomicReference<ImeAdapter> imeAdapter2Ref = stubImeAdapterLookup(webContents2);
        when(mTab.getWebContents()).thenReturn(webContents2);
        mTabObserverCaptor.getValue().onContentChanged(mTab);

        verify(imeAdapter1Ref.get(), times(2)).removeEventObserver(tabAssociatedApp);
        Assert.assertNotNull(imeAdapter2Ref.get());
        verify(imeAdapter2Ref.get()).addEventObserver(tabAssociatedApp);
    }

    private static AtomicReference<ImeAdapter> stubImeAdapterLookup(WebContents webContents) {
        AtomicReference<ImeAdapter> imeAdapterRef = new AtomicReference<>();
        when(webContents.getOrSetUserData(any(), any()))
                .thenAnswer(
                        inv -> {
                            if (imeAdapterRef.get() == null) {
                                Class<? extends UserData> key = inv.getArgument(0);
                                imeAdapterRef.set((ImeAdapter) mock(key));
                            }
                            return (UserData) imeAdapterRef.get();
                        });
        return imeAdapterRef;
    }
}
