// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

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

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.TabStateAttributes.DirtinessState;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.TimeUnit;

/** Unit tests for TabStateAttributes. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowLooper.class})
public class TabStateAttributesTest {

    @Rule public final MockitoRule mockito = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private MockWebContents mWebContents;
    @Mock private TabStateAttributes.Observer mAttributesObserver;
    @Mock private TabStateAttributes.Observer mAttributesObserver2;

    @Captor ArgumentCaptor<WebContentsObserver> mWebContentsObserverCaptor;

    private MockTab mTab;

    @Before
    public void setUp() {
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
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, null);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        TabStateAttributesRegistry.clearForTesting(mTab);

        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        TabStateAttributesRegistry.clearForTesting(mTab);

        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_FOR_LAZY_LOAD);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        TabStateAttributesRegistry.clearForTesting(mTab);

        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.LIVE_IN_BACKGROUND);
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        TabStateAttributesRegistry.clearForTesting(mTab);

        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.LIVE_IN_FOREGROUND);
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        TabStateAttributesRegistry.clearForTesting(mTab);
    }

    @Test
    public void testTitleUpdate() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onTitleUpdated(mTab);
        }

        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.UNTIDY);
    }

    @Test
    public void testFinishMainFrameNavigation() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onContentChanged(mTab);
        }
        WebContentsObserver webContentsObserver = mWebContentsObserverCaptor.getValue();
        getAttributes().addObserver(mAttributesObserver);
        GURL testGURL = JUnitTestGURLs.EXAMPLE_URL;
        NavigationHandle navHandle = NavigationHandle.createForTesting(testGURL, false, 0, false);

        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        webContentsObserver.didFinishNavigationInPrimaryMainFrame(navHandle);
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.UNTIDY);
    }

    @Test
    public void testPageLoadFinished() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);
        GURL testGURL = JUnitTestGURLs.EXAMPLE_URL;

        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onPageLoadFinished(mTab, testGURL);
        }
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.UNTIDY);
    }

    @Test
    public void testLoadStopped_DifferentDocument() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);

        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onLoadStopped(mTab, /* toDifferentDocument= */ true);
        }
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        getAttributes().setStateForTesting(DirtinessState.UNTIDY);
        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onLoadStopped(mTab, /* toDifferentDocument= */ true);
        }
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
    }

    @Test
    public void testLoadStopped_SameDocument() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);

        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onLoadStopped(mTab, /* toDifferentDocument= */ false);
        }
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        RobolectricUtil.runAllBackgroundAndUi();
        getAttributes().setStateForTesting(DirtinessState.UNTIDY);
        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onLoadStopped(mTab, /* toDifferentDocument= */ false);
        }
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        assertEquals(1, Robolectric.getForegroundThreadScheduler().size());

        // An additional call to onLoadStopped should not change the state, nor should another
        // task be queued.
        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onLoadStopped(mTab, /* toDifferentDocument= */ false);
        }
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        assertEquals(1, Robolectric.getForegroundThreadScheduler().size());

        Robolectric.getForegroundThreadScheduler()
                .advanceBy(
                        TabStateAttributes.DEFAULT_LOW_PRIORITY_SAVE_DELAY_MS,
                        TimeUnit.MILLISECONDS);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
        assertEquals(0, Robolectric.getForegroundThreadScheduler().size());
    }

    @Test
    public void testLoadStopped_NTPInTabGroup() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);

        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        mTab.setUrl(new GURL(getOriginalNativeNtpUrl()));
        mTab.setTabGroupId(new Token(1L, 2L));

        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onLoadStopped(mTab, /* toDifferentDocument= */ true);
        }
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
    }

    @Test
    public void testHide() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        getAttributes().addObserver(mAttributesObserver);

        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onHidden(mTab, TabHidingType.CHANGED_TABS);
        }
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        // If a tab is not closing, then hiding the tab should mark it as dirty.
        getAttributes().setStateForTesting(DirtinessState.UNTIDY);
        mTab.setClosing(false);
        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onHidden(mTab, TabHidingType.CHANGED_TABS);
        }
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);

        // If a tab is closing, then hiding the tab should not mark it as dirty.
        getAttributes().setStateForTesting(DirtinessState.CLEAN);
        mTab.setClosing(true);
        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onHidden(mTab, TabHidingType.CHANGED_TABS);
        }
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);
    }

    @Test
    public void testUndoClosingCommitsDirtiness() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        getAttributes().addObserver(mAttributesObserver);

        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onClosingStateChanged(mTab, false);
        }
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        getAttributes().setStateForTesting(DirtinessState.UNTIDY);
        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onClosingStateChanged(mTab, false);
        }
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
    }

    @Test
    public void testReparenting() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        getAttributes().addObserver(mAttributesObserver);

        // Detaching a tab does not mark a tab as needing to be saved.
        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onActivityAttachmentChanged(mTab, null);
        }
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        WindowAndroid window = mock(WindowAndroid.class);
        // Re-attaching a tab does mark a tab as needing to be saved.
        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onActivityAttachmentChanged(mTab, window);
        }
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.UNTIDY);
    }

    @Test
    public void testNavigationEntryUpdates() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onContentChanged(mTab);
        }
        WebContentsObserver webContentsObserver = mWebContentsObserverCaptor.getValue();
        getAttributes().addObserver(mAttributesObserver);

        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        webContentsObserver.navigationEntriesChanged();
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.UNTIDY);
        reset(mAttributesObserver);

        getAttributes().setStateForTesting(DirtinessState.CLEAN);
        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onNavigationEntriesDeleted(mTab);
        }
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);

        getAttributes().setStateForTesting(DirtinessState.CLEAN);
        for (TabObserver observer : TabTestUtils.getTabObservers(mTab)) {
            observer.onNavigationEntriesAppended(mTab);
        }
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver, times(2))
                .onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
    }

    @Test
    public void testRootIdUpdates() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        mTab.setRootId(12);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
        getAttributes().clearTabStateDirtiness();

        mTab.setUrl(new GURL(getOriginalNativeNtpUrl()));
        mTab.setRootId(56);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver, times(2))
                .onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
        getAttributes().clearTabStateDirtiness();

        mTab.setUrl(new GURL(UrlConstants.CONTENT_SCHEME + "://hello_world"));
        mTab.setRootId(100);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
    }

    @Test
    public void testTabGroupIdUpdates() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        mTab.setTabGroupId(new Token(1L, 2L));
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
        getAttributes().clearTabStateDirtiness();

        mTab.setUrl(new GURL(getOriginalNativeNtpUrl()));
        mTab.setTabGroupId(null);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver, times(2))
                .onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
        getAttributes().clearTabStateDirtiness();

        mTab.setUrl(new GURL(UrlConstants.CONTENT_SCHEME + "://hello_world"));
        mTab.setTabGroupId(new Token(2L, 1L));
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
    }

    @Test
    public void testTabHasSensitiveContentUpdates() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        mTab.setTabHasSensitiveContent(true);
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.UNTIDY);
        getAttributes().clearTabStateDirtiness();

        mTab.setUrl(new GURL(getOriginalNativeNtpUrl()));
        mTab.setTabHasSensitiveContent(false);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        getAttributes().clearTabStateDirtiness();

        mTab.setUrl(new GURL(UrlConstants.CONTENT_SCHEME + "://hello_world"));
        mTab.setTabHasSensitiveContent(true);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        // Checks that that the number of dirtiness changes to `UNTIDY` did not increase since the
        // last `UNTIDY` check above.
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.UNTIDY);
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
    }

    @Test
    public void testIsPinnedUpdates() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        mTab.setIsPinned(true);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
        getAttributes().clearTabStateDirtiness();

        mTab.setIsPinned(false);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        getAttributes().clearTabStateDirtiness();
        verify(mAttributesObserver, times(2))
                .onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);

        // Test for NTP.
        mTab.setUrl(new GURL(getOriginalNativeNtpUrl()));
        mTab.setIsPinned(true);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        getAttributes().clearTabStateDirtiness();
        verify(mAttributesObserver, times(3))
                .onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);

        mTab.setIsPinned(false);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        getAttributes().clearTabStateDirtiness();
        verify(mAttributesObserver, times(4))
                .onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);

        verify(mAttributesObserver, never())
                .onTabStateDirtinessChanged(mTab, DirtinessState.UNTIDY);
    }

    @Test
    public void testTabUnarchived() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        mTab.onTabRestoredFromArchivedTabModel();
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
        getAttributes().clearTabStateDirtiness();

        mTab.setUrl(new GURL(getOriginalNativeNtpUrl()));
        mTab.onTabRestoredFromArchivedTabModel();
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver, times(2))
                .onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
        getAttributes().clearTabStateDirtiness();

        mTab.setUrl(new GURL(UrlConstants.CONTENT_SCHEME + "://hello_world"));
        mTab.onTabRestoredFromArchivedTabModel();
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
    }

    @Test
    public void testDuplicateUpdateCalls() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        getAttributes().updateIsDirty(DirtinessState.CLEAN);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        getAttributes().updateIsDirty(DirtinessState.UNTIDY);
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.UNTIDY);
        reset(mAttributesObserver);

        getAttributes().updateIsDirty(DirtinessState.UNTIDY);
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        getAttributes().updateIsDirty(DirtinessState.DIRTY);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
        reset(mAttributesObserver);

        getAttributes().updateIsDirty(DirtinessState.DIRTY);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
        verifyNoMoreInteractions(mAttributesObserver);
        reset(mAttributesObserver);

        getAttributes().updateIsDirty(DirtinessState.CLEAN);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.CLEAN);
        reset(mAttributesObserver);
    }

    @Test
    public void testUpdatesIgnoredDuringRestore() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().updateIsDirty(DirtinessState.CLEAN);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        mTab.setIsBeingRestored(true);
        getAttributes().updateIsDirty(DirtinessState.DIRTY);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        mTab.setIsBeingRestored(false);
        getAttributes().updateIsDirty(DirtinessState.DIRTY);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
    }

    @Test
    public void testDirtyCannotBecomeUntidy() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_FOR_LAZY_LOAD);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());

        getAttributes().updateIsDirty(DirtinessState.UNTIDY);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
    }

    @Test
    public void testUpdateDirtinessPredicate() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        mTab.setUrl(new GURL(getOriginalNativeNtpUrl()));
        getAttributes().updateIsDirty(DirtinessState.UNTIDY);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        mTab.setCanGoForward(false);
        mTab.setCanGoBack(true);
        getAttributes().updateIsDirty(DirtinessState.UNTIDY);
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        getAttributes().clearTabStateDirtiness();

        mTab.setCanGoForward(true);
        mTab.setCanGoBack(false);
        getAttributes().updateIsDirty(DirtinessState.UNTIDY);
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
        getAttributes().clearTabStateDirtiness();

        mTab.setCanGoForward(false);
        mTab.setCanGoBack(false);
        mTab.setUrl(new GURL(UrlConstants.CONTENT_SCHEME + "://hello_world"));
        getAttributes().updateIsDirty(DirtinessState.UNTIDY);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        mTab.setUrl(new GURL("https://www.foo.com/"));
        getAttributes().updateIsDirty(DirtinessState.UNTIDY);
        assertEquals(DirtinessState.UNTIDY, getAttributes().getDirtinessState());
    }

    @Test
    public void testBatchEdit() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);

        getAttributes().beginBatchEdit();
        mTab.setRootId(1);
        mTab.setTabGroupId(Token.createRandom());
        mTab.setRootId(2);
        mTab.setTabGroupId(null);
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());
        getAttributes().endBatchEdit();
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
        reset(mAttributesObserver);

        getAttributes().beginBatchEdit();
        getAttributes().updateIsDirty(DirtinessState.CLEAN);
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());
        getAttributes().endBatchEdit();
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.CLEAN);
        reset(mAttributesObserver);

        getAttributes().beginBatchEdit();
        getAttributes().updateIsDirty(DirtinessState.UNTIDY);
        getAttributes().updateIsDirty(DirtinessState.CLEAN);
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());
        getAttributes().endBatchEdit();
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.UNTIDY);
        reset(mAttributesObserver);

        getAttributes().beginBatchEdit();
        getAttributes().updateIsDirty(DirtinessState.UNTIDY);
        getAttributes().updateIsDirty(DirtinessState.CLEAN);
        getAttributes().updateIsDirty(DirtinessState.DIRTY);
        getAttributes().updateIsDirty(DirtinessState.CLEAN);
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());
        getAttributes().endBatchEdit();
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
        reset(mAttributesObserver);
    }

    @Test
    public void testNestedBatchEdit() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);

        getAttributes().beginBatchEdit();
        getAttributes().updateIsDirty(DirtinessState.UNTIDY);
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());

        getAttributes().beginBatchEdit();
        getAttributes().updateIsDirty(DirtinessState.DIRTY);
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());

        getAttributes().endBatchEdit();
        verify(mAttributesObserver, never()).onTabStateDirtinessChanged(eq(mTab), anyInt());

        getAttributes().endBatchEdit();
        verify(mAttributesObserver).onTabStateDirtinessChanged(mTab, DirtinessState.DIRTY);
        verifyNoMoreInteractions(mAttributesObserver);
    }

    @Test
    public void testDoubleObserver() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        getAttributes().addObserver(mAttributesObserver);
        getAttributes().addObserver(mAttributesObserver2);

        // Both observers will try to clear the dirty state.
        MockitoHelper.doRunnable(
                        () ->
                                TabStateAttributesRegistry.getAttributesFor(
                                                mTab, TabStateAttributes.StoreKey.class)
                                        .clearTabStateDirtiness())
                .when(mAttributesObserver)
                .onTabStateDirtinessChanged(any(), anyInt());
        MockitoHelper.doRunnable(
                        () ->
                                TabStateAttributesRegistry.getAttributesFor(
                                                mTab, TabStateAttributes.StoreKey.class)
                                        .clearTabStateDirtiness())
                .when(mAttributesObserver2)
                .onTabStateDirtinessChanged(any(), anyInt());

        getAttributes().updateIsDirty(DirtinessState.DIRTY);

        // Regardless of which observer is notified first, both should see dirty.
        verify(mAttributesObserver).onTabStateDirtinessChanged(any(), eq(DirtinessState.DIRTY));
        verify(mAttributesObserver2).onTabStateDirtinessChanged(any(), eq(DirtinessState.DIRTY));
    }

    @Test
    public void testSetDirty() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        assertEquals(DirtinessState.CLEAN, getAttributes().getDirtinessState());

        TabStateAttributes.setDirty(mTab);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());

        // Null and destroyed tabs should not throw exception
        TabStateAttributes.setDirty(null);
        mTab.destroy();
        TabStateAttributes.setDirty(mTab);
    }

    @Test
    public void testUpdateIsDirty_nullTabUrl() {
        TabStateAttributesRegistry.createAttributesForTab(
                mTab, TabStateAttributes.StoreKey.class, TabCreationState.FROZEN_ON_RESTORE);
        mTab.setUrl(null);
        getAttributes().updateIsDirty(DirtinessState.DIRTY);
        assertEquals(DirtinessState.DIRTY, getAttributes().getDirtinessState());
    }

    private TabStateAttributes getAttributes() {
        return TabStateAttributesRegistry.getAttributesFor(mTab, TabStateAttributes.StoreKey.class);
    }
}
