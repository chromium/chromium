// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.graphics.drawable.BitmapDrawable;
import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.PaymentHandlerHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentRequestDetailsUpdate;

import java.net.URI;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * This app class represents a service worker based payment app.
 *
 * Such apps are implemented as service workers according to the Payment
 * Handler API specification.
 *
 * @see https://w3c.github.io/payment-handler/
 */
public class ServiceWorkerPaymentApp extends PaymentInstrument implements PaymentApp {
    private final WebContents mWebContents;
    private final long mRegistrationId;
    private final URI mScope;
    private final Set<String> mMethodNames;
    private final boolean mExplicitlyVerified;
    private final Capabilities[] mCapabilities;
    private final boolean mCanPreselect;
    private final Set<String> mPreferredRelatedApplicationIds;
    private final boolean mIsIncognito;
    private final SupportedDelegations mSupportedDelegations;

    // Below variables are used for installable service worker payment app specifically.
    private final boolean mNeedsInstallation;
    private final String mAppName;
    private final URI mSwUri;
    private final boolean mUseCache;

    /* The endpoint for payment handler communication, such as the
     * change-[payment-method|shipping-address|shipping-option] events.
     */
    private PaymentHandlerHost mPaymentHandlerHost;

    /**
     * This class represents capabilities of a payment instrument. It is currently only used for
     * 'basic-card' payment instrument.
     */
    protected static class Capabilities {
        // Stores mojom::BasicCardNetwork.
        private int[] mSupportedCardNetworks;

        // Stores mojom::BasicCardType.
        private int[] mSupportedCardTypes;

        /**
         * Build capabilities for a payment instrument.
         *
         * @param supportedCardNetworks The supported card networks of a 'basic-card' payment
         *                              instrument.
         * @param supportedCardTypes    The supported card types of a 'basic-card' payment
         *                              instrument.
         */
        /* package */ Capabilities(int[] supportedCardNetworks, int[] supportedCardTypes) {
            mSupportedCardNetworks = supportedCardNetworks;
            mSupportedCardTypes = supportedCardTypes;
        }

        /**
         * Gets supported card networks.
         *
         * @return a set of mojom::BasicCardNetwork.
         */
        /* package */ int[] getSupportedCardNetworks() {
            return mSupportedCardNetworks;
        }

        /**
         * Gets supported card types.
         *
         * @return a set of mojom::BasicCardType.
         */
        /* package */ int[] getSupportedCardTypes() {
            return mSupportedCardTypes;
        }
    }

    /**
     * This class represents the supported delegations of a service worker based payment app.
     */
    protected static class SupportedDelegations {
        private final boolean mShippingAddress;
        private final boolean mPayerName;
        private final boolean mPayerPhone;
        private final boolean mPayerEmail;

        SupportedDelegations(boolean shippingAddress, boolean payerName, boolean payerPhone,
                boolean payerEmail) {
            mShippingAddress = shippingAddress;
            mPayerName = payerName;
            mPayerPhone = payerPhone;
            mPayerEmail = payerEmail;
        }
        SupportedDelegations() {
            mShippingAddress = false;
            mPayerName = false;
            mPayerPhone = false;
            mPayerEmail = false;
        }
    }

