// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;

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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.concurrent.TimeoutException;

/** This class tests the behavior of the {@link ReaderModeManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReaderModeManagerTest {
    private static final GURL MOCK_DISTILLER_URL = new GURL("chrome-distiller://url");
    private static final GURL MOCK_URL = JUnitTestGURLs.GOOGLE_URL_CAT;

    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock private Tab mTab;

    @Mock private WebContents mWebContents;

    @Mock private TabDistillabilityProvider mDistillabilityProvider;

    @Mock private NavigationController mNavController;

    @Mock private DomDistillerTabUtils.Natives mDistillerTabUtilsJniMock;

    @Mock private DomDistillerUrlUtils.Natives mDistillerUrlUtilsJniMock;

    @Mock private NavigationHandle mNavigationHandle;

    @Mock private MessageDispatcher mMessageDispatcher;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    private TabObserver mTabObserver;

    @Captor private ArgumentCaptor<DistillabilityObserver> mDistillabilityObserverCaptor;
    private DistillabilityObserver mDistillabilityObserver;

    @Captor private ArgumentCaptor<WebContentsObserver> mWebContentsObserverCaptor;
    private WebContentsObserver mWebContentsObserver;

    private UserDataHost mUserDataHost;
    private ReaderModeManager mManager;

    @Before
    public void setUp() throws TimeoutException {
        MockitoAnnotations.initMocks(this);
        jniMocker.mock(
                org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtilsJni.TEST_HOOKS,
                mDistillerTabUtilsJniMock);
        jniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDistillerUrlUtilsJniMock);

        DomDistillerTabUtils.setExcludeMobileFriendlyForTesting(true);

        mUserDataHost = new UserDataHost();
        mUserDataHost.setUserData(TabDistillabilityProvider.USER_DATA_KEY, mDistillabilityProvider);

        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getUrl()).thenReturn(MOCK_URL);
        when(mTab.getContext()).thenReturn(ApplicationProvider.getApplicationContext());
        when(mWebContents.getNavigationController()).thenReturn(mNavController);
        when(mNavController.getUseDesktopUserAgent()).thenReturn(false);

        when(mDistillerUrlUtilsJniMock.isDistilledPage(MOCK_DISTILLER_URL.getSpec()))
                .thenReturn(true);

        when(mDistillerUrlUtilsJniMock.getOriginalUrlFromDistillerUrl(MOCK_DISTILLER_URL.getSpec()))
                .thenReturn(MOCK_URL);

        mManager = new ReaderModeManager(mTab, () -> mMessageDispatcher);

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
        mManager.clearSavedSitesForTesting();
    }

    @Test
    @Feature("ReaderMode")
    public void testUI_notTriggered() {
        mDistillabilityObserver.onIsPageDistillableResult(mTab, false, true, false);
        assertEquals(
                "Distillation should not be possible.",
                DistillationStatus.NOT_POSSIBLE,
                mManager.getDistillationStatus());
        verifyNoMoreInteractions(mMessageDispatcher);
    }

    @Test
    @Feature("ReaderMode")
    public void testUI_notTriggered_navBeforeCallback() {
        // Simulate a page navigation prior to the distillability callback happening.
        when(mTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);

        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals(
                "Distillation should not be possible.",
                DistillationStatus.NOT_POSSIBLE,
                mManager.getDistillationStatus());
    }

    @Test
    @Feature("ReaderMode")
    public void testUI_notTriggered_muted() {
        mManager.muteSiteForTesting(mTab.getUrl());
        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals(
                "Distillation should be possible.",
                DistillationStatus.POSSIBLE,
                mManager.getDistillationStatus());
        verify(mMessageDispatcher, never()).enqueueMessage(any(), any(), anyInt(), anyBoolean());
    }

    @Test
    @Feature("ReaderMode")
    public void testUI_notTriggered_mutedByDomain() {
        mManager.muteSiteForTesting(JUnitTestGURLs.GOOGLE_URL_DOG);
        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals(
                "Distillation should be possible.",
                DistillationStatus.POSSIBLE,
                mManager.getDistillationStatus());
        verify(
                        mMessageDispatcher,
                        never().description("Reader mode should be muted in this domain"))
                .enqueueMessage(any(), any(), anyInt(), anyBoolean());
    }

    @Test
    @Feature("ReaderMode")
    public void testUI_notTriggered_contextualPageActionUiEnabled() {
        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals(
                "Distillation should be possible.",
                DistillationStatus.POSSIBLE,
                mManager.getDistillationStatus());
        verify(
                        mMessageDispatcher,
                        never().description(
                                        "Message should be suppressed as the CPA UI will be shown"))
                .enqueueMessage(
                        any(), eq(mWebContents), eq(MessageScopeType.NAVIGATION), eq(false));
    }

    @Test
    @Feature("ReaderMode")
    public void testUI_notTriggered_contextualPageActionUiEnabled_exceptOnCCT() {
        when(mTab.isCustomTab()).thenReturn(true);
        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals(
                "Distillation should be possible.",
                DistillationStatus.POSSIBLE,
                mManager.getDistillationStatus());
        verify(mMessageDispatcher)
                .enqueueMessage(
                        any(), eq(mWebContents), eq(MessageScopeType.NAVIGATION), eq(false));
    }

    @Test
    @Feature("ReaderMode")
    public void testUI_notTriggered_contextualPageActionUiEnabled_exceptOnIncognitoTabs() {
        when(mTab.isIncognito()).thenReturn(true);
        mDistillabilityObserver.onIsPageDistillableResult(mTab, true, true, false);
        assertEquals(
                "Distillation should be possible.",
                DistillationStatus.POSSIBLE,
                mManager.getDistillationStatus());
        verify(mMessageDispatcher)
                .enqueueMessage(
                        any(), eq(mWebContents), eq(MessageScopeType.NAVIGATION), eq(false));
    }

    @Test
    @Feature("ReaderMode")
    public void testWebContentsObserver_distillerNavigationRemoved() {
        when(mNavController.getEntryAtIndex(0))
                .thenReturn(createNavigationEntry(0, MOCK_DISTILLER_URL));
        when(mNavController.getEntryAtIndex(1)).thenReturn(createNavigationEntry(1, MOCK_URL));

        // Simulate a navigation from a distilled page.
        when(mNavController.getLastCommittedEntryIndex()).thenReturn(0);
        when(mNavigationHandle.isSameDocument()).thenReturn(false);
        when(mNavigationHandle.hasCommitted()).thenReturn(true);
        when(mNavigationHandle.getUrl()).thenReturn(MOCK_URL);

        mWebContentsObserver.didStartNavigationInPrimaryMainFrame(mNavigationHandle);
        mWebContentsObserver.didFinishNavigationInPrimaryMainFrame(mNavigationHandle);

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
        when(mNavigationHandle.isSameDocument()).thenReturn(false);
        when(mNavigationHandle.getUrl()).thenReturn(MOCK_DISTILLER_URL);

        mWebContentsObserver.didStartNavigationInPrimaryMainFrame(mNavigationHandle);
        mWebContentsObserver.didFinishNavigationInPrimaryMainFrame(mNavigationHandle);

        assertEquals(
                "Distillation should have started.",
                DistillationStatus.STARTED,
                mManager.getDistillationStatus());
    }

    @Test
    @Feature("ReaderMode")
    public void testWebContentsObserver_sameDocumentLoad() {
        when(mNavController.getEntryAtIndex(0)).thenReturn(createNavigationEntry(0, MOCK_URL));

        // Simulate an same-document navigation.
        when(mNavController.getLastCommittedEntryIndex()).thenReturn(0);
        when(mNavigationHandle.isSameDocument()).thenReturn(true);
        when(mNavigationHandle.getUrl()).thenReturn(MOCK_URL);

        mWebContentsObserver.didStartNavigationInPrimaryMainFrame(mNavigationHandle);
        mWebContentsObserver.didFinishNavigationInPrimaryMainFrame(mNavigationHandle);

        assertEquals(
                "Distillation should not be possible.",
                DistillationStatus.NOT_POSSIBLE,
                mManager.getDistillationStatus());
    }

    /**
     * @param index The index of the entry.
     * @param url The URL the entry represents.
     * @return A new {@link NavigationEntry}.
     */
    private NavigationEntry createNavigationEntry(int index, GURL url) {
        return new NavigationEntry(
                index, url, url, url, "", null, 0, 0, /* isInitialEntry= */ false);
    }
}
