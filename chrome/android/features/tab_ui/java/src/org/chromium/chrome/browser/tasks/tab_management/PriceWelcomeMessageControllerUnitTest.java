// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

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

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceWelcomeMessageReviewActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.PriceWelcomeMessageController.PriceMessageUpdateObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;

/** Unit tests for {@link PriceWelcomeMessageController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PriceWelcomeMessageControllerUnitTest {
    private static final int TAB_ID = 123;
    private static final int OTHER_TAB_ID = 456;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mContext;
    @Mock private TabSwitcherMessageManager mTabSwitcherMessageManager;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock private MessageCardProvider<@MessageType Integer, @UiType Integer> mMessageCardProvider;
    @Mock private Profile mProfile;
    @Mock private TabListCoordinator mTabListCoordinator;
    @Mock private PriceMessageService mPriceMessageService;
    @Mock private PriceMessageService.PriceTabData mPriceTabData;
    @Mock private PriceMessageUpdateObserver mPriceMessageUpdateObserver;
    @Mock private PriceWelcomeMessageReviewActionProvider mActionProvider;

    @Captor private ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;

    private final SettableMonotonicObservableSupplier<TabGroupModelFilter>
            mTabGroupModelFilterSupplier = ObservableSuppliers.createMonotonic();
    private final SettableMonotonicObservableSupplier<TabListCoordinator>
            mTabListCoordinatorSupplier = ObservableSuppliers.createMonotonic();
    private final SettableNullableObservableSupplier<PriceWelcomeMessageReviewActionProvider>
            mActionProviderSupplier = ObservableSuppliers.createNullable();

    private PriceWelcomeMessageController mController;
    private MockTab mTab;

    @Before
    public void setUp() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
        PriceTrackingUtilities.setTrackPricesOnTabsEnabled(false);

        mTab = MockTab.createAndInitialize(TAB_ID, mProfile);
        MockTab.createAndInitialize(OTHER_TAB_ID, mProfile);

        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(true);
        PriceTrackingFeatures.setIsSignedInAndSyncEnabledForTesting(true);
        PriceTrackingUtilities.setTrackPricesOnTabsEnabled(true);

        doNothing().when(mTabGroupModelFilter).addObserver(mTabModelObserverCaptor.capture());
        mTabGroupModelFilterSupplier.set(mTabGroupModelFilter);
        mTabListCoordinatorSupplier.set(mTabListCoordinator);
        mActionProviderSupplier.set(mActionProvider);

        mController =
                new PriceWelcomeMessageController(
                        mTabSwitcherMessageManager,
                        mTabGroupModelFilterSupplier,
                        mMessageCardProvider,
                        mActionProviderSupplier,
                        mProfile,
                        mTabListCoordinatorSupplier,
                        mPriceMessageService);
        mController.addObserver(mPriceMessageUpdateObserver);
    }

    @After
    public void tearDown() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
        PriceTrackingUtilities.setTrackPricesOnTabsEnabled(true);
        PriceTrackingUtilities.disablePriceWelcomeMessageCard();
    }

    @Test
    public void testShowPriceWelcomeMessage() {
        when(mPriceMessageService.preparePriceMessage(anyInt(), any())).thenReturn(true);
        when(mTabGroupModelFilter.getCurrentRepresentativeTabIndex()).thenReturn(5);

        mController.showPriceWelcomeMessage(mPriceTabData);

        verify(mPriceMessageService)
                .preparePriceMessage(
                        eq(PriceMessageService.PriceMessageType.PRICE_WELCOME), eq(mPriceTabData));
        verify(mTabSwitcherMessageManager).appendNextMessage(eq(MessageType.PRICE_MESSAGE));
        verify(mActionProvider).scrollToTab(5);
        verify(mPriceMessageUpdateObserver).onShowPriceWelcomeMessage();
    }

    @Test
    public void testShowPriceWelcomeMessage_priceAnnotationsDisabled() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);

        mController.showPriceWelcomeMessage(mPriceTabData);

        verify(mPriceMessageService, never()).preparePriceMessage(anyInt(), any());
        verify(mPriceMessageUpdateObserver, never()).onShowPriceWelcomeMessage();
    }

    @Test
    public void testShowPriceWelcomeMessage_actionProviderNull() {
        mActionProviderSupplier.set(null);

        mController.showPriceWelcomeMessage(mPriceTabData);

        verify(mPriceMessageService, never()).preparePriceMessage(anyInt(), any());
        verify(mPriceMessageUpdateObserver, never()).onShowPriceWelcomeMessage();
    }

    @Test
    public void testShowPriceWelcomeMessage_prepareFails() {
        when(mPriceMessageService.preparePriceMessage(anyInt(), any())).thenReturn(false);
        mController.showPriceWelcomeMessage(mPriceTabData);

        verify(mTabSwitcherMessageManager, never()).appendNextMessage(anyInt());
        verify(mPriceMessageUpdateObserver).onShowPriceWelcomeMessage();
    }

    @Test
    public void testRemovePriceWelcomeMessage() {
        mController.removePriceWelcomeMessage();

        verify(mTabListCoordinator)
                .removeSpecialListItem(eq(UiType.PRICE_MESSAGE), eq(MessageType.PRICE_MESSAGE));
        verify(mPriceMessageUpdateObserver).onRemovePriceWelcomeMessage();
    }

    @Test
    public void testRestorePriceWelcomeMessage() {
        mController.restorePriceWelcomeMessage();

        verify(mTabSwitcherMessageManager).appendNextMessage(eq(MessageType.PRICE_MESSAGE));
        verify(mPriceMessageUpdateObserver).onRestorePriceWelcomeMessage();
    }

    @Test
    public void testInvalidate() {
        mController.invalidate();
        verify(mPriceMessageService).invalidateMessage();
    }

    @Test
    public void testTabModelObserver_willCloseTab() {
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        when(mPriceMessageService.getBindingTabId()).thenReturn(OTHER_TAB_ID);
        observer.willCloseTab(mTab, false);

        verify(mPriceMessageUpdateObserver, never()).onRemovePriceWelcomeMessage();
        when(mPriceMessageService.getBindingTabId()).thenReturn(TAB_ID);

        observer.willCloseTab(mTab, false);
        verify(mPriceMessageUpdateObserver).onRemovePriceWelcomeMessage();
    }

    @Test
    public void testTabModelObserver_tabClosureUndone() {
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        when(mPriceMessageService.getBindingTabId()).thenReturn(OTHER_TAB_ID);
        observer.tabClosureUndone(mTab);

        verify(mPriceMessageUpdateObserver, never()).onRestorePriceWelcomeMessage();
        when(mPriceMessageService.getBindingTabId()).thenReturn(TAB_ID);

        observer.tabClosureUndone(mTab);
        verify(mPriceMessageUpdateObserver).onRestorePriceWelcomeMessage();
    }

    @Test
    public void testTabModelObserver_tabClosureCommitted() {
        TabModelObserver observer = mTabModelObserverCaptor.getValue();

        when(mPriceMessageService.getBindingTabId()).thenReturn(OTHER_TAB_ID);
        observer.tabClosureCommitted(mTab);

        verify(mPriceMessageService, never()).invalidateMessage();
        when(mPriceMessageService.getBindingTabId()).thenReturn(TAB_ID);

        observer.tabClosureCommitted(mTab);
        verify(mPriceMessageService).invalidateMessage();
    }

    @Test
    public void testBuild_priceAnnotationsEnabled() {
        reset(mMessageCardProvider);
        var filterSupplier = ObservableSuppliers.<TabGroupModelFilter>createMonotonic();
        mController =
                PriceWelcomeMessageController.build(
                        mContext,
                        mTabSwitcherMessageManager,
                        filterSupplier,
                        mMessageCardProvider,
                        mActionProviderSupplier,
                        mProfile,
                        mTabListCoordinatorSupplier);
        verify(mMessageCardProvider).subscribeMessageService(any(PriceMessageService.class));
        assertTrue(filterSupplier.hasObservers());
    }

    @Test
    public void testBuild_priceAnnotationsDisabled() {
        PriceTrackingFeatures.setPriceAnnotationsEnabledForTesting(false);
        reset(mMessageCardProvider);
        var filterSupplier = ObservableSuppliers.<TabGroupModelFilter>createMonotonic();
        mController =
                PriceWelcomeMessageController.build(
                        mContext,
                        mTabSwitcherMessageManager,
                        filterSupplier,
                        mMessageCardProvider,
                        mActionProviderSupplier,
                        mProfile,
                        mTabListCoordinatorSupplier);
        verify(mMessageCardProvider, never()).subscribeMessageService(any());
        assertFalse(filterSupplier.hasObservers());
    }
}