    /**
     * Build a service worker payment app instance per origin.
     *
     * @see https://w3c.github.io/webpayments-payment-handler/#structure-of-a-web-payment-app
     *
     * @param webContents                    The web contents where PaymentRequest was invoked.
     * @param registrationId                 The registration id of the corresponding service worker
     *                                       payment app.
     * @param scope                          The registration scope of the corresponding service
     *                                       worker.
     * @param name                           The name of the payment app.
     * @param userHint                       The user hint of the payment app.
     * @param origin                         The origin of the payment app.
     * @param icon                           The drawable icon of the payment app.
     * @param methodNames                    A set of payment method names supported by the payment
     *                                       app.
     * @param explicitlyVerified             A flag indicates whether this app has explicitly
     *                                       verified payment methods, like listed as default
     *                                       application or supported origin in the payment methods'
     *                                       manifest.
     * @param capabilities                   A set of capabilities of the payment instruments in
     *                                       this payment app (only valid for basic-card payment
     *                                       method for now).
     * @param preferredRelatedApplicationIds A set of preferred related application Ids.
     * @param supportedDelegations           Supported delegations of the payment app.
     */
    public ServiceWorkerPaymentApp(WebContents webContents, long registrationId, URI scope,
            @Nullable String name, @Nullable String userHint, String origin,
            @Nullable BitmapDrawable icon, String[] methodNames, boolean explicitlyVerified,
            Capabilities[] capabilities, String[] preferredRelatedApplicationIds,
            SupportedDelegations supportedDelegations) {
        // Do not display duplicate information.
        super(scope.toString(), TextUtils.isEmpty(name) ? origin : name, userHint,
                TextUtils.isEmpty(name) ? null : origin, icon);
        mWebContents = webContents;
        mRegistrationId = registrationId;
        mScope = scope;

        // Name and/or icon are set to null if fetching or processing the corresponding web
        // app manifest failed. Then do not preselect this payment app.
        mCanPreselect = !TextUtils.isEmpty(name) && icon != null;

        mMethodNames = new HashSet<>();
        for (int i = 0; i < methodNames.length; i++) {
            mMethodNames.add(methodNames[i]);
        }

        mExplicitlyVerified = explicitlyVerified;

        mCapabilities = Arrays.copyOf(capabilities, capabilities.length);

        mPreferredRelatedApplicationIds = new HashSet<>();
        Collections.addAll(mPreferredRelatedApplicationIds, preferredRelatedApplicationIds);

        mSupportedDelegations = supportedDelegations;

        ChromeActivity activity = ChromeActivity.fromWebContents(mWebContents);
        mIsIncognito = activity != null && activity.getCurrentTabModel().isIncognito();

        mNeedsInstallation = false;
        mAppName = name;
        mSwUri = null;
        mUseCache = false;
    }

    /**
     * Build a service worker payment app instance which has not been installed yet.
     * The payment app will be installed when paying with it.
     *
     * @param webContents                     The web contents where PaymentRequest was invoked.
     * @param name                            The name of the payment app.
     * @param origin                          The origin of the payment app.
     * @param swUri                           The URI to get the service worker js script.
     * @param scope                           The registration scope of the corresponding service
     *                                        worker.
     * @param useCache                        Whether cache is used to register the service worker.
     * @param icon                            The drawable icon of the payment app.
     * @param methodName                      The supported method name.
     * @param preferredRelatedApplicationIds  A set of preferred related application Ids.
     * @param supportedDelegations            Supported delegations of the payment app.
     */
    public ServiceWorkerPaymentApp(WebContents webContents, @Nullable String name, String origin,
            URI swUri, URI scope, boolean useCache, @Nullable BitmapDrawable icon,
            String methodName, String[] preferredRelatedApplicationIds,
            SupportedDelegations supportedDelegations) {
        // Do not display duplicate information.
        super(scope.toString(), TextUtils.isEmpty(name) ? origin : name, null,
                TextUtils.isEmpty(name) ? null : origin, icon);

        mWebContents = webContents;
        // No registration ID before the app is registered (installed).
        mRegistrationId = -1;
        mScope = scope;
        // If name and/or icon is missing or failed to parse from the web app manifest, then do not
        // preselect this payment app.
        mCanPreselect = !TextUtils.isEmpty(name) && icon != null;
        mMethodNames = new HashSet<>();
        mMethodNames.add(methodName);
        // Installable payment apps must be default application of a payment method.
        mExplicitlyVerified = true;
        mCapabilities = new Capabilities[0];
        mPreferredRelatedApplicationIds = new HashSet<>();
        Collections.addAll(mPreferredRelatedApplicationIds, preferredRelatedApplicationIds);

        mSupportedDelegations = supportedDelegations;

        ChromeActivity activity = ChromeActivity.fromWebContents(mWebContents);
        mIsIncognito = activity != null && activity.getCurrentTabModel().isIncognito();

        mNeedsInstallation = true;
        mAppName = name;
        mSwUri = swUri;
        mUseCache = useCache;
    }

    /**
     * Sets the endpoint for payment handler communication. Must be called before invoking this
     * payment handler.
     * @param host The endpoint for payment handler communication. Should not be null.
     */
    /* package */ void setPaymentHandlerHost(PaymentHandlerHost host) {
        assert host != null;
        mPaymentHandlerHost = host;
    }

    /*package*/ URI getScope() {
        return mScope;
    }

