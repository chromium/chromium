// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.payments_churned_users;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link AutofillPaymentsChurnedUsersBottomSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillPaymentsChurnedUsersBottomSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private BottomSheetContent mBottomSheetContent;
    @Mock private AutofillPaymentsChurnedUsersBottomSheetCoordinator.Delegate mDelegate;

    private PropertyModel mModel;
    private AutofillPaymentsChurnedUsersBottomSheetMediator mMediator;

    @Before
    public void setUp() {
        mModel =
                new PropertyModel.Builder(
                                AutofillPaymentsChurnedUsersBottomSheetProperties.ALL_KEYS)
                        .build();
        mMediator =
                new AutofillPaymentsChurnedUsersBottomSheetMediator(
                        mBottomSheetController, mBottomSheetContent, mModel, mDelegate);
    }

    @Test
    public void testRequestShowContent() {
        when(mBottomSheetController.requestShowContent(
                        eq(mBottomSheetContent), /* animate= */ eq(true)))
                .thenReturn(true);

        mMediator.requestShowContent();

        verify(mBottomSheetController)
                .requestShowContent(eq(mBottomSheetContent), /* animate= */ eq(true));
        verify(mBottomSheetController).addObserver(mMediator);
    }

    @Test
    public void testRequestShowContent_afterDestroy_doesNothing() {
        mMediator.destroy();

        mMediator.requestShowContent();

        verify(mBottomSheetController, times(0))
                .requestShowContent(eq(mBottomSheetContent), /* animate= */ eq(true));
    }

    @Test
    public void testRequestShowContent_whenRequestFails_destroysMediator() {
        when(mBottomSheetController.requestShowContent(
                        eq(mBottomSheetContent), /* animate= */ eq(true)))
                .thenReturn(false);

        mMediator.requestShowContent();

        verify(mBottomSheetController, times(0)).addObserver(mMediator);
        verify(mDelegate).onUiNotShown();
        verify(mBottomSheetController)
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(false),
                        eq(BottomSheetController.StateChangeReason.NONE));
        verify(mBottomSheetController).removeObserver(mMediator);
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
    public void testDestroy_calledMultipleTimes_isIdempotent() {
        mMediator.destroy();
        mMediator.destroy();

        verify(mBottomSheetController)
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(false),
                        eq(BottomSheetController.StateChangeReason.NONE));
        verify(mBottomSheetController).removeObserver(mMediator);
    }

    @Test
    public void testOnSheetClosed_userDismissalReasons() {
        for (@StateChangeReason
        int reason :
                List.of(
                        BottomSheetController.StateChangeReason.SWIPE,
                        BottomSheetController.StateChangeReason.BACK_PRESS,
                        BottomSheetController.StateChangeReason.TAP_SCRIM)) {
            BottomSheetController controller = mock(BottomSheetController.class);
            AutofillPaymentsChurnedUsersBottomSheetCoordinator.Delegate delegate =
                    mock(AutofillPaymentsChurnedUsersBottomSheetCoordinator.Delegate.class);
            AutofillPaymentsChurnedUsersBottomSheetMediator mediator =
                    new AutofillPaymentsChurnedUsersBottomSheetMediator(
                            controller, mBottomSheetContent, mModel, delegate);

            mediator.onSheetClosed(reason);

            verify(delegate).onUiDismissed();
            verify(controller)
                    .hideContent(
                            eq(mBottomSheetContent),
                            /* animate= */ eq(false),
                            eq(BottomSheetController.StateChangeReason.NONE));
            verify(controller).removeObserver(mediator);
        }
    }

    @Test
    public void testOnSheetClosed_noneOrSystemReason() {
        mMediator.onSheetClosed(BottomSheetController.StateChangeReason.NONE);

        verify(mDelegate).onUiDismissed();
        verify(mBottomSheetController)
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(false),
                        eq(BottomSheetController.StateChangeReason.NONE));
        verify(mBottomSheetController).removeObserver(mMediator);
    }

    @Test
    public void testOnAcceptClicked_showsLoadingStateAndHidesContentAfterDelay() {
        mModel.get(AutofillPaymentsChurnedUsersBottomSheetProperties.ON_ACCEPT_CLICKED).run();

        assertThat(
                mModel.get(AutofillPaymentsChurnedUsersBottomSheetProperties.SHOW_LOADING_STATE),
                equalTo(true));
        verify(mDelegate).onUiAccepted();
        verify(mDelegate, times(0)).onShowConfirmation();
        verify(mBottomSheetController, times(0))
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));

        // Once the full 1000ms delay elapses, hideContent is called to start the close animation,
        // and onShowConfirmation waits until the sheet reaches SheetState.HIDDEN.
        ShadowLooper.idleMainLooper(
                AutofillPaymentsChurnedUsersBottomSheetMediator.LOADING_DELAY_MS,
                TimeUnit.MILLISECONDS);
        verify(mBottomSheetController)
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
        verify(mDelegate, times(0)).onShowConfirmation();

        // Non-HIDDEN state changes (e.g. SCROLLING during close animation) and onSheetClosed
        // do not prematurely trigger confirmation or destroy the observer before HIDDEN.
        mMediator.onSheetStateChanged(
                SheetState.SCROLLING, BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
        mMediator.onSheetClosed(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
        verify(mDelegate, times(0)).onUiDismissed();
        verify(mDelegate, times(0)).onShowConfirmation();

        mMediator.onSheetStateChanged(
                SheetState.HIDDEN, BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
        verify(mDelegate).onShowConfirmation();
        verify(mBottomSheetController).removeObserver(mMediator);
    }

    @Test
    public void testOnAcceptClicked_afterDestroy_doesNothing() {
        mMediator.destroy();

        mModel.get(AutofillPaymentsChurnedUsersBottomSheetProperties.ON_ACCEPT_CLICKED).run();

        verify(mDelegate, times(0)).onUiAccepted();
        verify(mDelegate, times(0)).onShowConfirmation();
        verify(mBottomSheetController, times(0))
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testOnAcceptClicked_destroyedBeforeDelay_cancelsPendingHideAndConfirmation() {
        mModel.get(AutofillPaymentsChurnedUsersBottomSheetProperties.ON_ACCEPT_CLICKED).run();
        verify(mDelegate).onUiAccepted();

        mMediator.destroy();

        ShadowLooper.idleMainLooper(
                AutofillPaymentsChurnedUsersBottomSheetMediator.LOADING_DELAY_MS,
                TimeUnit.MILLISECONDS);

        verify(mDelegate, times(0)).onShowConfirmation();
        verify(mBottomSheetController, times(0))
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testOnAcceptClicked_ignoresSubsequentAcceptAndCancelClicks() {
        mModel.get(AutofillPaymentsChurnedUsersBottomSheetProperties.ON_ACCEPT_CLICKED).run();
        mModel.get(AutofillPaymentsChurnedUsersBottomSheetProperties.ON_ACCEPT_CLICKED).run();
        mModel.get(AutofillPaymentsChurnedUsersBottomSheetProperties.ON_CANCEL_CLICKED).run();

        verify(mDelegate, times(1)).onUiAccepted();
        verify(mDelegate, times(0)).onUiCanceled();

        ShadowLooper.idleMainLooper(
                AutofillPaymentsChurnedUsersBottomSheetMediator.LOADING_DELAY_MS,
                TimeUnit.MILLISECONDS);
        verify(mBottomSheetController)
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
        mMediator.onSheetStateChanged(
                SheetState.HIDDEN, BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
        verify(mDelegate).onShowConfirmation();
    }

    @Test
    public void testOnCancelClicked() {
        mModel.get(AutofillPaymentsChurnedUsersBottomSheetProperties.ON_CANCEL_CLICKED).run();

        verify(mDelegate).onUiCanceled();
        verify(mBottomSheetController)
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testOnCancelClicked_afterDestroy_doesNothing() {
        mMediator.destroy();

        mModel.get(AutofillPaymentsChurnedUsersBottomSheetProperties.ON_CANCEL_CLICKED).run();

        verify(mDelegate, times(0)).onUiCanceled();
        verify(mBottomSheetController, times(0))
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testOnSheetClosed_afterAccept_doesNotCallOnUiDismissedAndCancelsPendingHide() {
        mModel.get(AutofillPaymentsChurnedUsersBottomSheetProperties.ON_ACCEPT_CLICKED).run();
        verify(mDelegate).onUiAccepted();

        mMediator.onSheetClosed(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
        verify(mDelegate, times(0)).onUiDismissed();

        // Idling the looper after early sheet closure should not trigger a duplicate animated hide
        // or show confirmation.
        ShadowLooper.idleMainLooper(
                AutofillPaymentsChurnedUsersBottomSheetMediator.LOADING_DELAY_MS,
                TimeUnit.MILLISECONDS);
        verify(mDelegate, times(0)).onShowConfirmation();
        verify(mBottomSheetController, times(0))
                .hideContent(
                        eq(mBottomSheetContent),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testOnSheetClosed_afterCancel_doesNotCallDelegateOnUiDismissed() {
        mModel.get(AutofillPaymentsChurnedUsersBottomSheetProperties.ON_CANCEL_CLICKED).run();
        verify(mDelegate).onUiCanceled();

        mMediator.onSheetClosed(BottomSheetController.StateChangeReason.SWIPE);
        verify(mDelegate, times(0)).onUiAccepted();
        verify(mDelegate, times(0)).onUiDismissed();
    }
}
