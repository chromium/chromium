// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.archived_tabs_auto_delete_promo;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.test.core.app.ApplicationProvider;

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

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ArchivedTabsAutoDeletePromoManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ArchivedTabsAutoDeletePromoManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private TabArchiveSettings mMockTabArchiveSettings;
    @Mock private TabModel mMockRegularTabModel;
    @Mock private Tab mMockNtpTab;
    @Mock private Tab mMockOtherTab;
    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    private ObservableSupplierImpl<Integer> mArchivedTabCountSupplier;
    private ArchivedTabsAutoDeletePromoManager mManager;

    @Before
    public void setUp() {
        mArchivedTabCountSupplier = new ObservableSupplierImpl<>();

        when(mMockNtpTab.isIncognitoBranded()).thenReturn(false);
        when(mMockNtpTab.getUrl()).thenReturn(JUnitTestGURLs.NTP_URL);
        when(mMockNtpTab.isClosing()).thenReturn(false);
        when(mMockNtpTab.isHidden()).thenReturn(false);

        when(mMockOtherTab.isIncognitoBranded()).thenReturn(false);
        when(mMockOtherTab.getUrl()).thenReturn(JUnitTestGURLs.URL_1);
        when(mMockOtherTab.isClosing()).thenReturn(false);
        when(mMockOtherTab.isHidden()).thenReturn(false);
    }

    /** Sets up all conditions for the promo. */
    private void setupAllConditionsForPromo(
            boolean decisionMade,
            boolean archiveEnabled,
            boolean autoDeleteEnabled,
            int archiveCount) {
        when(mMockTabArchiveSettings.getAutoDeleteDecisionMade()).thenReturn(decisionMade);
        when(mMockTabArchiveSettings.getArchiveEnabled()).thenReturn(archiveEnabled);
        when(mMockTabArchiveSettings.isAutoDeleteEnabled()).thenReturn(autoDeleteEnabled);
        mArchivedTabCountSupplier.set(archiveCount);
    }

    /** Helper to create the manager with common default mock setups for conditions. */
    private void createManager(
            boolean decisionMade,
            boolean archiveEnabled,
            boolean autoDeleteEnabled,
            int archiveCount) {
        setupAllConditionsForPromo(decisionMade, archiveEnabled, autoDeleteEnabled, archiveCount);
        mManager =
                new ArchivedTabsAutoDeletePromoManager(
                        ApplicationProvider.getApplicationContext(),
                        mMockBottomSheetController,
                        mMockTabArchiveSettings,
                        mArchivedTabCountSupplier,
                        mMockRegularTabModel);
    }

    @Test
    public void testConstructor_ConditionsMet_AddsObserverToReadyModel() {
        createManager(false, true, false, 1);
        verify(mMockRegularTabModel).addObserver(any(TabModelObserver.class));
    }

    @Test
    public void testConstructor_DecisionAlreadyMade_NoObserverAdded() {
        createManager(true, true, false, 1);
        verify(mMockRegularTabModel, never()).addObserver(any(TabModelObserver.class));
    }

    @Test
    public void testConstructor_ArchivingDisabled_NoObserverAdded() {
        createManager(false, false, false, 1);
        verify(mMockRegularTabModel, never()).addObserver(any(TabModelObserver.class));
    }

    @Test
    public void testConstructor_AutoDeleteAlreadyEnabled_NoObserverAdded() {
        createManager(false, true, true, 1);
        verify(mMockRegularTabModel, never()).addObserver(any(TabModelObserver.class));
    }

    @Test
    public void testConstructor_NoArchivedTabs_NoObserverAdded() {
        createManager(false, true, false, 0);
        verify(mMockRegularTabModel, never()).addObserver(any(TabModelObserver.class));
    }

    @Test
    public void testOnNtpSelected_AllConditionsMet_ShowsPromo() {
        createManager(false, true, false, 1);
        verify(mMockRegularTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        setupAllConditionsForPromo(false, true, false, 1);

        observer.didSelectTab(mMockNtpTab, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);

        verify(mMockBottomSheetController)
                .requestShowContent(any(ArchivedTabsAutoDeletePromoSheetContent.class), eq(true));
    }

    @Test
    public void testOnNtpSelected_DecisionMadeElseWhere_NoPromo() {
        createManager(false, true, false, 1);
        verify(mMockRegularTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        setupAllConditionsForPromo(true, true, false, 1);

        observer.didSelectTab(mMockNtpTab, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);
        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
        verify(mMockRegularTabModel).removeObserver(observer);
    }

    @Test
    public void testOnNtpSelected_ArchivingDisabled_NoPromo() {
        createManager(false, true, false, 1);
        verify(mMockRegularTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        setupAllConditionsForPromo(false, false, false, 1);

        observer.didSelectTab(mMockNtpTab, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);
        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
        verify(mMockRegularTabModel).removeObserver(observer);
    }

    @Test
    public void testOnNtpSelected_AutoDeleteEnabled_NoPromo() {
        createManager(false, true, false, 1);
        verify(mMockRegularTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        setupAllConditionsForPromo(false, true, true, 1);

        observer.didSelectTab(mMockNtpTab, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);
        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
        verify(mMockRegularTabModel).removeObserver(observer);
    }

    @Test
    public void testOnNtpSelected_NoArchivedTabs_NoPromo() {
        createManager(false, true, false, 1);
        verify(mMockRegularTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        setupAllConditionsForPromo(false, true, false, 0);

        observer.didSelectTab(mMockNtpTab, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);
        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
        verify(mMockRegularTabModel).removeObserver(observer);
    }

    @Test
    public void testOnNonNtpSelected_NoPromo() {
        createManager(false, true, false, 1);
        verify(mMockRegularTabModel).addObserver(mTabModelObserverCaptor.capture());
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        setupAllConditionsForPromo(false, true, false, 1);

        observer.didSelectTab(mMockOtherTab, TabSelectionType.FROM_USER, Tab.INVALID_TAB_ID);
        verify(mMockBottomSheetController, never()).requestShowContent(any(), anyBoolean());
    }
}
