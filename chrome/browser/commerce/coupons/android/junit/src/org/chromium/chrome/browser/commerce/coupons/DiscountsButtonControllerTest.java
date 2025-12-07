// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce.coupons;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.commerce.CommerceBottomSheetContentController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Unit tests for {@link DiscountsButtonController} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DiscountsButtonControllerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mMockTab;
    @Mock private BottomSheetController mMockBottomSheetController;
    @Mock private ModalDialogManager mMockModalDialogManager;
    @Mock private CommerceBottomSheetContentController mCommerceBottomSheetContentController;

    @Captor private ArgumentCaptor<BottomSheetObserver> mBottomSheetObserverCaptor;

    private DiscountsButtonController mDiscountsButtonController;

    @Before
    public void setUp() {
        Context context = Robolectric.buildActivity(Activity.class).get();

        mDiscountsButtonController =
                new DiscountsButtonController(
                        context,
                        () -> mMockTab,
                        mMockModalDialogManager,
                        mMockBottomSheetController,
                        () -> mCommerceBottomSheetContentController);
    }

    @Test
    public void testOnClick_requestShowContent() {
        ButtonData buttonData = mDiscountsButtonController.get(mMockTab);
        buttonData.getButtonSpec().getOnClickListener().onClick(null);
        verify(mCommerceBottomSheetContentController, times(1)).requestShowContent();
    }

    @Test
    public void testOnClick_buttonDisabled() {
        ButtonData buttonData = mDiscountsButtonController.get(mMockTab);
        verify(mMockBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        assertTrue(buttonData.isEnabled());

        buttonData.getButtonSpec().getOnClickListener().onClick(null);
        mBottomSheetObserverCaptor
                .getValue()
                .onSheetStateChanged(SheetState.FULL, StateChangeReason.NONE);

        assertFalse(buttonData.isEnabled());
    }

    @Test
    public void testOnDestroy() {
        ButtonData buttonData = mDiscountsButtonController.get(mMockTab);
        verify(mMockBottomSheetController).addObserver(mBottomSheetObserverCaptor.capture());
        assertTrue(buttonData.isEnabled());

        mDiscountsButtonController.destroy();

        verify(mMockBottomSheetController).removeObserver(mBottomSheetObserverCaptor.capture());
        assertTrue(buttonData.isEnabled());
    }
}
