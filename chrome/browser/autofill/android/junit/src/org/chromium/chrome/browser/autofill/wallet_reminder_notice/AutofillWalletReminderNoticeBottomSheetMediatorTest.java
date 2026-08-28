// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.notNullValue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.autofill.wallet_reminder_notice.AutofillWalletReminderNoticeBottomSheetMediator.HISTOGRAM_INTERACTION;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.components.autofill.payments.WalletReminderNoticeInteraction;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Unit tests for {@link AutofillWalletReminderNoticeBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillWalletReminderNoticeBottomSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private BottomSheetContent mBottomSheetContent;

    private PropertyModel mModel;
    private AutofillWalletReminderNoticeBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mModel =
                new PropertyModel.Builder(
                                AutofillWalletReminderNoticeBottomSheetProperties.ALL_KEYS)
                        .build();
        mMediator =
                new AutofillWalletReminderNoticeBottomSheetMediator(
                        mBottomSheetController, mBottomSheetContent, mModel);
        verify(mBottomSheetController).addObserver(mMediator);
    }

    @Test
    public void testRequestShowContent() {
        mMediator.requestShowContent();

        verify(mBottomSheetController)
                .requestShowContent(eq(mBottomSheetContent), /* animate= */ eq(true));
    }

    @Test
    public void testOnGotItClicked() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_INTERACTION, WalletReminderNoticeInteraction.ACKNOWLEDGED_CTA);

        mMediator.onGotItClicked();

        histogramWatcher.assertExpected();
        verify(mBottomSheetController)
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testModelGotItClickAction_invokesOnGotItClicked() {
        Runnable action =
                mModel.get(
                        AutofillWalletReminderNoticeBottomSheetProperties.ON_GOT_IT_CLICK_ACTION);
        assertThat(action, notNullValue());

        action.run();

        verify(mBottomSheetController)
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testOnLegalMessageLinkClicked() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_INTERACTION, WalletReminderNoticeInteraction.CLICKED_LINK);

        mMediator.onLegalMessageLinkClicked();

        histogramWatcher.assertExpected();
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();

        verify(mBottomSheetController)
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(false),
                        eq(BottomSheetController.StateChangeReason.NONE));
        verify(mBottomSheetController).removeObserver(mMediator);
    }

    @Test
    public void testOnSheetClosed_userDismissalReasons_logsDismissed() {
        for (@StateChangeReason
        int reason :
                List.of(
                        BottomSheetController.StateChangeReason.SWIPE,
                        BottomSheetController.StateChangeReason.TAP_SCRIM,
                        BottomSheetController.StateChangeReason.BACK_PRESS)) {
            BottomSheetController controller = mock(BottomSheetController.class);
            var histogramWatcher =
                    HistogramWatcher.newSingleRecordWatcher(
                            HISTOGRAM_INTERACTION, WalletReminderNoticeInteraction.DISMISSED);
            PropertyModel model =
                    new PropertyModel.Builder(
                                    AutofillWalletReminderNoticeBottomSheetProperties.ALL_KEYS)
                            .build();
            AutofillWalletReminderNoticeBottomSheetMediator mediator =
                    new AutofillWalletReminderNoticeBottomSheetMediator(
                            controller, mBottomSheetContent, model);

            mediator.onSheetClosed(reason);

            histogramWatcher.assertExpected();
            verify(controller)
                    .hideContent(
                            eq(mBottomSheetContent),
                            /* animate= */ eq(false),
                            eq(BottomSheetController.StateChangeReason.NONE));
            verify(controller).removeObserver(mediator);
        }
    }

    @Test
    public void testOnSheetClosed_noneOrSystemReason_doesNotLogDismissed() {
        var histogramWatcher =
                HistogramWatcher.newBuilder().expectNoRecords(HISTOGRAM_INTERACTION).build();

        mMediator.onSheetClosed(BottomSheetController.StateChangeReason.NONE);

        histogramWatcher.assertExpected();
        verify(mBottomSheetController)
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(false),
                        eq(BottomSheetController.StateChangeReason.NONE));
        verify(mBottomSheetController).removeObserver(mMediator);
    }

    @Test
    public void testOnSheetClosed_afterGotItClicked_doesNotLogDismissed() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_INTERACTION, WalletReminderNoticeInteraction.ACKNOWLEDGED_CTA);

        mMediator.onGotItClicked();
        mMediator.onSheetClosed(BottomSheetController.StateChangeReason.SWIPE);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testOnSheetClosed_afterLegalMessageLinkClicked_doesNotLogDismissed() {
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        HISTOGRAM_INTERACTION, WalletReminderNoticeInteraction.CLICKED_LINK);

        mMediator.onLegalMessageLinkClicked();
        mMediator.onSheetClosed(BottomSheetController.StateChangeReason.SWIPE);

        histogramWatcher.assertExpected();
    }
}
