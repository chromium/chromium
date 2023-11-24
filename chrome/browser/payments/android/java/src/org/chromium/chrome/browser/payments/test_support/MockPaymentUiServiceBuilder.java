// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.test_support;

import org.mockito.Mockito;

import org.chromium.chrome.browser.payments.ui.PaymentUiService;
import org.chromium.components.payments.PaymentApp;

import java.util.ArrayList;
import java.util.List;

/** The builder of the mock PaymentUiService. */
public class MockPaymentUiServiceBuilder {
    private final PaymentUiService mPaymentUiService;
    private final List<PaymentApp> mPaymentApps = new ArrayList<>();

    public static MockPaymentUiServiceBuilder defaultBuilder() {
        return new MockPaymentUiServiceBuilder();
    }

    public MockPaymentUiServiceBuilder() {
        mPaymentUiService = Mockito.mock(PaymentUiService.class);
        Mockito.doReturn(null)
                .when(mPaymentUiService)
                .buildPaymentRequestUI(
                        Mockito.anyBoolean(), Mockito.any(), Mockito.any(), Mockito.any());
        Mockito.doAnswer(
                        (args) -> {
                            mPaymentApps.addAll(args.getArgument(0));
                            return null;
                        })
                .when(mPaymentUiService)
                .setPaymentApps(Mockito.any());
        Mockito.doAnswer((args) -> mPaymentApps.size() > 0)
                .when(mPaymentUiService)
                .hasAvailableApps();
        Mockito.doAnswer((args) -> mPaymentApps).when(mPaymentUiService).getPaymentApps();
        Mockito.doAnswer((args) -> mPaymentApps.get(0))
                .when(mPaymentUiService)
                .getSelectedPaymentApp();
    }

    public MockPaymentUiServiceBuilder setBuildPaymentRequestUIResult(String result) {
        Mockito.doReturn(result)
                .when(mPaymentUiService)
                .buildPaymentRequestUI(
                        Mockito.anyBoolean(), Mockito.any(), Mockito.any(), Mockito.any());
        return this;
    }

    public PaymentUiService build() {
        return mPaymentUiService;
    }

    public MockPaymentUiServiceBuilder setHasAvailableApps(boolean hasAvailableApps) {
        Mockito.doReturn(hasAvailableApps).when(mPaymentUiService).hasAvailableApps();
        return this;
    }
}
