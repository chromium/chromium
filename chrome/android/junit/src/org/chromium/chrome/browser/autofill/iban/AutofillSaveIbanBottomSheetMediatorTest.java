// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

@RunWith(BaseRobolectricTestRunner.class)
public final class AutofillSaveIbanBottomSheetMediatorTest {
    private static final String IBAN_LABEL = "CH56 **** **** **** *800 9";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutofillSaveIbanBottomSheetBridge mBridge;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private TabModel mTabModel;

    private AutofillSaveIbanBottomSheetContent mBottomSheetContent;
    private AutofillSaveIbanBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mMediator =
                new AutofillSaveIbanBottomSheetMediator(
                        mBridge,
                        mBottomSheetContent,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel);
    }

    @Test
    public void testRequestShowContent_showsContent() {
        when(mBottomSheetController.requestShowContent(
                        any(AutofillSaveIbanBottomSheetContent.class), /* animate= */ eq(true)))
                .thenReturn(true);
        mMediator.requestShowContent(IBAN_LABEL);

        verify(mBottomSheetController).requestShowContent(mBottomSheetContent, /* animate= */ true);
    }

    @Test
    public void testDestroy_hidesBottomSheetContent() {
        mMediator.destroy();

        verify(mBottomSheetController).hideContent(mBottomSheetContent, /* animate= */ true);
    }
}