    @Override
    public void getInstruments(String id, Map<String, PaymentMethodData> methodDataMap,
            String origin, String iframeOrigin, byte[][] unusedCertificateChain,
            Map<String, PaymentDetailsModifier> modifiers, final InstrumentsCallback callback) {
        // Do not send canMakePayment event when in incognito mode or basic-card is the only
        // supported payment method or this app needs installation for the payment request or this
        // app has not been explicitly verified.
        if (mIsIncognito || isOnlySupportBasiccard(methodDataMap) || mNeedsInstallation
                || !mExplicitlyVerified) {
            new Handler().post(() -> {
                List<PaymentInstrument> instruments =
                        Collections.singletonList(ServiceWorkerPaymentApp.this);
                callback.onInstrumentsReady(ServiceWorkerPaymentApp.this, instruments);
            });
            return;
        }

        ServiceWorkerPaymentAppBridge.canMakePayment(mWebContents, mRegistrationId,
                mScope.toString(), id, origin, iframeOrigin, new HashSet<>(methodDataMap.values()),
                new HashSet<>(modifiers.values()), (boolean canMakePayment) -> {
                    List<PaymentInstrument> instruments = canMakePayment
                            ? Collections.singletonList(ServiceWorkerPaymentApp.this)
                            : Collections.emptyList();
                    callback.onInstrumentsReady(ServiceWorkerPaymentApp.this, instruments);
                });
    }

    // Returns true if 'basic-card' is the only supported payment method of this payment app in the
    // payment request.
    private boolean isOnlySupportBasiccard(Map<String, PaymentMethodData> methodDataMap) {
        Set<String> requestMethods = new HashSet<>(methodDataMap.keySet());
        requestMethods.retainAll(mMethodNames);
        return requestMethods.size() == 1 && requestMethods.contains(MethodStrings.BASIC_CARD);
    }

    // Matches |requestMethodData|.supportedTypes and |requestMethodData|.supportedNetwokrs for
    // 'basic-card' payment method with the Capabilities in this payment app to determine whether
    // this payment app supports |requestMethodData|.
    private boolean matchBasiccardCapabilities(PaymentMethodData requestMethodData) {
        assert requestMethodData != null;
        // Empty supported card types and networks in payment request method data indicates it
        // supports all card types and networks.
        if (requestMethodData.supportedTypes.length == 0
                && requestMethodData.supportedNetworks.length == 0) {
            return true;
        }
        // Payment app with emtpy capabilities can only match payment request method data with empty
        // supported card types and networks.
        if (mCapabilities.length == 0) return false;

        Set<Integer> requestSupportedTypes = new HashSet<>();
        for (int i = 0; i < requestMethodData.supportedTypes.length; i++) {
            requestSupportedTypes.add(requestMethodData.supportedTypes[i]);
        }
        Set<Integer> requestSupportedNetworks = new HashSet<>();
        for (int i = 0; i < requestMethodData.supportedNetworks.length; i++) {
            requestSupportedNetworks.add(requestMethodData.supportedNetworks[i]);
        }

        // If requestSupportedTypes and requestSupportedNetworks are not empty, match them with the
        // capabilities. Break out of the for loop if a matched capability has been found. So 'j
        // < mCapabilities.length' indicates that there is a matched capability in this payment
        // app.
        int j = 0;
        for (; j < mCapabilities.length; j++) {
            if (!requestSupportedTypes.isEmpty()) {
                int[] supportedTypes = mCapabilities[j].getSupportedCardTypes();

                Set<Integer> capabilitiesSupportedCardTypes = new HashSet<>();
                for (int i = 0; i < supportedTypes.length; i++) {
                    capabilitiesSupportedCardTypes.add(supportedTypes[i]);
                }

                capabilitiesSupportedCardTypes.retainAll(requestSupportedTypes);
                if (capabilitiesSupportedCardTypes.isEmpty()) continue;
            }

            if (!requestSupportedNetworks.isEmpty()) {
                int[] supportedNetworks = mCapabilities[j].getSupportedCardNetworks();

                Set<Integer> capabilitiesSupportedCardNetworks = new HashSet<>();
                for (int i = 0; i < supportedNetworks.length; i++) {
                    capabilitiesSupportedCardNetworks.add(supportedNetworks[i]);
                }

                capabilitiesSupportedCardNetworks.retainAll(requestSupportedNetworks);
                if (capabilitiesSupportedCardNetworks.isEmpty()) continue;
            }

            break;
        }
        return j < mCapabilities.length;
    }

