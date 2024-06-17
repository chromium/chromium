// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.price_insights;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.bookmarks.TabBookmarker;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonData;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modaldialog.ModalDialogManager;

@RunWith(BaseRobolectricTestRunner.class)
public class PriceInsightsButtonControllerTest {

    @Mock private Tab mMockTab;
    @Mock private Supplier<TabBookmarker> mMockTabBookmarkerSupplier;
    @Mock private Supplier<Tab> mMockTabSupplier;
    @Mock private ModalDialogManager mMockModalDialogManager;
    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private SnackbarManager mMockSnackbarManager;

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
                mMockModalDialogManager,
                mMockBottomSheetController,
                mMockSnackbarManager,
                mock(Drawable.class));
    }

    @Test
    public void testPriceInsightsButtonClicked_controllerInit() {
        PriceInsightsButtonController buttonController = createButtonController();
        ButtonData buttonData = buttonController.get(mMockTab);
        buttonData.getButtonSpec().getOnClickListener().onClick(null);

        // TODO(b/336825059): Test price insights bottom sheet controller is presented.
    }
}
