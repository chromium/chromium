// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.graphics.drawable.Drawable;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppType;
import org.chromium.components.payments.SupportedDelegations;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;
import org.chromium.payments.mojom.PaymentShippingOption;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** A mock payment app for tests. */
/* package */ class MockPaymentApp extends PaymentApp {
    private final Set<String> mSupportedMethodNames;
    private final SupportedDelegations mSupportedDelegations;

    /**
     * Constructs a mock payment app for tests.
     *
     * @param identifier The identifier of the payment app. Must be unique.
     * @param name The name of the mocked payment app.
     * @param icon The icon that should be used for this mock payment app. Can be null.
     * @param supportedMethodNames The supported payment methods of the mock payment app.
     * @param supportedDelegations The supported delegations of the mock payment app.
     */
    /* package */ MockPaymentApp(
            String identifier,
            @Nullable String name,
            Drawable icon,
            String[] supportedMethodNames,
            @Nullable SupportedDelegations supportedDelegations) {
        super(identifier, name == null ? "" : name, "test@bobpay.test", icon);
        mSupportedMethodNames = new HashSet<>(Arrays.asList(supportedMethodNames));
        mSupportedDelegations = supportedDelegations;
    }

    @Override
    public Set<String> getInstrumentMethodNames() {
        return mSupportedMethodNames;
    }

    @Override
    public boolean handlesShippingAddress() {
        return mSupportedDelegations != null && mSupportedDelegations.getShippingAddress();
    }

    @Override
    public boolean handlesPayerName() {
        return mSupportedDelegations != null && mSupportedDelegations.getPayerName();
    }

    @Override
    public boolean handlesPayerEmail() {
        return mSupportedDelegations != null && mSupportedDelegations.getPayerEmail();
    }

    @Override
    public boolean handlesPayerPhone() {
        return mSupportedDelegations != null && mSupportedDelegations.getPayerPhone();
    }

    @Override
    public void invokePaymentApp(
            String id,
            String merchantName,
            String origin,
            String iframeOrigin,
            @Nullable byte[][] certificateChain,
            Map<String, PaymentMethodData> methodDataMap,
            PaymentItem total,
            List<PaymentItem> displayItems,
            Map<String, PaymentDetailsModifier> modifiers,
            PaymentOptions paymentOptions,
            List<PaymentShippingOption> shippingOptions,
            PaymentApp.InstrumentDetailsCallback callback) {
        PostTask.postTask(
                TaskTraits.UI_DEFAULT, () -> callback.onInstrumentDetailsError("Not implemented"));
    }

    @Override
    public @PaymentAppType int getPaymentAppType() {
        return PaymentAppType.SERVICE_WORKER_APP;
    }

    @Override
    public boolean canPreselect() {
        // (https://crbug.com/1090604): Move pre-selection tests from
        // PaymentRequestServiceWorker*Test.java to android_browsertests with a real service
        // worker based payment app.
        return !TextUtils.isEmpty(getLabel()) && getDrawableIcon() != null;
    }

    @Override
    public void dismissInstrument() {}
}
