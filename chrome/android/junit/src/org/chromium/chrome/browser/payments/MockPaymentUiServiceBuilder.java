// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.mockito.Mockito;

import org.chromium.chrome.browser.payments.ui.PaymentUiService;
import org.chromium.components.payments.PaymentApp;

import java.util.ArrayList;
import java.util.List;

/** The builder of the mock PaymentUiService. */
public class MockPaymentUiServiceBuilder {
    PaymentUiService mPaymentUiService;

    /* package */ static MockPaymentUiServiceBuilder defaultBuilder(PaymentApp app) {
        return new MockPaymentUiServiceBuilder(app);
    }

    /* package */ MockPaymentUiServiceBuilder(PaymentApp app) {
        mPaymentUiService = Mockito.mock(PaymentUiService.class);
        Mockito.doReturn(null)
                .when(mPaymentUiService)
                .buildPaymentRequestUI(Mockito.anyBoolean(), Mockito.any(), Mockito.any(),
                        Mockito.any(), Mockito.any());
        Mockito.doReturn(true).when(mPaymentUiService).hasAvailableApps();
        List<PaymentApp> apps = new ArrayList<>();
        apps.add(app);
        Mockito.doReturn(apps).when(mPaymentUiService).getPaymentApps();
        Mockito.doReturn(app).when(mPaymentUiService).getSelectedPaymentApp();
    }

    /* package */ MockPaymentUiServiceBuilder setBuildPaymentRequestUIResult(String result) {
        Mockito.doReturn(result)
                .when(mPaymentUiService)
                .buildPaymentRequestUI(Mockito.anyBoolean(), Mockito.any(), Mockito.any(),
                        Mockito.any(), Mockito.any());
        return this;
    }

    /* package */ PaymentUiService build() {
        return mPaymentUiService;
    }

    public MockPaymentUiServiceBuilder setHasAvailableApps(boolean hasAvailableApps) {
        Mockito.doReturn(hasAvailableApps).when(mPaymentUiService).hasAvailableApps();
        return this;
    }
}
