// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.notNullValue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.widget.Button;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Integration tests for the Autofill Wallet Reminder Notice bottom sheet module. */
@RunWith(BaseRobolectricTestRunner.class)
public class AutofillWalletReminderNoticeBottomSheetModuleTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;

    private Activity mActivity;
    private AutofillWalletReminderNoticeBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new AutofillWalletReminderNoticeBottomSheetCoordinator(
                        mActivity, mBottomSheetController);
    }

    @Test
    public void testRequestShowContent() {
        mCoordinator.requestShowContent();

        verify(mBottomSheetController)
                .requestShowContent(
                        any(AutofillWalletReminderNoticeBottomSheetContent.class),
                        /* animate= */ eq(true));
    }

    @Test
    public void testInitialModelValues() {
        assertThat(
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillWalletReminderNoticeBottomSheetProperties.TITLE),
                equalTo(mActivity.getString(R.string.autofill_wallet_reminder_notice_title)));
        assertThat(
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(
                                AutofillWalletReminderNoticeBottomSheetProperties
                                        .ON_GOT_IT_CLICK_ACTION),
                notNullValue());
    }

    @Test
    public void testClickGotItButton() {
        mCoordinator.requestShowContent();
        Button gotItButton =
                mCoordinator
                        .getContentViewForTesting()
                        .findViewById(R.id.wallet_reminder_button_got_it);
        assertThat(gotItButton, notNullValue());
        gotItButton.performClick();

        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillWalletReminderNoticeBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testDestroy() {
        mCoordinator.destroy();

        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillWalletReminderNoticeBottomSheetContent.class),
                        /* animate= */ eq(false),
                        eq(BottomSheetController.StateChangeReason.NONE));
        verify(mBottomSheetController)
                .removeObserver(any(AutofillWalletReminderNoticeBottomSheetMediator.class));
    }
}
