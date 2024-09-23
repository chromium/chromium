// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.price_insights;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.price_insights.PriceInsightsBottomSheetCoordinator.PriceInsightsDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.modaldialog.ModalDialogManager;

@RunWith(BaseRobolectricTestRunner.class)
public class PriceInsightsButtonControllerTest {

    @Mock private Tab mMockTab;
    @Mock private Supplier<TabBookmarker> mMockTabBookmarkerSupplier;
    @Mock private Supplier<Tab> mMockTabSupplier;
    @Mock private Supplier<TabModelSelector> mMockTabModelSelectorSupplier;
    @Mock private Supplier<ShoppingService> mMockShoppingServiceSupplier;
    @Mock private ModalDialogManager mMockModalDialogManager;
    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private SnackbarManager mMockSnackbarManager;
    @Mock private PriceInsightsBottomSheetCoordinator mMockPriceInsightsBottomSheetCoordinator;
    @Mock private PriceInsightsDelegate mMockPriceInsightsDelegate;
    @Mock private CommerceBottomSheetContentController mMockCommerceBottomSheetContentController;

    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        Context mockContext = mock(Context.class);
        Resources mockResources = mock(Resources.class);
        doReturn(mockResources).when(mockContext).getResources();
        doReturn(mockContext).when(mMockTab).getContext();
        doReturn(mMockTab).when(mMockTabSupplier).get();
    }

    private PriceInsightsButtonController createButtonController() {
        return new PriceInsightsButtonController(
                mMockTab.getContext(),
                mMockTabSupplier,
                mMockTabModelSelectorSupplier,
                mMockShoppingServiceSupplier,
                mMockModalDialogManager,
                mMockBottomSheetController,
                mMockSnackbarManager,
                mMockPriceInsightsDelegate,
                mock(Drawable.class),
                () -> mMockCommerceBottomSheetContentController);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ENABLE_DISCOUNT_INFO_API)
    public void testPriceInsightsButtonControllerOnClick() {
        PriceInsightsButtonController buttonController = createButtonController();
        buttonController.setPriceInsightsBottomSheetCoordinatorForTesting(
                mMockPriceInsightsBottomSheetCoordinator);
        ButtonData buttonData = buttonController.get(mMockTab);

        verify(mMockBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        assertTrue(buttonData.isEnabled());

        buttonData.getButtonSpec().getOnClickListener().onClick(null);
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        verify(mMockPriceInsightsBottomSheetCoordinator, times(1)).requestShowContent();
        assertFalse(buttonData.isEnabled());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ENABLE_DISCOUNT_INFO_API)
    public void testPriceInsightsButtonControllerDestroy() {
        PriceInsightsButtonController buttonController = createButtonController();
        buttonController.setPriceInsightsBottomSheetCoordinatorForTesting(
                mMockPriceInsightsBottomSheetCoordinator);
        ButtonData buttonData = buttonController.get(mMockTab);

        verify(mMockBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        assertTrue(buttonData.isEnabled());

        buttonData.getButtonSpec().getOnClickListener().onClick(null);
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        assertFalse(buttonData.isEnabled());

        buttonController.destroy();
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.HIDDEN, StateChangeReason.NONE);

        verify(mMockPriceInsightsBottomSheetCoordinator, times(1)).closeContent();
        verify(mMockBottomSheetController).removeObserver(mBottomSheetObserverCaptor.capture());
        assertTrue(buttonData.isEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ENABLE_DISCOUNT_INFO_API)
    public void testPriceInsightsButtonControllerOnClick_withCommerceBottomSheet() {
        PriceInsightsButtonController buttonController = createButtonController();
        buttonController.onClick(null);
        verify(mMockCommerceBottomSheetContentController, times(1)).requestShowContent();
    }
}