    @Override
    public Set<String> getAppMethodNames() {
        return Collections.unmodifiableSet(mMethodNames);
    }

    @Override
    public boolean supportsMethodsAndData(Map<String, PaymentMethodData> methodsAndData) {
        Set<String> methodNames = new HashSet<>(methodsAndData.keySet());
        methodNames.retainAll(mMethodNames);
        return !methodNames.isEmpty();
    }

    @Override
    public Set<String> getPreferredRelatedApplicationIds() {
        return Collections.unmodifiableSet(mPreferredRelatedApplicationIds);
    }

    @Override
    public String getAppIdentifier() {
        return getIdentifier();
    }

    @Override
    public Set<String> getInstrumentMethodNames() {
        return getAppMethodNames();
    }

    @Override
    public boolean isValidForPaymentMethodData(String method, @Nullable PaymentMethodData data) {
        boolean isSupportedMethod = super.isValidForPaymentMethodData(method, data);
        if (isSupportedMethod && MethodStrings.BASIC_CARD.equals(method) && data != null) {
            return matchBasiccardCapabilities(data);
        }
        return isSupportedMethod;
    }

    @Override
    public void invokePaymentApp(String id, String merchantName, String origin, String iframeOrigin,
            byte[][] unusedCertificateChain, Map<String, PaymentMethodData> methodData,
            PaymentItem total, List<PaymentItem> displayItems,
            Map<String, PaymentDetailsModifier> modifiers, InstrumentDetailsCallback callback) {
        assert mPaymentHandlerHost != null;
        if (mNeedsInstallation) {
            assert !mIsMicrotransaction;
            BitmapDrawable icon = (BitmapDrawable) getDrawableIcon();
            ServiceWorkerPaymentAppBridge.installAndInvokePaymentApp(mWebContents, origin,
                    iframeOrigin, id, new HashSet<>(methodData.values()), total,
                    new HashSet<>(modifiers.values()), mPaymentHandlerHost, callback, mAppName,
                    icon == null ? null : icon.getBitmap(), mSwUri, mScope, mUseCache,
                    mMethodNames.toArray(new String[0])[0]);
        } else {
            ServiceWorkerPaymentAppBridge.invokePaymentApp(mWebContents, mRegistrationId,
                    mScope.toString(), origin, iframeOrigin, id, new HashSet<>(methodData.values()),
                    total, new HashSet<>(modifiers.values()), mPaymentHandlerHost,
                    mIsMicrotransaction, callback);
        }
    }

    @Override
    public void updateWith(PaymentRequestDetailsUpdate response) {
        assert isChangingPaymentMethod();
        mPaymentHandlerHost.updateWith(response);
    }

    @Override
    public void noUpdatedPaymentDetails() {
        assert isChangingPaymentMethod();
        mPaymentHandlerHost.noUpdatedPaymentDetails();
    }

    @Override
    public boolean isChangingPaymentMethod() {
        return mPaymentHandlerHost != null && mPaymentHandlerHost.isChangingPaymentMethod();
    }

    @Override
    public void abortPaymentApp(String id, AbortCallback callback) {
        ServiceWorkerPaymentAppBridge.abortPaymentApp(
                mWebContents, mRegistrationId, mScope.toString(), id, callback);
    }

    @Override
    public void dismissInstrument() {}

    @Override
    public boolean canPreselect() {
        return mCanPreselect;
    }

    @Override
    public boolean handlesShippingAddress() {
        return mSupportedDelegations.mShippingAddress;
    }

    @Override
    public boolean handlesPayerName() {
        return mSupportedDelegations.mPayerName;
    }

    @Override
    public boolean handlesPayerEmail() {
        return mSupportedDelegations.mPayerEmail;
    }

    @Override
    public boolean handlesPayerPhone() {
        return mSupportedDelegations.mPayerPhone;
    }

    @Override
    public boolean isReadyForMicrotransaction() {
        return true; // TODO(https://crbug.com/1000432): Implement microtransactions.
    }

    @Override
    @Nullable
    public String accountBalance() {
        return "18.00"; // TODO(https://crbug.com/1000432): Implement microtransactions.
    }
}
