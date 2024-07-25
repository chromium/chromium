// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.iban;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;

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
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.autofill.payments.AutofillSaveIbanUiInfo;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

@RunWith(BaseRobolectricTestRunner.class)
public final class AutofillSaveIbanBottomSheetCoordinatorTest {
    private static final AutofillSaveIbanUiInfo TEST_IBAN_UI_INFO =
            new AutofillSaveIbanUiInfo.Builder()
                    .withAcceptText("Save")
                    .withCancelText("No thanks")
                    .withIbanLabel("FR** **** **** **** **** ***0 189")
                    .withTitleText("Save IBAN?")
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private AutofillSaveIbanBottomSheetBridge mDelegate;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private TabModel mTabModel;

    private Activity mActivity;
    private AutofillSaveIbanBottomSheetCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        // set a MaterialComponents theme which is required for the `OutlinedBox` text field.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mCoordinator =
                new AutofillSaveIbanBottomSheetCoordinator(
                        mDelegate,
                        TEST_IBAN_UI_INFO,
                        mActivity,
                        mBottomSheetController,
                        mLayoutStateProvider,
                        mTabModel);
    }

    @Test
    public void testRequestShowContent() {
        mCoordinator.requestShowContent();

        verify(mBottomSheetController)
                .requestShowContent(
                        any(AutofillSaveIbanBottomSheetContent.class), /* animate= */ eq(true));
    }

    @Test
    public void testDestroy() {
        mCoordinator.requestShowContent();
        mCoordinator.destroy();

        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillSaveIbanBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.NONE));
    }

    @Test
    public void testInitialModelValues() {
        assertEquals(
                TEST_IBAN_UI_INFO.getIbanLabel(),
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveIbanBottomSheetProperties.IBAN_LABEL));
        assertEquals(
                TEST_IBAN_UI_INFO.getTitleText(),
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveIbanBottomSheetProperties.TITLE));
        assertEquals(
                TEST_IBAN_UI_INFO.getAcceptText(),
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveIbanBottomSheetProperties.ACCEPT_BUTTON_LABEL));
        assertEquals(
                TEST_IBAN_UI_INFO.getCancelText(),
                mCoordinator
                        .getPropertyModelForTesting()
                        .get(AutofillSaveIbanBottomSheetProperties.CANCEL_BUTTON_LABEL));
    }

    @Test
    public void testClickAccept_savesEmptyNicknameIfNoneEntered() {
        mCoordinator.requestShowContent();

        mCoordinator.getAutofillSaveIbanBottomSheetViewForTesting().mAcceptButton.performClick();

        verify(mDelegate).onUiAccepted("");
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillSaveIbanBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testClickAccept_savesCorrectNickname() {
        String expectedNickname = "My IBAN";
        mCoordinator.requestShowContent();
        mCoordinator
                .getAutofillSaveIbanBottomSheetViewForTesting()
                .mNickname
                .setText(expectedNickname);

        mCoordinator.getAutofillSaveIbanBottomSheetViewForTesting().mAcceptButton.performClick();

        verify(mDelegate).onUiAccepted(expectedNickname);
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillSaveIbanBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }

    @Test
    public void testClickCancel() {
        mCoordinator.requestShowContent();
        mCoordinator.getAutofillSaveIbanBottomSheetViewForTesting().mCancelButton.performClick();

        verify(mDelegate).onUiCanceled();
        verify(mBottomSheetController)
                .hideContent(
                        any(AutofillSaveIbanBottomSheetContent.class),
                        /* animate= */ eq(true),
                        eq(BottomSheetController.StateChangeReason.INTERACTION_COMPLETE));
    }
}
