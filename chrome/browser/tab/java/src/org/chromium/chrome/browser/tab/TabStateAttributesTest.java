// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.Mockito.doNothing;

import android.os.Handler;
import android.os.Looper;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.TimeUnit;

/**
 * Unit tests for TabStateAttributes.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowLooper.class, ShadowPostTask.class})
public class TabStateAttributesTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

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
        ShadowPostTask.setTestImpl(new ShadowPostTask.TestImpl() {
            @Override
            public void postDelayedTask(@TaskTraits int taskTraits, Runnable task, long delay) {
                new Handler(Looper.getMainLooper()).postDelayed(task, delay);
            }
        });

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

    @After
    public void tearDown() {
        ShadowPostTask.reset();
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
    public void testDefaultShouldSave() {
        TabStateAttributes.createForTab(mTab, null);
        Assert.assertTrue(CriticalPersistedTabData.from(mTab).getShouldSaveForTesting());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);
        mTab.getUserDataHost().removeUserData(CriticalPersistedTabData.class);

        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_FOR_LAZY_LOAD);
        Assert.assertTrue(CriticalPersistedTabData.from(mTab).getShouldSaveForTesting());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);
        mTab.getUserDataHost().removeUserData(CriticalPersistedTabData.class);

        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        Assert.assertFalse(CriticalPersistedTabData.from(mTab).getShouldSaveForTesting());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);
        mTab.getUserDataHost().removeUserData(CriticalPersistedTabData.class);

        TabStateAttributes.createForTab(mTab, TabCreationState.LIVE_IN_BACKGROUND);
        Assert.assertFalse(CriticalPersistedTabData.from(mTab).getShouldSaveForTesting());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);
        mTab.getUserDataHost().removeUserData(CriticalPersistedTabData.class);

        TabStateAttributes.createForTab(mTab, TabCreationState.LIVE_IN_FOREGROUND);
        Assert.assertFalse(CriticalPersistedTabData.from(mTab).getShouldSaveForTesting());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);
        mTab.getUserDataHost().removeUserData(CriticalPersistedTabData.class);
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
    public void testFinishMainFrameNavigation() {
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
    }

    @Test
    public void testPageLoadFinished() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);
        GURL testGURL = JUnitTestGURLs.getGURL(JUnitTestGURLs.EXAMPLE_URL);

        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        while (observers.hasNext()) observers.next().onPageLoadFinished(mTab, testGURL);
        Assert.assertEquals(TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
    }

    @Test
    public void testLoadStopped_DifferentDocument() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        while (observers.hasNext()) {
            observers.next().onLoadStopped(mTab, /* toDifferentDocument */ true);
        }
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verifyNoMoreInteractions(mAttributesObserver);
        Mockito.reset(mAttributesObserver);

        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.UNTIDY);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) {
            observers.next().onLoadStopped(mTab, /* toDifferentDocument */ true);
        }
        Assert.assertEquals(TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
    }

    @Test
    public void testLoadStopped_SameDocument() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        while (observers.hasNext()) {
            observers.next().onLoadStopped(mTab, /* toDifferentDocument */ false);
        }
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verifyNoMoreInteractions(mAttributesObserver);
        Mockito.reset(mAttributesObserver);

        ShadowLooper.idleMainLooper();
        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.UNTIDY);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) {
            observers.next().onLoadStopped(mTab, /* toDifferentDocument */ false);
        }
        Assert.assertEquals(TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Assert.assertEquals(1, Robolectric.getForegroundThreadScheduler().size());

        // An additional call to onLoadStopped should not change the state, nor should another
        // task be queued.
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) {
            observers.next().onLoadStopped(mTab, /* toDifferentDocument */ false);
        }
        Assert.assertEquals(TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Assert.assertEquals(1, Robolectric.getForegroundThreadScheduler().size());

        Robolectric.getForegroundThreadScheduler().advanceBy(
                TabStateAttributes.DEFAULT_LOW_PRIORITY_SAVE_DELAY_MS, TimeUnit.MILLISECONDS);
        Assert.assertEquals(TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
        Assert.assertEquals(0, Robolectric.getForegroundThreadScheduler().size());
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

        // If a tab is not closing, then hiding the tab should mark it as dirty.
        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.UNTIDY);
        mTab.setClosing(false);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onHidden(mTab, TabHidingType.CHANGED_TABS);
        Assert.assertEquals(TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);

        // If a tab is closing, then hiding the tab should not mark it as dirty.
        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.CLEAN);
        mTab.setClosing(true);
        while (observers.hasNext()) observers.next().onHidden(mTab, TabHidingType.CHANGED_TABS);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verifyNoMoreInteractions(mAttributesObserver);
        Mockito.reset(mAttributesObserver);
    }

    @Test
    public void testUndoClosingCommitsDirtiness() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onClosingStateChanged(mTab, false);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        Mockito.verifyNoMoreInteractions(mAttributesObserver);
        Mockito.reset(mAttributesObserver);

        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.UNTIDY);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onClosingStateChanged(mTab, false);
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
    }

    @Test
    public void testUpdatesIgnoredDuringRestore() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.CLEAN);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        mTab.setIsBeingRestored(true);
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.DIRTY);
        Assert.assertEquals(TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        mTab.setIsBeingRestored(false);
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.DIRTY);
        Assert.assertEquals(TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
    }
}
