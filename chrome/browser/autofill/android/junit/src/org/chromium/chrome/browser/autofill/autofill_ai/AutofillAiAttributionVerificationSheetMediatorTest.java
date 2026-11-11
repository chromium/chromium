// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiAttributionVerificationSheetProperties.DISMISS_HANDLER;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiAttributionVerificationSheetCoordinator.Delegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link AutofillAiAttributionVerificationSheetMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillAiAttributionVerificationSheetMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private BottomSheetContent mBottomSheetContent;
    @Mock private Delegate mDelegate;

    private PropertyModel mModel;
    private AutofillAiAttributionVerificationSheetMediator mMediator;

    @Before
    public void setUp() {
        mMediator =
                new AutofillAiAttributionVerificationSheetMediator(
                        mBottomSheetController, mBottomSheetContent, mDelegate);
        mModel = mMediator.getModel();
    }

    @Test
    public void testInitialModel() {
        assertNotNull(mModel.get(DISMISS_HANDLER));
    }

    @Test
    public void testRequestShowContent_showsAndExpandsSheetOnce() {
        when(mBottomSheetController.requestShowContent(eq(mBottomSheetContent), eq(true)))
                .thenReturn(true);

        mMediator.requestShowContent();
        mMediator.requestShowContent();

        InOrder inOrder = Mockito.inOrder(mBottomSheetController);
        inOrder.verify(mBottomSheetController)
                .requestShowContent(eq(mBottomSheetContent), /* animate= */ eq(true));
        inOrder.verify(mBottomSheetController).expandSheet();
        verify(mBottomSheetController, times(1))
                .requestShowContent(eq(mBottomSheetContent), anyBoolean());
        verify(mDelegate, never()).onDismissed();
    }

    @Test
    public void testRequestShowContent_rejectedPurgesQueueAndNotifiesDelegate() {
        when(mBottomSheetController.requestShowContent(eq(mBottomSheetContent), eq(true)))
                .thenReturn(false);

        mMediator.requestShowContent();
        mModel.get(DISMISS_HANDLER).run();

        verify(mBottomSheetController)
                .hideContent(eq(mBottomSheetContent), /* animate= */ eq(false));
        verify(mBottomSheetController, never()).expandSheet();
        verify(mDelegate, times(1)).onDismissed();
    }

    @Test
    public void
            testRequestShowContent_dismissedSynchronouslyDuringShow_notifiesDelegateWithoutExpanding() {
        when(mBottomSheetController.requestShowContent(eq(mBottomSheetContent), eq(true)))
                .thenAnswer(
                        invocation -> {
                            mModel.get(DISMISS_HANDLER).run();
                            return true;
                        });

        mMediator.requestShowContent();
        mModel.get(DISMISS_HANDLER).run();

        verify(mBottomSheetController, never()).expandSheet();
        verify(mDelegate, times(1)).onDismissed();
    }

    @Test
    public void testDismissHandler_whenShowing_notifiesDelegateOnceWithoutRehiding() {
        when(mBottomSheetController.requestShowContent(eq(mBottomSheetContent), eq(true)))
                .thenReturn(true);
        mMediator.requestShowContent();

        mModel.get(DISMISS_HANDLER).run();
        mModel.get(DISMISS_HANDLER).run();

        verify(mBottomSheetController, never()).hideContent(eq(mBottomSheetContent), anyBoolean());
        verify(mDelegate, times(1)).onDismissed();
    }

    @Test
    public void testDismissHandler_ignoredWhenNotShowing() {
        mModel.get(DISMISS_HANDLER).run();

        verify(mDelegate, never()).onDismissed();
    }
}
