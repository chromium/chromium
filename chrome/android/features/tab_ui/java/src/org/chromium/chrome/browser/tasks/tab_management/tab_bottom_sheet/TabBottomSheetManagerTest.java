// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TabBottomSheetManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.TAB_BOTTOM_SHEET})
public class TabBottomSheetManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private TabModel mMockRegularTabModel;
    @Mock private Tab mMockNtpTab;
    @Mock private Tab mMockIncognitoTab;
    @Mock private Tab mMockOtherTab;
    @Mock private Tab mMockClosingTab;
    @Mock private Tab mMockHiddenTab;

    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private TabBottomSheetManager mManager;

    @Before
    public void setUp() {
        // Setup for a generic NTP tab
        when(mMockNtpTab.isIncognitoBranded()).thenReturn(false);
        when(mMockNtpTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mMockNtpTab.isClosing()).thenReturn(false);
        when(mMockNtpTab.isHidden()).thenReturn(false);

        // Setup for an incognito tab
        when(mMockIncognitoTab.isIncognitoBranded()).thenReturn(true);
        when(mMockIncognitoTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mMockIncognitoTab.isClosing()).thenReturn(false);
        when(mMockIncognitoTab.isHidden()).thenReturn(false);

        // Setup for another regular tab (non-NTP)
        when(mMockOtherTab.isIncognitoBranded()).thenReturn(false);
        when(mMockOtherTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mMockOtherTab.isClosing()).thenReturn(false);
        when(mMockOtherTab.isHidden()).thenReturn(false);

        // Setup for a closing tab
        when(mMockClosingTab.isIncognitoBranded()).thenReturn(false);
        when(mMockClosingTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mMockClosingTab.isClosing()).thenReturn(true);
        when(mMockClosingTab.isHidden()).thenReturn(false);

        // Setup for a hidden tab
        when(mMockHiddenTab.isIncognitoBranded()).thenReturn(false);
        when(mMockHiddenTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mMockHiddenTab.isClosing()).thenReturn(false);
        when(mMockHiddenTab.isHidden()).thenReturn(true);
    }

    @After
    public void tearDown() {
        if (mManager != null) {
            mManager.destroy();
        }
    }

    /** Helper to create the manager with a specific feature flag state. */
    private void createManager() {
        mManager =
                new TabBottomSheetManager(
                        ApplicationProvider.getApplicationContext(),
                        mMockRegularTabModel,
                        mMockBottomSheetController);
    }

    @Test
    public void testConstructor_FeatureEnabled_AddsObserver() {
        createManager();
        verify(mMockRegularTabModel).addObserver(any(TabModelObserver.class));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.TAB_BOTTOM_SHEET})
    public void testConstructor_FeatureDisabled_NoObserverAdded() {
        createManager();
        verify(mMockRegularTabModel, never()).addObserver(any(TabModelObserver.class));
    }

    @Test
    public void testTryToShowBottomSheet_FeatureEnabled_ShowsBottomSheet() {
        createManager();
        mManager.tryToShowBottomSheet();
        verify(mMockBottomSheetController).requestShowContent(any(), anyBoolean());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.TAB_BOTTOM_SHEET})
    public void testTryToShowBottomSheet_FeatureDisabled_NoBottomSheet() {
        createManager();
        mManager.tryToShowBottomSheet();
        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testOnDidSelectTab_NtpTabFeatureEnabled_ShowsBottomSheet() {
        createManager();
        verify(mMockRegularTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        observer.didSelectTab(mMockNtpTab, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);

        verify(mMockBottomSheetController).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testOnDidSelectTab_IncognitoTab_NoBottomSheet() {
        createManager();
        verify(mMockRegularTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        observer.didSelectTab(mMockIncognitoTab, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);

        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testOnDidSelectTab_NonNtpTab_NoBottomSheet() {
        createManager();
        verify(mMockRegularTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        observer.didSelectTab(mMockOtherTab, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);

        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testOnDidSelectTab_ClosingTab_NoBottomSheet() {
        createManager();
        verify(mMockRegularTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        observer.didSelectTab(mMockClosingTab, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);

        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testOnDidSelectTab_HiddenTab_NoBottomSheet() {
        createManager();
        verify(mMockRegularTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        observer.didSelectTab(mMockHiddenTab, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);

        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void testDestroy_RemovesObserverAndDestroysCoordinator() {
        createManager();
        verify(mMockRegularTabModel).addObserver(mTabModelObserverCaptor.capture());

        mManager.destroy();

        verify(mMockRegularTabModel).removeObserver(mTabModelObserverCaptor.getValue());
        assertNull(mManager.getTabBottomSheetCoordinatorForTesting());
    }

    @Test
    @DisableFeatures({ChromeFeatureList.TAB_BOTTOM_SHEET})
    public void testDestroy_FeatureDisabled_NoExtraActions() {
        createManager();
        verify(mMockRegularTabModel, never()).addObserver(any(TabModelObserver.class));
        mManager.destroy();
        assertNull(mManager.getTabBottomSheetCoordinatorForTesting());
    }
}
