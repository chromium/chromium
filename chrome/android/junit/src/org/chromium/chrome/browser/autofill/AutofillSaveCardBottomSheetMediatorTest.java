// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;
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
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.autofill.payments.AutofillSaveCardUiInfo;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;

import java.util.function.Consumer;

@RunWith(BaseRobolectricTestRunner.class)
public final class AutofillSaveCardBottomSheetMediatorTest {
    private static final String HTTPS_EXAMPLE_TEST = "https://example.test";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private AutofillSaveCardBottomSheetMediator mMediator;

    @Mock
    private AutofillSaveCardBottomSheetContent mBottomSheetContent;
    private AutofillSaveCardUiInfo mUiInfo;
    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private LayoutStateProvider mLayoutStateProvider;
    @Mock
    private TabModel mTabModel;
    @Mock
    private Consumer<String> mOnUiLegalMessageUrlClicked;
    @Mock
    private AutofillSaveCardBottomSheetBridge mBridge;

    @Before
    public void setUp() {
        mMediator = new AutofillSaveCardBottomSheetMediator(mBottomSheetContent, mUiInfo,
                mBottomSheetController, mLayoutStateProvider, mTabModel,
                mOnUiLegalMessageUrlClicked, mBridge);
    }

    @Test
    public void testConstructor_setsUpBottomSheetContentAndAddsObservers() {
        verify(mBottomSheetContent).setDelegate(mMediator);
        verify(mBottomSheetContent).setUiInfo(mUiInfo);
        verify(mBottomSheetController).addObserver(mMediator);
        verify(mLayoutStateProvider).addObserver(mMediator);
        verify(mTabModel).addObserver(mMediator);
    }

    @Test
    public void testRequestShow_showsContentAndCallsOnUiShown_whenShown() {
        when(mBottomSheetController.requestShowContent(any(), /* animate= */ anyBoolean()))
                .thenReturn(true);

        mMediator.requestShowContent();

        verify(mBottomSheetController).requestShowContent(mBottomSheetContent, /* animate= */ true);
        verify(mBridge).onUiShown();
    }

    @Test
    public void testRequestShow_callsOnUiIgnored_whenNotShown() {
        when(mBottomSheetController.requestShowContent(any(), /* animate= */ anyBoolean()))
                .thenReturn(false);

        mMediator.requestShowContent();

        verify(mBridge, never()).onUiShown();
        verify(mBridge).onUiIgnored();
    }

    @Test
    public void testDestroy_hidesBottomSheetContent() {
        mMediator.destroy();

        verify(mBottomSheetController).hideContent(mBottomSheetContent, /* animate= */ false);
    }

    @Test
    public void testDestroy_callsOnUiIgnoredAndRemovesObservers() {
        mMediator.destroy();

        verify(mBridge).onUiIgnored();
        verify(mBottomSheetController).removeObserver(mMediator);
        verify(mLayoutStateProvider).removeObserver(mMediator);
        verify(mTabModel).removeObserver(mMediator);
    }

    @Test
    public void testDestroy_doesNotCallOnUiIgnored_afterDestroy() {
        mMediator.destroy();
        verify(mBridge).onUiIgnored();

        mMediator.destroy();

        verifyNoMoreInteractions(mBridge);
    }

    @Test
    public void testDidClickLegalMessageUrl_invokesCallback() {
        mMediator.didClickLegalMessageUrl(HTTPS_EXAMPLE_TEST);

        verify(mOnUiLegalMessageUrlClicked).accept(HTTPS_EXAMPLE_TEST);
    }

    @Test
    public void testDidClickConfirm_hidesBottomSheetAndCallsOnUiAccepted() {
        mMediator.didClickConfirm();

        verify(mBottomSheetController)
                .hideContent(mBottomSheetContent,
                        /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        verify(mBridge).onUiAccepted();
    }

    @Test
    public void testDidClickCancel_hidesBottomSheetAndCallsOnUiCanceled() {
        mMediator.didClickCancel();

        verify(mBottomSheetController)
                .hideContent(mBottomSheetContent,
                        /* animate= */ true, StateChangeReason.INTERACTION_COMPLETE);
        verify(mBridge).onUiCanceled();
    }

    @Test
    public void testOnSheetClosed_callsBridgeOnUiCanceled_whenBackPress() {
        mMediator.onSheetClosed(StateChangeReason.BACK_PRESS);

        verify(mBridge).onUiCanceled();
    }

    @Test
    public void testOnSheetClosed_doesNotCallBridge_whenInteractionComplete() {
        mMediator.onSheetClosed(StateChangeReason.INTERACTION_COMPLETE);

        verifyNoInteractions(mBridge);
    }

    @Test
    public void testOnSheetClosed_callsBrigeIgnored_whenPromoteTab() {
        mMediator.onSheetClosed(StateChangeReason.PROMOTE_TAB);

        verify(mBridge).onUiIgnored();
    }

    @Test
    public void testOnStartedShowing_callsBridgeOnUiIgnored_whenNotBrowsing() {
        mMediator.onStartedShowing(LayoutType.TAB_SWITCHER);

        verify(mBridge).onUiIgnored();
    }

    @Test
    public void testOnStartedShowing_doesNotCallBridge_whenIsBrowsing() {
        mMediator.onStartedShowing(LayoutType.BROWSING);

        verifyNoInteractions(mBridge);
    }

    @Test
    public void testOnStartedShowing_hidesBottomSheetContent_whenNotBrowsing() {
        mMediator.onStartedShowing(LayoutType.TAB_SWITCHER);

        verify(mBottomSheetController).hideContent(mBottomSheetContent, /* animate= */ true);
    }

    @Test
    public void testOnStartedShowing_doesNotHideBottomSheet_whenBrowsing() {
        mMediator.onStartedShowing(LayoutType.BROWSING);

        verify(mBottomSheetController, never())
                .hideContent(eq(mBottomSheetContent), /* animate= */ anyBoolean());
        verify(mBottomSheetController, never())
                .hideContent(eq(mBottomSheetContent),
                        /* animate= */ anyBoolean(),
                        /* reason= */ anyInt());
    }
}
