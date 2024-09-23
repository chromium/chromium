// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.graphics.Bitmap;
import android.util.Pair;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentEventResponseType;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/** Native bridge for interacting with service worker based payment apps. */
public class ServiceWorkerPaymentAppBridge {
    /** The interface for checking whether there is an installed SW payment app. */
    public static interface HasServiceWorkerPaymentAppsCallback {
        /**
         * Called to return checking result.
         *
         * @param hasPaymentApps Indicates whether there is an installed SW payment app.
         */
        public void onHasServiceWorkerPaymentAppsResponse(boolean hasPaymentApps);
    }

    /** The interface for getting all installed SW payment apps' information. */
    public static interface GetServiceWorkerPaymentAppsInfoCallback {
        /**
         * Called to return installed SW payment apps' information.
         *
         * @param appsInfo Contains all installed SW payment apps' information.
         */
        public void onGetServiceWorkerPaymentAppsInfo(Map<String, Pair<String, Bitmap>> appsInfo);
    }

    /* package */ ServiceWorkerPaymentAppBridge() {}

    /**
     * Checks whether there is a installed SW payment app.
     *
     * @param callback The callback to return result.
     */
    public static void hasServiceWorkerPaymentApps(HasServiceWorkerPaymentAppsCallback callback) {
        ThreadUtils.assertOnUiThread();

        if (!PaymentFeatureList.isEnabled(PaymentFeatureList.SERVICE_WORKER_PAYMENT_APPS)) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    new Runnable() {
                        @Override
                        public void run() {
                            callback.onHasServiceWorkerPaymentAppsResponse(false);
                        }
                    });
            return;
        }
        ServiceWorkerPaymentAppBridgeJni.get().hasServiceWorkerPaymentApps(callback);
    }

    /**
     * Gets all installed SW payment apps' information.
     *
     * @param callback The callback to return result.
     */
    public static void getServiceWorkerPaymentAppsInfo(
            GetServiceWorkerPaymentAppsInfoCallback callback) {
        ThreadUtils.assertOnUiThread();

        if (!PaymentFeatureList.isEnabled(PaymentFeatureList.SERVICE_WORKER_PAYMENT_APPS)) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    new Runnable() {
                        @Override
                        public void run() {
                            callback.onGetServiceWorkerPaymentAppsInfo(
                                    new HashMap<String, Pair<String, Bitmap>>());
                        }
                    });
            return;
        }
        ServiceWorkerPaymentAppBridgeJni.get().getServiceWorkerPaymentAppsInfo(callback);
    }

    /**
     * Notify closing the opened payment app window.
     *
     * @param paymentRequestWebContents The web contents in the opened window. Can be null.
     * @param responseType The type of response for payment event, used to decide the user-visible
     *         error message, defined in {@link PaymentEventResponseType}.
     */
    public static void onClosingPaymentAppWindow(
            @Nullable WebContents paymentRequestWebContents, int responseType) {
        if (paymentRequestWebContents == null || paymentRequestWebContents.isDestroyed()) return;
        ServiceWorkerPaymentAppBridgeJni.get()
                .onClosingPaymentAppWindow(paymentRequestWebContents, responseType);
    }

    /**
     * Called when payment handler's window is being opened.
     *
     * @param paymentRequestWebContents The web contents of the merchant's frame, cannot be null.
     * @param paymentHandlerWebContents The web contents of the payment handler, cannot be null.
     */
    public static void onOpeningPaymentAppWindow(
            WebContents paymentRequestWebContents, WebContents paymentHandlerWebContents) {
        if (paymentRequestWebContents == null || paymentRequestWebContents.isDestroyed()) return;
        ServiceWorkerPaymentAppBridgeJni.get()
                .onOpeningPaymentAppWindow(
                        /* paymentRequestWebContents= */ paymentRequestWebContents,
                        /* paymentHandlerWebContents= */ paymentHandlerWebContents);
    }

    /**
     * Get the ukm source id for the invoked payment app.
     * @param swScope The scope of the invoked payment app.
     */
    public static long getSourceIdForPaymentAppFromScope(GURL swScope) {
        return ServiceWorkerPaymentAppBridgeJni.get().getSourceIdForPaymentAppFromScope(swScope);
    }

    @CalledByNative
    private static void onHasServiceWorkerPaymentApps(
            HasServiceWorkerPaymentAppsCallback callback, boolean hasPaymentApps) {
        ThreadUtils.assertOnUiThread();

        callback.onHasServiceWorkerPaymentAppsResponse(hasPaymentApps);
    }

    @CalledByNative
    private static Object createPaymentAppsInfo() {
        return new HashMap<String, Pair<String, Bitmap>>();
    }

    @SuppressWarnings("unchecked")
    @CalledByNative
    private static void addPaymentAppInfo(
            Object appsInfo,
            @JniType("std::string") String scope,
            @JniType("std::string") String name,
            @Nullable Bitmap icon) {
        ((Map<String, Pair<String, Bitmap>>) appsInfo).put(scope, new Pair<>(name, icon));
    }

    @SuppressWarnings("unchecked")
    @CalledByNative
    private static void onGetServiceWorkerPaymentAppsInfo(
            GetServiceWorkerPaymentAppsInfoCallback callback, Object appsInfo) {
        ThreadUtils.assertOnUiThread();

        callback.onGetServiceWorkerPaymentAppsInfo(((Map<String, Pair<String, Bitmap>>) appsInfo));
    }

    @NativeMethods
    interface Natives {
        void hasServiceWorkerPaymentApps(HasServiceWorkerPaymentAppsCallback callback);

        void getServiceWorkerPaymentAppsInfo(GetServiceWorkerPaymentAppsInfoCallback callback);

        void onClosingPaymentAppWindow(WebContents paymentRequestWebContents, int reason);

        void onOpeningPaymentAppWindow(
                WebContents paymentRequestWebContents, WebContents paymentHandlerWebContents);

        long getSourceIdForPaymentAppFromScope(GURL swScope);
    }
}
