// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.Mockito.doNothing;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/**
 * Unit tests for TabStateAttributes.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabStateAttributesTest {
    @Mock
    private WebContents mWebContents;
    @Mock
    private TabStateAttributes.Observer mAttributesObserver;

    @Captor
    ArgumentCaptor<WebContentsObserver> mWebContentsObserverCaptor;

    private MockTab mTab;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mTab = new MockTab(0, false) {
            @Override
            public WebContents getWebContents() {
                return mWebContents;
            };

            @Override
            public boolean isInitialized() {
                return true;
            }
        };
        doNothing().when(mWebContents).addObserver(mWebContentsObserverCaptor.capture());
    }

    @Test
    public void testDefaultDirtyState() {
        TabStateAttributes.createForTab(mTab, null);
        Assert.assertEquals(TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);

        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);

        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_FOR_LAZY_LOAD);
        Assert.assertEquals(TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);

        TabStateAttributes.createForTab(mTab, TabCreationState.LIVE_IN_BACKGROUND);
        Assert.assertEquals(TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);

        TabStateAttributes.createForTab(mTab, TabCreationState.LIVE_IN_FOREGROUND);
        Assert.assertEquals(TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);
    }

    @Test
    public void testTitleUpdate() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onTitleUpdated(mTab);

        Assert.assertEquals(TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
    }

    @Test
    public void testNavigationUpdates() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onContentChanged(mTab);
        WebContentsObserver webContentsObserver = mWebContentsObserverCaptor.getValue();
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);
        GURL testGURL = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);
        NavigationHandle navHandle = NavigationHandle.createForTesting(testGURL, false, 0, false);

        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        webContentsObserver.didFinishNavigationInPrimaryMainFrame(navHandle);
        Assert.assertEquals(TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
        Mockito.reset(mAttributesObserver);

        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.CLEAN);

        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onPageLoadFinished(mTab, testGURL);
        Assert.assertEquals(TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
        Mockito.reset(mAttributesObserver);

        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.CLEAN);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onLoadStopped(mTab, true);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verifyNoMoreInteractions(mAttributesObserver);
        Mockito.reset(mAttributesObserver);

        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.UNTIDY);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onLoadStopped(mTab, true);
        Assert.assertEquals(TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
    }

    @Test
    public void testHide() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onHidden(mTab, TabHidingType.CHANGED_TABS);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verifyNoMoreInteractions(mAttributesObserver);
        Mockito.reset(mAttributesObserver);

        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.UNTIDY);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onHidden(mTab, TabHidingType.CHANGED_TABS);
        Assert.assertEquals(TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
    }

    @Test
    public void testReparenting() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        // Detaching a tab does not mark a tab as needing to be saved.
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onActivityAttachmentChanged(mTab, null);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verifyNoMoreInteractions(mAttributesObserver);
        Mockito.reset(mAttributesObserver);

        WindowAndroid window = Mockito.mock(WindowAndroid.class);
        // Re-attaching a tab does mark a tab as needing to be saved.
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onActivityAttachmentChanged(mTab, window);
        Assert.assertEquals(TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
    }

    @Test
    public void testNavigationEntryUpdates() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onContentChanged(mTab);
        WebContentsObserver webContentsObserver = mWebContentsObserverCaptor.getValue();
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        webContentsObserver.navigationEntriesChanged();
        Assert.assertEquals(TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
        Mockito.reset(mAttributesObserver);

        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.CLEAN);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onNavigationEntriesDeleted(mTab);
        Assert.assertEquals(TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
    }

    @Test
    public void testRootIdUpdates() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        CriticalPersistedTabData.from(mTab).setRootId(12);
        Assert.assertEquals(TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
    }

    @Test
    public void testDuplicateUpdateCalls() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.CLEAN);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verifyNoMoreInteractions(mAttributesObserver);
        Mockito.reset(mAttributesObserver);

        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.UNTIDY);
        Assert.assertEquals(TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
        Mockito.reset(mAttributesObserver);

        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.UNTIDY);
        Assert.assertEquals(TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verifyNoMoreInteractions(mAttributesObserver);
        Mockito.reset(mAttributesObserver);

        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.DIRTY);
        Assert.assertEquals(TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
        Mockito.reset(mAttributesObserver);

        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.DIRTY);
        Assert.assertEquals(TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verifyNoMoreInteractions(mAttributesObserver);
        Mockito.reset(mAttributesObserver);

        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.CLEAN);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.CLEAN);
        Mockito.reset(mAttributesObserver);
        ;
    }
}
