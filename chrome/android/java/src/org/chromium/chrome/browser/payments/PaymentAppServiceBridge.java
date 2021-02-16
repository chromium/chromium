// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppFactoryInterface;
import org.chromium.components.payments.PaymentRequestSpec;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.RenderFrameHost;

/**
 * Native bridge for finding payment apps.
 */
public class PaymentAppServiceBridge implements PaymentAppFactoryInterface {
    private static boolean sCanMakePaymentForTesting;

    /* package */ PaymentAppServiceBridge() {}

    /**
     * Make canMakePayment() return true always for testing purpose.
     *
     * @param canMakePayment Indicates whether a SW payment app can make payment.
     */
    @VisibleForTesting
    public static void setCanMakePaymentForTesting(boolean canMakePayment) {
        sCanMakePaymentForTesting = canMakePayment;
    }

    // PaymentAppFactoryInterface implementation.
    @Override
    public void create(PaymentAppFactoryDelegate delegate) {
        if (delegate.getParams().hasClosed()
                || delegate.getParams().getRenderFrameHost().getLastCommittedURL() == null
                || delegate.getParams().getRenderFrameHost().getLastCommittedOrigin() == null
                || delegate.getParams().getWebContents().isDestroyed()) {
            return;
        }

        assert delegate.getParams().getPaymentRequestOrigin().equals(
                UrlFormatter.formatUrlForSecurityDisplay(
                        delegate.getParams().getRenderFrameHost().getLastCommittedURL(),
                        SchemeDisplay.SHOW));

        PaymentAppServiceCallback callback = new PaymentAppServiceCallback(delegate);

        PaymentAppServiceBridgeJni.get().create(delegate.getParams().getRenderFrameHost(),
                delegate.getParams().getTopLevelOrigin(), delegate.getParams().getSpec(),
                delegate.getParams().getTwaPackageName(), delegate.getParams().getMayCrawl(),
                callback);
    }

    /** Handles callbacks from native PaymentAppService. */
    public class PaymentAppServiceCallback {
        private final PaymentAppFactoryDelegate mDelegate;

        private PaymentAppServiceCallback(PaymentAppFactoryDelegate delegate) {
            mDelegate = delegate;
        }

        @CalledByNative("PaymentAppServiceCallback")
        private void onCanMakePaymentCalculated(boolean canMakePayment) {
            ThreadUtils.assertOnUiThread();
            mDelegate.onCanMakePaymentCalculated(canMakePayment || sCanMakePaymentForTesting);
        }

        @CalledByNative("PaymentAppServiceCallback")
        private void onPaymentAppCreated(PaymentApp paymentApp) {
            ThreadUtils.assertOnUiThread();
            mDelegate.onPaymentAppCreated(paymentApp);
        }

        /**
         * Called when an error has occurred.
         * @param errorMessage Developer facing error message.
         */
        @CalledByNative("PaymentAppServiceCallback")
        private void onPaymentAppCreationError(String errorMessage) {
            ThreadUtils.assertOnUiThread();
            mDelegate.onPaymentAppCreationError(errorMessage);
        }

        /**
         * Called when the factory is finished creating payment apps. Expects to be called exactly
         * once and after all onPaymentAppCreated() calls.
         */
        @CalledByNative("PaymentAppServiceCallback")
        private void onDoneCreatingPaymentApps() {
            ThreadUtils.assertOnUiThread();
            mDelegate.onDoneCreatingPaymentApps(PaymentAppServiceBridge.this);
        }
    }

    @NativeMethods
    /* package */ interface Natives {
        /**
         * Creates a native payment app service.
         * @param initiatorRenderFrameHost The host of the render frame where PaymentRequest API was
         * invoked.
         * @param topOrigin The (scheme, host, port) tuple of top level context where
         * PaymentRequest API was invoked.
         * @param spec The parameters passed into the PaymentRequest API.
         * @param twaPackageName The Android package name of the Trusted Web Activity that invoked
         * Chrome. If not running in TWA mode, then this string is null or empty.
         * @param mayCrawlForInstallablePaymentApps Whether crawling for just-in-time installable
         * payment apps is allowed.
         * @param callback The callback that receives the discovered payment apps.
         */
        void create(RenderFrameHost initiatorRenderFrameHost, String topOrigin,
                PaymentRequestSpec spec, String twaPackageName,
                boolean mayCrawlForInstallablePaymentApps, PaymentAppServiceCallback callback);
    }
}
