// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.notNullValue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

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
        mMediator.onGotItClicked();

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
    public void testDestroy() {
        mMediator.destroy();

        verify(mBottomSheetController).removeObserver(mMediator);
    }
}
