// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.notNullValue;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.PaymentInstrument;
import org.chromium.components.autofill.payments.PaymentRail;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Unit tests for {@link FacilitatedPaymentsPaymentMethodsViewBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FacilitatedPaymentsPaymentMethodsViewBridgeTest {
    private static final BankAccount[] BANK_ACCOUNTS = {
        new BankAccount.Builder()
                .setPaymentInstrument(
                        new PaymentInstrument.Builder()
                                .setInstrumentId(100)
                                .setNickname("nickname1")
                                .setSupportedPaymentRails(new int[] {PaymentRail.PIX})
                                .build())
                .setBankName("bankName1")
                .setAccountNumberSuffix("1111")
                .setAccountType(AccountType.CHECKING)
                .build(),
        new BankAccount.Builder()
                .setPaymentInstrument(
                        new PaymentInstrument.Builder()
                                .setInstrumentId(200)
                                .setNickname("nickname2")
                                .setSupportedPaymentRails(new int[] {PaymentRail.PIX})
                                .build())
                .setBankName("bankName2")
                .setAccountNumberSuffix("2222")
                .setAccountType(AccountType.CHECKING)
                .build()
    };

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContents mWebContents;
    @Mock private ManagedBottomSheetController mBottomSheetController;
    @Mock private FacilitatedPaymentsPaymentMethodsComponent.Delegate mDelegateMock;
    @Mock private Profile mProfile;

    private FacilitatedPaymentsPaymentMethodsViewBridge mViewBridge;
    private WindowAndroid mWindow;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Context mApplicationContext = ApplicationProvider.getApplicationContext();
        mWindow = new WindowAndroid(mApplicationContext);
        BottomSheetControllerFactory.attach(mWindow, mBottomSheetController);
        mViewBridge =
                FacilitatedPaymentsPaymentMethodsViewBridge.create(
                        mDelegateMock, mWindow, mProfile);
    }

    @After
    public void tearDown() {
        BottomSheetControllerFactory.detach(mBottomSheetController);
        mWindow.destroy();
    }

    @Test
    @SmallTest
    public void create_nullProfile() {
        mViewBridge =
                FacilitatedPaymentsPaymentMethodsViewBridge.create(
                        mDelegateMock, mWindow, /* profile= */ null);

        assertNull(mViewBridge);
    }

    @Test
    @SmallTest
    public void create_nullWindowAndroid() {
        mViewBridge =
                FacilitatedPaymentsPaymentMethodsViewBridge.create(
                        mDelegateMock, /* windowAndroid= */ null, mProfile);

        assertNull(mViewBridge);
    }

    @Test
    @SmallTest
    public void create_nullBottomSheetController() {
        BottomSheetControllerFactory.detach(mBottomSheetController);

        mViewBridge =
                FacilitatedPaymentsPaymentMethodsViewBridge.create(
                        mDelegateMock, mWindow, mProfile);

        assertNull(mViewBridge);
    }

    @Test
    @SmallTest
    public void requestShowContent_callsControllerRequestShowContent() {
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);

        mViewBridge.requestShowContent(BANK_ACCOUNTS);

        verify(mBottomSheetController)
                .requestShowContent(
                        any(FacilitatedPaymentsPaymentMethodsView.class), /* animate= */ eq(true));
    }

    @Test
    @SmallTest
    public void requestShowContent_bottomSheetContentImplIsStubbed() {
        when(mWebContents.getTopLevelNativeWindow()).thenReturn(mWindow);

        mViewBridge.requestShowContent(BANK_ACCOUNTS);

        ArgumentCaptor<FacilitatedPaymentsPaymentMethodsView> contentCaptor =
                ArgumentCaptor.forClass(FacilitatedPaymentsPaymentMethodsView.class);
        verify(mBottomSheetController)
                .requestShowContent(contentCaptor.capture(), /* animate= */ anyBoolean());
        FacilitatedPaymentsPaymentMethodsView content = contentCaptor.getValue();
        assertThat(content.getContentView(), notNullValue());
        assertThat(
                content.getSheetContentDescriptionStringId(),
                equalTo(R.string.pix_payment_methods_bottom_sheet_content_description));
        assertThat(
                content.getSheetFullHeightAccessibilityStringId(),
                equalTo(R.string.pix_payment_methods_bottom_sheet_full_height));
        assertThat(
                content.getSheetClosedAccessibilityStringId(),
                equalTo(R.string.pix_payment_methods_bottom_sheet_closed));
    }
}
