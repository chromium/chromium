// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager.DistillationStatus;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider.DistillabilityObserver;
import org.chromium.chrome.browser.infobar.ReaderModeInfoBar;
import org.chromium.chrome.browser.infobar.ReaderModeInfoBarJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

import java.util.concurrent.TimeoutException;

/** This class tests the behavior of the {@link ReaderModeManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReaderModeManagerTest {
    private static final String MOCK_DISTILLER_URL = "distiller://url";
    private static final String MOCK_URL = "http://url";

    @Rule
    public JniMocker jniMocker = new JniMocker();

    @Mock
    private Tab mTab;

    @Mock
    private WebContents mWebContents;

    @Mock
    private TabDistillabilityProvider mDistillabilityProvider;

    @Mock
    private NavigationController mNavController;

    @Mock
    private DomDistillerTabUtils.Natives mDistillerTabUtilsJniMock;

    @Mock
    private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;

    @Mock
    private ReaderModeInfoBar.Natives mReaderModeInfobarJniMock;

    @Mock
    private NavigationHandle mNavigationHandle;

    @Captor
    private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    private TabObserver mTabObserver;

    @Captor
    private ArgumentCaptor<DistillabilityObserver> mDistillabilityObserverCaptor;
    private DistillabilityObserver mDistillabilityObserver;

    @Captor
    private ArgumentCaptor<WebContentsObserver> mWebContentsObserverCaptor;
    private WebContentsObserver mWebContentsObserver;

    private UserDataHost mUserDataHost;
    private ReaderModeManager mManager;

    @Before
    public void setUp() throws TimeoutException {
        MockitoAnnotations.initMocks(this);
        jniMocker.mock(org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtilsJni.TEST_HOOKS,
                mDistillerTabUtilsJniMock);
        jniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDistillerUrlUtilsJniMock);
        jniMocker.mock(ReaderModeInfoBarJni.TEST_HOOKS, mReaderModeInfobarJniMock);

        DomDistillerTabUtils.setExcludeMobileFriendlyForTesting(true);

        mUserDataHost = new UserDataHost();
        mUserDataHost.setUserData(TabDistillabilityProvider.USER_DATA_KEY, mDistillabilityProvider);

        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrlString()).thenReturn("url");
        when(mWebContents.getNavigationController()).thenReturn(mNavController);
        when(mNavController.getUseDesktopUserAgent()).thenReturn(false);

        when(DomDistillerUrlUtils.isDistilledPage(MOCK_DISTILLER_URL)).thenReturn(true);
        when(DomDistillerUrlUtils.isDistilledPage(MOCK_URL)).thenReturn(false);

        when(DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(MOCK_DISTILLER_URL))
                .thenReturn(MOCK_URL);

        mManager = new ReaderModeManager(mTab);

        // Ensure the tab observer is attached when the manager is created.
        verify(mTab).addObserver(mTabObserverCaptor.capture());
        mTabObserver = mTabObserverCaptor.getValue();

        // Ensure the distillability observer is attached when the tab is shown.
        mTabObserver.onShown(mTab, 0);
        verify(mDistillabilityProvider).addObserver(mDistillabilityObserverCaptor.capture());
        mDistillabilityObserver = mDistillabilityObserverCaptor.getValue();

        // An observer should have also been added to the web contents.
        verify(mWebContents).addObserver(mWebContentsObserverCaptor.capture());
        mWebContentsObserver = mWebContentsObserverCaptor.getValue();
    }

    @Test
    @Feature("ReaderMode")
    public void testUI_triggered() {
        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals("Distillation should be possible.", DistillationStatus.POSSIBLE,
                mManager.getDistillationStatus());
        verify(mReaderModeInfobarJniMock).create(mTab);
    }

    @Test
    @Feature("ReaderMode")
    public void testUI_notTriggered() {
        mDistillabilityObserver.onIsPageDistillableResult(mTab, false, true, false);
        assertEquals("Distillation should not be possible.", DistillationStatus.NOT_POSSIBLE,
                mManager.getDistillationStatus());
        verifyNoMoreInteractions(mReaderModeInfobarJniMock);
    }

    @Test
    @Feature("ReaderMode")
    public void testUI_notTriggered_navBeforeCallback() {
        // Simulate a page navigation prior to the distillability callback happening.
        when(mTab.getUrlString()).thenReturn("http://different_url");

        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals("Distillation should not be possible.", DistillationStatus.NOT_POSSIBLE,
                mManager.getDistillationStatus());
        verifyNoMoreInteractions(mReaderModeInfobarJniMock);
    }

    @Test
    @Feature("ReaderMode")
    public void testUI_notTriggered_afterDismiss() {
        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        verify(mReaderModeInfobarJniMock).create(mTab);
        mManager.onClosed();

        mManager.tryShowingInfoBar();
        verifyNoMoreInteractions(mReaderModeInfobarJniMock);
    }

    @Test
    @Feature("ReaderMode")
    public void testWebContentsObserver_distillerNavigationRemoved() {
        when(mNavController.getEntryAtIndex(0))
                .thenReturn(createNavigationEntry(0, MOCK_DISTILLER_URL));
        when(mNavController.getEntryAtIndex(1)).thenReturn(createNavigationEntry(1, MOCK_URL));

        // Simulate a navigation from a distilled page.
        when(mNavController.getLastCommittedEntryIndex()).thenReturn(0);
        when(mNavigationHandle.isInMainFrame()).thenReturn(true);
        when(mNavigationHandle.isSameDocument()).thenReturn(false);
        when(mNavigationHandle.hasCommitted()).thenReturn(true);
        when(mNavigationHandle.getUrl()).thenReturn(MOCK_URL);

        mWebContentsObserver.didStartNavigation(mNavigationHandle);
        mWebContentsObserver.didFinishNavigation(mNavigationHandle);

        // Distiller entry should have been removed.
        verify(mNavController).removeEntryAtIndex(0);
    }

    @Test
    @Feature("ReaderMode")
    public void testWebContentsObserver_navigateToDistilledPage() {
        when(mNavController.getEntryAtIndex(0))
                .thenReturn(createNavigationEntry(0, MOCK_DISTILLER_URL));

        // Simulate a navigation to a distilled page.
        when(mNavController.getLastCommittedEntryIndex()).thenReturn(0);
        when(mNavigationHandle.isInMainFrame()).thenReturn(true);
        when(mNavigationHandle.isSameDocument()).thenReturn(false);
        when(mNavigationHandle.getUrl()).thenReturn(MOCK_DISTILLER_URL);

        mWebContentsObserver.didStartNavigation(mNavigationHandle);
        mWebContentsObserver.didFinishNavigation(mNavigationHandle);

        assertEquals("Distillation should have started.", DistillationStatus.STARTED,
                mManager.getDistillationStatus());
    }

    @Test
    @Feature("ReaderMode")
    public void testWebContentsObserver_sameDocumentLoad() {
        when(mNavController.getEntryAtIndex(0)).thenReturn(createNavigationEntry(0, MOCK_URL));

        // Simulate an same-document navigation.
        when(mNavController.getLastCommittedEntryIndex()).thenReturn(0);
        when(mNavigationHandle.isInMainFrame()).thenReturn(true);
        when(mNavigationHandle.isSameDocument()).thenReturn(true);
        when(mNavigationHandle.getUrl()).thenReturn(MOCK_URL);

        mWebContentsObserver.didStartNavigation(mNavigationHandle);
        mWebContentsObserver.didFinishNavigation(mNavigationHandle);

        assertEquals("Distillation should not be possible.", DistillationStatus.NOT_POSSIBLE,
                mManager.getDistillationStatus());
    }

    /**
     * @param index The index of the entry.
     * @param url The URL the entry represents.
     * @return A new {@link NavigationEntry}.
     */
    private NavigationEntry createNavigationEntry(int index, String url) {
        return new NavigationEntry(index, url, url, url, url, "", null, 0, 0);
    }
}
