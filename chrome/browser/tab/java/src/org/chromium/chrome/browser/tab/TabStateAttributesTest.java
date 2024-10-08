// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.os.Handler;
import android.os.Looper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.Token;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.TimeUnit;

/** Unit tests for TabStateAttributes. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowLooper.class, ShadowPostTask.class})
public class TabStateAttributesTest {
    @Rule public final MockitoRule mockito = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private WebContents mWebContents;
    @Mock private TabStateAttributes.Observer mAttributesObserver;

    @Captor ArgumentCaptor<WebContentsObserver> mWebContentsObserverCaptor;

    private MockTab mTab;

    @Before
    public void setUp() {
        ShadowPostTask.setTestImpl(
                (@TaskTraits int taskTraits, Runnable task, long delay) -> {
                    new Handler(Looper.getMainLooper()).postDelayed(task, delay);
                });

        mTab =
                new MockTab(0, mProfile) {
                    @Override
                    public WebContents getWebContents() {
                        return mWebContents;
                    }

                    @Override
                    public boolean isInitialized() {
                        return true;
                    }
                };
        mTab.setCanGoForward(false);
        mTab.setCanGoBack(false);

        doNothing().when(mWebContents).addObserver(mWebContentsObserverCaptor.capture());
    }

    @Test
    public void testDefaultDirtyState() {
        TabStateAttributes.createForTab(mTab, null);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);

        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);

        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_FOR_LAZY_LOAD);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);

        TabStateAttributes.createForTab(mTab, TabCreationState.LIVE_IN_BACKGROUND);
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);

        TabStateAttributes.createForTab(mTab, TabCreationState.LIVE_IN_FOREGROUND);
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        mTab.getUserDataHost().removeUserData(TabStateAttributes.class);
    }

    @Test
    public void testTitleUpdate() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onTitleUpdated(mTab);

        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
    }

    @Test
    public void testFinishMainFrameNavigation() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onContentChanged(mTab);
        WebContentsObserver webContentsObserver = mWebContentsObserverCaptor.getValue();
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);
        GURL testGURL = JUnitTestGURLs.EXAMPLE_URL;
        NavigationHandle navHandle = NavigationHandle.createForTesting(testGURL, false, 0, false);

        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        webContentsObserver.didFinishNavigationInPrimaryMainFrame(navHandle);
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
    }

    @Test
    public void testPageLoadFinished() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);
        GURL testGURL = JUnitTestGURLs.EXAMPLE_URL;

        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        while (observers.hasNext()) observers.next().onPageLoadFinished(mTab, testGURL);
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
    }

    @Test
    public void testLoadStopped_DifferentDocument() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        while (observers.hasNext()) {
            observers.next().onLoadStopped(mTab, /* toDifferentDocument= */ true);
        }
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.UNTIDY);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) {
            observers.next().onLoadStopped(mTab, /* toDifferentDocument= */ true);
        }
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
    }

    @Test
    public void testLoadStopped_SameDocument() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        while (observers.hasNext()) {
            observers.next().onLoadStopped(mTab, /* toDifferentDocument= */ false);
        }
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        ShadowLooper.idleMainLooper();
        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.UNTIDY);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) {
            observers.next().onLoadStopped(mTab, /* toDifferentDocument= */ false);
        }
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        assertEquals(1, Robolectric.getForegroundThreadScheduler().size());

        // An additional call to onLoadStopped should not change the state, nor should another
        // task be queued.
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) {
            observers.next().onLoadStopped(mTab, /* toDifferentDocument= */ false);
        }
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        assertEquals(1, Robolectric.getForegroundThreadScheduler().size());

        Robolectric.getForegroundThreadScheduler()
                .advanceBy(
                        TabStateAttributes.DEFAULT_LOW_PRIORITY_SAVE_DELAY_MS,
                        TimeUnit.MILLISECONDS);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
        assertEquals(0, Robolectric.getForegroundThreadScheduler().size());
    }

    @Test
    public void testHide() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onHidden(mTab, TabHidingType.CHANGED_TABS);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        // If a tab is not closing, then hiding the tab should mark it as dirty.
        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.UNTIDY);
        mTab.setClosing(false);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onHidden(mTab, TabHidingType.CHANGED_TABS);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);

        // If a tab is closing, then hiding the tab should not mark it as dirty.
        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.CLEAN);
        mTab.setClosing(true);
        while (observers.hasNext()) observers.next().onHidden(mTab, TabHidingType.CHANGED_TABS);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);
    }

    @Test
    public void testUndoClosingCommitsDirtiness() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onClosingStateChanged(mTab, false);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.UNTIDY);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onClosingStateChanged(mTab, false);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
    }

    @Test
    public void testReparenting() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        // Detaching a tab does not mark a tab as needing to be saved.
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onActivityAttachmentChanged(mTab, null);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        WindowAndroid window = mock(WindowAndroid.class);
        // Re-attaching a tab does mark a tab as needing to be saved.
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onActivityAttachmentChanged(mTab, window);
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
    }

    @Test
    public void testNavigationEntryUpdates() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        RewindableIterator<TabObserver> observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onContentChanged(mTab);
        WebContentsObserver webContentsObserver = mWebContentsObserverCaptor.getValue();
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        webContentsObserver.navigationEntriesChanged();
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
        reset(mAttributesObserver);

        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.CLEAN);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onNavigationEntriesDeleted(mTab);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);

        TabStateAttributes.from(mTab).setStateForTesting(TabStateAttributes.DirtinessState.CLEAN);
        observers = TabTestUtils.getTabObservers(mTab);
        while (observers.hasNext()) observers.next().onNavigationEntriesAppended(mTab);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver, times(2))
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
    }

    @Test
    public void testRootIdUpdates() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        mTab.setRootId(12);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
        TabStateAttributes.from(mTab).clearTabStateDirtiness();

        mTab.setUrl(new GURL(UrlConstants.NTP_URL));
        mTab.setRootId(56);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver, times(2))
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
        TabStateAttributes.from(mTab).clearTabStateDirtiness();

        mTab.setUrl(new GURL(UrlConstants.CONTENT_SCHEME + "://hello_world"));
        mTab.setRootId(100);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
    }

    @Test
    public void testTabGroupIdUpdates() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        mTab.setTabGroupId(new Token(1L, 2L));
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
        TabStateAttributes.from(mTab).clearTabStateDirtiness();

        mTab.setUrl(new GURL(UrlConstants.NTP_URL));
        mTab.setTabGroupId(null);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver, times(2))
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
        TabStateAttributes.from(mTab).clearTabStateDirtiness();

        mTab.setUrl(new GURL(UrlConstants.CONTENT_SCHEME + "://hello_world"));
        mTab.setTabGroupId(new Token(2L, 1L));
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
    }

    @Test
    public void testTabHasSensitiveContentUpdates() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        mTab.setTabHasSensitiveContent(true);
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
        TabStateAttributes.from(mTab).clearTabStateDirtiness();

        mTab.setUrl(new GURL(UrlConstants.NTP_URL));
        mTab.setTabHasSensitiveContent(false);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        TabStateAttributes.from(mTab).clearTabStateDirtiness();

        mTab.setUrl(new GURL(UrlConstants.CONTENT_SCHEME + "://hello_world"));
        mTab.setTabHasSensitiveContent(true);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        // Checks that that the number of dirtiness changes to `UNTIDY` did not increase since the
        // last `UNTIDY` check above.
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
        verify(mAttributesObserver, never())
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
    }

    @Test
    public void testDuplicateUpdateCalls() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.CLEAN);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.UNTIDY);
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
        reset(mAttributesObserver);

        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.UNTIDY);
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.DIRTY);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
        reset(mAttributesObserver);

        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.DIRTY);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.CLEAN);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.CLEAN);
        reset(mAttributesObserver);
    }

    @Test
    public void testUpdatesIgnoredDuringRestore() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.CLEAN);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        mTab.setIsBeingRestored(true);
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.DIRTY);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        mTab.setIsBeingRestored(false);
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.DIRTY);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
    }

    @Test
    public void testDirtyCannotBecomeUntidy() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_FOR_LAZY_LOAD);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());

        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.UNTIDY);
        assertEquals(
                TabStateAttributes.DirtinessState.DIRTY,
                TabStateAttributes.from(mTab).getDirtinessState());
    }

    @Test
    public void testUpdateDirtinessPredicate() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        mTab.setUrl(new GURL(UrlConstants.NTP_URL));
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.UNTIDY);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        mTab.setCanGoForward(false);
        mTab.setCanGoBack(true);
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.UNTIDY);
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        TabStateAttributes.from(mTab).clearTabStateDirtiness();

        mTab.setCanGoForward(true);
        mTab.setCanGoBack(false);
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.UNTIDY);
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
        TabStateAttributes.from(mTab).clearTabStateDirtiness();

        mTab.setCanGoForward(false);
        mTab.setCanGoBack(false);
        mTab.setUrl(new GURL(UrlConstants.CONTENT_SCHEME + "://hello_world"));
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.UNTIDY);
        assertEquals(
                TabStateAttributes.DirtinessState.CLEAN,
                TabStateAttributes.from(mTab).getDirtinessState());

        mTab.setUrl(new GURL("https://www.foo.com/"));
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.UNTIDY);
        assertEquals(
                TabStateAttributes.DirtinessState.UNTIDY,
                TabStateAttributes.from(mTab).getDirtinessState());
    }

    @Test
    public void testBatchEdit() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        TabStateAttributes.from(mTab).beginBatchEdit();
        mTab.setRootId(1);
        mTab.setTabGroupId(Token.createRandom());
        mTab.setRootId(2);
        mTab.setTabGroupId(null);
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());
        TabStateAttributes.from(mTab).endBatchEdit();
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
        reset(mAttributesObserver);

        TabStateAttributes.from(mTab).beginBatchEdit();
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.CLEAN);
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());
        TabStateAttributes.from(mTab).endBatchEdit();
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.CLEAN);
        reset(mAttributesObserver);

        TabStateAttributes.from(mTab).beginBatchEdit();
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.UNTIDY);
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.CLEAN);
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());
        TabStateAttributes.from(mTab).endBatchEdit();
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.UNTIDY);
        reset(mAttributesObserver);

        TabStateAttributes.from(mTab).beginBatchEdit();
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.UNTIDY);
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.CLEAN);
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.DIRTY);
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.CLEAN);
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());
        TabStateAttributes.from(mTab).endBatchEdit();
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
        reset(mAttributesObserver);
    }

    @Test
    public void testNestedBatchEdit() {
        TabStateAttributes.createForTab(mTab, TabCreationState.FROZEN_ON_RESTORE);
        TabStateAttributes.from(mTab).addObserver(mAttributesObserver);

        TabStateAttributes.from(mTab).beginBatchEdit();
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.UNTIDY);
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());

        TabStateAttributes.from(mTab).beginBatchEdit();
        TabStateAttributes.from(mTab).updateIsDirty(TabStateAttributes.DirtinessState.DIRTY);
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());

        TabStateAttributes.from(mTab).endBatchEdit();
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());

        TabStateAttributes.from(mTab).endBatchEdit();
        verify(mAttributesObserver)
                .onTabStateDirtinessChanged(mTab, TabStateAttributes.DirtinessState.DIRTY);
        verifyNoMoreInteractions(mAttributesObserver);
    }
}
