// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.drawable.Drawable;

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

import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Unit test for {@link PriceTrackingButtonController}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Features.EnableFeatures({ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING,
        ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2})
public class PriceTrackingButtonControllerUnitTest {
    @Rule
    public TestRule mFeaturesProcessor = new Features.JUnitProcessor();

    private Activity mActivity;
    @Mock
    private Tab mMockTab;
    @Mock
    private ObservableSupplier<Tab> mMockTabSupplier;
    @Mock
    private Supplier<TabBookmarker> mMockTabBookmarkerSupplier;
    @Mock
    private TabBookmarker mMockTabBookmarker;
    @Mock
    private BottomSheetController mMockBottomSheetController;
    @Mock
    private ModalDialogManager mMockModalDialogManager;
    @Captor
    private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        MockitoAnnotations.initMocks(this);

        when(mMockTab.getContext()).thenReturn(mActivity);
        when(mMockTabSupplier.get()).thenReturn(mMockTab);
        when(mMockTabBookmarkerSupplier.get()).thenReturn(mMockTabBookmarker);
    }

    @Test
    public void testButtonData_QuietVariation() {
        PriceTrackingButtonController priceTrackingButtonController =
                new PriceTrackingButtonController(mMockTabSupplier, mMockModalDialogManager,
                        mMockBottomSheetController, mock(Drawable.class),
                        mMockTabBookmarkerSupplier);
        ButtonData buttonData = priceTrackingButtonController.get(mMockTab);

        // Quiet variation uses an IPHCommandBuilder to highlight the action.
        Assert.assertNotNull(buttonData.getButtonSpec().getIPHCommandBuilder());
    }

    @Test
    public void testButtonData_ActionChipVariation() {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFeatureFlagOverride(ChromeFeatureList.CONTEXTUAL_PAGE_ACTIONS, true);
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.CONTEXTUAL_PAGE_ACTION_PRICE_TRACKING, "action_chip", "true");
        testValues.addFeatureFlagOverride(
                ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2, true);
        FeatureList.setTestValues(testValues);

        PriceTrackingButtonController priceTrackingButtonController =
                new PriceTrackingButtonController(mMockTabSupplier, mMockModalDialogManager,
                        mMockBottomSheetController, mock(Drawable.class),
                        mMockTabBookmarkerSupplier);
        ButtonData buttonData = priceTrackingButtonController.get(mMockTab);

        // Action chip variation should not set an IPH command builder.
        Assert.assertNull(buttonData.getButtonSpec().getIPHCommandBuilder());
    }

    @Test
    public void testPriceTrackingButtonClick() {
        PriceTrackingButtonController priceTrackingButtonController =
                new PriceTrackingButtonController(mMockTabSupplier, mMockModalDialogManager,
                        mMockBottomSheetController, mock(Drawable.class),
                        mMockTabBookmarkerSupplier);
        ButtonData buttonData = priceTrackingButtonController.get(mMockTab);

        buttonData.getButtonSpec().getOnClickListener().onClick(null);

        verify(mMockTabBookmarker).startOrModifyPriceTracking(mMockTab);
    }

    @Test
    public void testPriceTrackingButton_IsDisabledWhenBottomSheetAppears() {
        PriceTrackingButtonController priceTrackingButtonController =
                new PriceTrackingButtonController(mMockTabSupplier, mMockModalDialogManager,
                        mMockBottomSheetController, mock(Drawable.class),
                        mMockTabBookmarkerSupplier);
        ButtonDataProvider.ButtonDataObserver buttonDataObserver =
                Mockito.mock(ButtonDataProvider.ButtonDataObserver.class);
        priceTrackingButtonController.addObserver(buttonDataObserver);
        ButtonData buttonData = priceTrackingButtonController.get(mMockTab);

        // The controller should have registered an observer to listen to bottom sheet events.
        verify(mMockBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());

        mBottomSheetObserverCaptor.getValue().onSheetStateChanged(
                SheetState.FULL, StateChangeReason.NONE);

        Assert.assertFalse(buttonData.isEnabled());
        verify(buttonDataObserver).buttonDataChanged(true);
    }

    @Test
    public void testPriceTrackingButton_IsReenabledWhenBottomSheetDismissed() {
        PriceTrackingButtonController priceTrackingButtonController =
                new PriceTrackingButtonController(mMockTabSupplier, mMockModalDialogManager,
                        mMockBottomSheetController, mock(Drawable.class),
                        mMockTabBookmarkerSupplier);

        ButtonDataProvider.ButtonDataObserver buttonDataObserver =
                Mockito.mock(ButtonDataProvider.ButtonDataObserver.class);
        priceTrackingButtonController.addObserver(buttonDataObserver);
        ButtonData buttonData = priceTrackingButtonController.get(mMockTab);

        // The controller should have registered an observer to listen to bottom sheet events.
        verify(mMockBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());

        // Show bottom sheet to disable button.
        mBottomSheetObserverCaptor.getValue().onSheetStateChanged(
                SheetState.FULL, StateChangeReason.NONE);

        // Close the bottom sheet, button should be enabled again.
        mBottomSheetObserverCaptor.getValue().onSheetStateChanged(
                SheetState.HIDDEN, StateChangeReason.NONE);

        // After the bottom sheet is closed the button should be enabled.
        Assert.assertTrue(buttonData.isEnabled());
        // We should have notified of changes twice (when disabled and when enabled again).
        verify(buttonDataObserver, times(2)).buttonDataChanged(true);
    }
}
