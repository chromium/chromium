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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.autofill.SaveIbanPromptOffer;
import org.chromium.components.autofill.SaveIbanPromptResult;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

@RunWith(BaseRobolectricTestRunner.class)
public final class AutofillSaveIbanBottomSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutofillSaveIbanBottomSheetBridge mDelegate;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private TabModel mTabModel;

    private AutofillSaveIbanBottomSheetContent mBottomSheetContent;
    private AutofillSaveIbanBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mBottomSheetContent = new AutofillSaveIbanBottomSheetContent(null, null);
        mMediator =
                new AutofillSaveIbanBottomSheetMediator(
                        mDelegate,
                        mBottomSheetContent,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel,
                        /* isServerSave= */ true);
    }

    @Test
    public void testRequestShowContent_showsContent() {
        HistogramWatcher promptOfferHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillSaveIbanBottomSheetMediator.SAVE_IBAN_PROMPT_OFFER_HISTOGRAM
                                + ".Upload.FirstShow",
                        SaveIbanPromptOffer.SHOWN);

        when(mBottomSheetController.requestShowContent(
                        any(AutofillSaveIbanBottomSheetContent.class), /* animate= */ eq(true)))
                .thenReturn(true);

        mMediator.requestShowContent();

        verify(mBottomSheetController).requestShowContent(mBottomSheetContent, /* animate= */ true);
        promptOfferHistogramWatcher.assertExpected();
    }

    @Test
    public void testRequestShowContent_showsContent_ignored() {
        HistogramWatcher promptOfferHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillSaveIbanBottomSheetMediator.SAVE_IBAN_PROMPT_RESULT_HISTOGRAM
                                + ".Upload.FirstShow",
                        SaveIbanPromptResult.UNKNOWN);

        when(mBottomSheetController.requestShowContent(
                        any(AutofillSaveIbanBottomSheetContent.class), /* animate= */ eq(true)))
                .thenReturn(false);

        mMediator.requestShowContent();

        verify(mBottomSheetController).requestShowContent(mBottomSheetContent, /* animate= */ true);
        promptOfferHistogramWatcher.assertExpected();
    }

    @Test
    public void testHide_hidesBottomSheetContent() {
        mMediator.hide(BottomSheetController.StateChangeReason.NONE);

        verify(mBottomSheetController)
                .hideContent(
                        mBottomSheetContent,
                        /* animate= */ true,
                        BottomSheetController.StateChangeReason.NONE);
    }

    @Test
    public void testOnAccepted_emptyNickname() {
        HistogramWatcher promptResultHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillSaveIbanBottomSheetMediator.SAVE_IBAN_PROMPT_RESULT_HISTOGRAM
                                + ".Upload.FirstShow",
                        SaveIbanPromptResult.ACCEPTED);
        HistogramWatcher nicknameHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillSaveIbanBottomSheetMediator.SAVE_IBAN_PROMPT_RESULT_HISTOGRAM
                                + ".Upload.SavedWithNickname",
                        false);

        mMediator.onAccepted("");

        promptResultHistogramWatcher.assertExpected();
        nicknameHistogramWatcher.assertExpected();
    }

    @Test
    public void testOnAccepted_nicknameAvailable() {
        HistogramWatcher promptResultHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillSaveIbanBottomSheetMediator.SAVE_IBAN_PROMPT_RESULT_HISTOGRAM
                                + ".Upload.FirstShow",
                        SaveIbanPromptResult.ACCEPTED);
        HistogramWatcher nicknameHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillSaveIbanBottomSheetMediator.SAVE_IBAN_PROMPT_RESULT_HISTOGRAM
                                + ".Upload.SavedWithNickname",
                        true);

        mMediator.onAccepted("My iban");

        promptResultHistogramWatcher.assertExpected();
        nicknameHistogramWatcher.assertExpected();
    }

    @Test
    public void testOnCanceled() {
        HistogramWatcher promptResultHistogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        AutofillSaveIbanBottomSheetMediator.SAVE_IBAN_PROMPT_RESULT_HISTOGRAM
                                + ".Upload.FirstShow",
                        SaveIbanPromptResult.CANCELLED);

        mMediator.onCanceled();

        promptResultHistogramWatcher.assertExpected();
    }
}
