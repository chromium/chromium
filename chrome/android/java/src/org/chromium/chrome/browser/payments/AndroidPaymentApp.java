// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Parcelable;
import android.os.RemoteException;
import android.util.JsonWriter;

import androidx.annotation.Nullable;

import org.chromium.IsReadyToPayService;
import org.chromium.IsReadyToPayServiceCallback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.components.payments.ErrorStrings;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentCurrencyAmount;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.WindowAndroid;

import java.io.IOException;
import java.io.StringWriter;
import java.net.URI;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * The point of interaction with a locally installed 3rd party native Android payment app.
 * https://docs.google.com/document/d/1izV4uC-tiRJG3JLooqY3YRLU22tYOsLTNq0P_InPJeE
 */
public class AndroidPaymentApp
        extends PaymentInstrument implements PaymentApp, WindowAndroid.IntentCallback {
    /** The action name for the Pay Intent. */
    public static final String ACTION_PAY = "org.chromium.intent.action.PAY";

    /** The maximum number of milliseconds to wait for a response from a READY_TO_PAY service. */
    private static final long READY_TO_PAY_TIMEOUT_MS = 400;

    /** The maximum number of milliseconds to wait for a connection to READY_TO_PAY service. */
    private static final long SERVICE_CONNECTION_TIMEOUT_MS = 1000;

    // Deprecated parameters sent to the payment app for backward compatibility.
    private static final String EXTRA_DEPRECATED_CERTIFICATE_CHAIN = "certificateChain";
    private static final String EXTRA_DEPRECATED_DATA = "data";
    private static final String EXTRA_DEPRECATED_DATA_MAP = "dataMap";
    private static final String EXTRA_DEPRECATED_DETAILS = "details";
    private static final String EXTRA_DEPRECATED_ID = "id";
    private static final String EXTRA_DEPRECATED_IFRAME_ORIGIN = "iframeOrigin";
    private static final String EXTRA_DEPRECATED_METHOD_NAME = "methodName";
    private static final String EXTRA_DEPRECATED_ORIGIN = "origin";

    // Freshest parameters sent to the payment app.
    private static final String EXTRA_CERTIFICATE = "certificate";
    private static final String EXTRA_MERCHANT_NAME = "merchantName";
    private static final String EXTRA_METHOD_DATA = "methodData";
    private static final String EXTRA_METHOD_NAMES = "methodNames";
    private static final String EXTRA_MODIFIERS = "modifiers";
    private static final String EXTRA_PAYMENT_REQUEST_ID = "paymentRequestId";
    private static final String EXTRA_PAYMENT_REQUEST_ORIGIN = "paymentRequestOrigin";
    private static final String EXTRA_TOP_CERTIFICATE_CHAIN = "topLevelCertificateChain";
    private static final String EXTRA_TOP_ORIGIN = "topLevelOrigin";
    private static final String EXTRA_TOTAL = "total";

    // Response from the payment app.
    private static final String EXTRA_DEPRECATED_RESPONSE_INSTRUMENT_DETAILS = "instrumentDetails";
    private static final String EXTRA_RESPONSE_DETAILS = "details";
    private static final String EXTRA_RESPONSE_METHOD_NAME = "methodName";

    private static final String EMPTY_JSON_DATA = "{}";

    private final Handler mHandler;
    private final WebContents mWebContents;
    private final Intent mIsReadyToPayIntent;
    private final Intent mPayIntent;
    private final Set<String> mMethodNames;
    private final boolean mIsIncognito;
    private InstrumentsCallback mInstrumentsCallback;
    private InstrumentDetailsCallback mInstrumentDetailsCallback;
    private ServiceConnection mServiceConnection;
    @Nullable
    private URI mCanDedupedApplicationId;
    private boolean mIsReadyToPayQueried;
    private boolean mIsServiceBindingInitiated;

    /**
     * Builds the point of interaction with a locally installed 3rd party native Android payment
     * app.
     *
     * @param webContents             The web contents.
     * @param packageName             The name of the package of the payment app.
     * @param activity                The name of the payment activity in the payment app.
     * @param label                   The UI label to use for the payment app.
     * @param icon                    The icon to use in UI for the payment app.
     * @param isIncognito             Whether the user is in incognito mode.
     * @param canDedupedApplicationId The corresponding app Id this app can deduped.
     */
    public AndroidPaymentApp(WebContents webContents, String packageName, String activity,
            String label, Drawable icon, boolean isIncognito,
            @Nullable URI canDedupedApplicationId) {
        super(packageName, label, null, icon);
        ThreadUtils.assertOnUiThread();
        mHandler = new Handler();
        mWebContents = webContents;
        mPayIntent = new Intent();
        mIsReadyToPayIntent = new Intent();
        mIsReadyToPayIntent.setPackage(packageName);
        mPayIntent.setClassName(packageName, activity);
        mPayIntent.setAction(ACTION_PAY);
        mMethodNames = new HashSet<>();
        mIsIncognito = isIncognito;
        mCanDedupedApplicationId = canDedupedApplicationId;
    }

    /** @param methodName A payment method that this app supports, e.g., "https://bobpay.com". */
    public void addMethodName(String methodName) {
        mMethodNames.add(methodName);
    }

    /** @param className The class name of the "is ready to pay" service in the payment app. */
    public void setIsReadyToPayAction(String className) {
        mIsReadyToPayIntent.setClassName(mIsReadyToPayIntent.getPackage(), className);
    }

    @Override
    public void getInstruments(String unusedId, Map<String, PaymentMethodData> methodDataMap,
            String origin, String iframeOrigin, @Nullable byte[][] certificateChain,
            Map<String, PaymentDetailsModifier> modifiers, InstrumentsCallback callback) {
        assert mMethodNames.containsAll(methodDataMap.keySet());
        assert mInstrumentsCallback
                == null : "Have not responded to previous request for instruments yet";

        mInstrumentsCallback = callback;
        if (mIsReadyToPayIntent.getComponent() == null) {
            respondToGetInstrumentsQuery(AndroidPaymentApp.this);
            return;
        }

        assert !mIsIncognito;
        mServiceConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                IsReadyToPayService isReadyToPayService =
                        IsReadyToPayService.Stub.asInterface(service);
                if (isReadyToPayService == null) {
                    respondToGetInstrumentsQuery(null);
                } else {
                    sendIsReadyToPayIntentToPaymentApp(isReadyToPayService);
                }
            }

            // "Called when a connection to the Service has been lost. This typically happens when
            // the process hosting the service has crashed or been killed. This does not remove the
            // ServiceConnection itself -- this binding to the service will remain active, and you
            // will receive a call to onServiceConnected(ComponentName, IBinder) when the Service is
            // next running."
            // https://developer.android.com/reference/android/content/ServiceConnection.html#onServiceDisconnected(android.content.ComponentName)
            @Override
            public void onServiceDisconnected(ComponentName name) {
                // Do not wait for the service to restart.
                respondToGetInstrumentsQuery(null);
            }
        };

        mIsReadyToPayIntent.putExtras(buildExtras(null /* id */, null /* merchantName */,
                removeUrlScheme(origin), removeUrlScheme(iframeOrigin), certificateChain,
                methodDataMap, null /* total */, null /* displayItems */, null /* modifiers */));
        try {
            // This method returns "true if the system is in the process of bringing up a service
            // that your client has permission to bind to; false if the system couldn't find the
            // service or if your client doesn't have permission to bind to it. If this value is
            // true, you should later call unbindService(ServiceConnection) to release the
            // connection."
            // https://developer.android.com/reference/android/content/Context.html#bindService(android.content.Intent,%20android.content.ServiceConnection,%20int)
            mIsServiceBindingInitiated = ContextUtils.getApplicationContext().bindService(
                    mIsReadyToPayIntent, mServiceConnection, Context.BIND_AUTO_CREATE);
        } catch (SecurityException e) {
            // Intentionally blank, so mIsServiceBindingInitiated is false.
        }

        if (!mIsServiceBindingInitiated) {
            respondToGetInstrumentsQuery(null);
            return;
        }

        mHandler.postDelayed(() -> {
            if (!mIsReadyToPayQueried) respondToGetInstrumentsQuery(null);
        }, SERVICE_CONNECTION_TIMEOUT_MS);
    }

    private void respondToGetInstrumentsQuery(final PaymentInstrument instrument) {
        if (mServiceConnection != null) {
            if (mIsServiceBindingInitiated) {
                // mServiceConnection "parameter must not be null."
                // https://developer.android.com/reference/android/content/Context.html#unbindService(android.content.ServiceConnection)
                ContextUtils.getApplicationContext().unbindService(mServiceConnection);
                mIsServiceBindingInitiated = false;
            }
            mServiceConnection = null;
        }

        if (mInstrumentsCallback == null) return;
        mHandler.post(() -> {
            ThreadUtils.assertOnUiThread();
            if (mInstrumentsCallback == null) return;
            List<PaymentInstrument> instruments = null;
            if (instrument != null) {
                instruments = new ArrayList<>();
                instruments.add(instrument);
            }
            mInstrumentsCallback.onInstrumentsReady(AndroidPaymentApp.this, instruments);
            mInstrumentsCallback = null;
        });
    }

    private void sendIsReadyToPayIntentToPaymentApp(IsReadyToPayService isReadyToPayService) {
        if (mInstrumentsCallback == null) return;
        mIsReadyToPayQueried = true;
        IsReadyToPayServiceCallback.Stub callback = new IsReadyToPayServiceCallback.Stub() {
            @Override
            public void handleIsReadyToPay(boolean isReadyToPay) throws RemoteException {
                if (isReadyToPay) {
                    respondToGetInstrumentsQuery(AndroidPaymentApp.this);
                } else {
                    respondToGetInstrumentsQuery(null);
                }
            }
        };
        try {
            isReadyToPayService.isReadyToPay(callback);
        } catch (Throwable e) {
            // Many undocumented exceptions are not caught in the remote Service but passed on to
            // the Service caller, see writeException in Parcel.java.
            respondToGetInstrumentsQuery(null);
            return;
        }
        mHandler.postDelayed(() -> respondToGetInstrumentsQuery(null), READY_TO_PAY_TIMEOUT_MS);
    }

    @Override
    public boolean supportsMethodsAndData(Map<String, PaymentMethodData> methodsAndData) {
        assert methodsAndData != null;
        Set<String> methodNames = new HashSet<>(methodsAndData.keySet());
        methodNames.retainAll(getAppMethodNames());
        return !methodNames.isEmpty();
    }

    @Override
    public URI getCanDedupedApplicationId() {
        return mCanDedupedApplicationId;
    }

    @Override
    public String getAppIdentifier() {
        return getIdentifier();
    }

    @Override
    public Set<String> getAppMethodNames() {
        return Collections.unmodifiableSet(mMethodNames);
    }

    @Override
    public Set<String> getInstrumentMethodNames() {
        return getAppMethodNames();
    }

    @Override
    public void invokePaymentApp(final String id, final String merchantName, String origin,
            String iframeOrigin, final byte[][] certificateChain,
            final Map<String, PaymentMethodData> methodDataMap, final PaymentItem total,
            final List<PaymentItem> displayItems,
            final Map<String, PaymentDetailsModifier> modifiers,
            InstrumentDetailsCallback callback) {
        mInstrumentDetailsCallback = callback;

        final String schemelessOrigin = removeUrlScheme(origin);
        final String schemelessIframeOrigin = removeUrlScheme(iframeOrigin);
        if (!mIsIncognito) {
            launchPaymentApp(id, merchantName, schemelessOrigin, schemelessIframeOrigin,
                    certificateChain, methodDataMap, total, displayItems, modifiers);
            return;
        }

        ChromeActivity activity = ChromeActivity.fromWebContents(mWebContents);
        if (activity == null) {
            notifyErrorInvokingPaymentApp(ErrorStrings.ACTIVITY_NOT_FOUND);
            return;
        }

        new UiUtils.CompatibleAlertDialogBuilder(activity, R.style.Theme_Chromium_AlertDialog)
                .setTitle(R.string.external_app_leave_incognito_warning_title)
                .setMessage(R.string.external_payment_app_leave_incognito_warning)
                .setPositiveButton(R.string.ok,
                        (OnClickListener) (dialog, which)
                                -> launchPaymentApp(id, merchantName, schemelessOrigin,
                                        schemelessIframeOrigin, certificateChain, methodDataMap,
                                        total, displayItems, modifiers))
                .setNegativeButton(R.string.cancel,
                        (OnClickListener) (dialog, which)
                                -> notifyErrorInvokingPaymentApp(ErrorStrings.USER_CANCELLED))
                .setOnCancelListener(
                        dialog -> notifyErrorInvokingPaymentApp(ErrorStrings.USER_CANCELLED))
                .show();
    }

    private static String removeUrlScheme(String url) {
        return UrlFormatter.formatUrlForSecurityDisplayOmitScheme(url);
    }

    private void launchPaymentApp(String id, String merchantName, String origin,
            String iframeOrigin, byte[][] certificateChain,
            Map<String, PaymentMethodData> methodDataMap, PaymentItem total,
            List<PaymentItem> displayItems, Map<String, PaymentDetailsModifier> modifiers) {
        assert mMethodNames.containsAll(methodDataMap.keySet());
        assert mInstrumentDetailsCallback != null;

        if (mWebContents.isDestroyed()) {
            notifyErrorInvokingPaymentApp(ErrorStrings.PAYMENT_APP_LAUNCH_FAIL);
            return;
        }

        WindowAndroid window = mWebContents.getTopLevelNativeWindow();
        if (window == null) {
            notifyErrorInvokingPaymentApp(ErrorStrings.PAYMENT_APP_LAUNCH_FAIL);
            return;
        }

        mPayIntent.putExtras(buildExtras(id, merchantName, origin, iframeOrigin, certificateChain,
                methodDataMap, total, displayItems, modifiers));
        try {
            if (!window.showIntent(mPayIntent, this, R.string.payments_android_app_error)) {
                notifyErrorInvokingPaymentApp(ErrorStrings.PAYMENT_APP_LAUNCH_FAIL);
            }
        } catch (SecurityException e) {
            // Payment app does not have android:exported="true" on the PAY activity.
            notifyErrorInvokingPaymentApp(ErrorStrings.PAYMENT_APP_PRIVATE_ACTIVITY);
        }
    }

    private static Bundle buildExtras(@Nullable String id, @Nullable String merchantName,
            String origin, String iframeOrigin, @Nullable byte[][] certificateChain,
            Map<String, PaymentMethodData> methodDataMap, @Nullable PaymentItem total,
            @Nullable List<PaymentItem> displayItems,
            @Nullable Map<String, PaymentDetailsModifier> modifiers) {
        Bundle extras = new Bundle();

        if (id != null) extras.putString(EXTRA_PAYMENT_REQUEST_ID, id);

        if (merchantName != null) extras.putString(EXTRA_MERCHANT_NAME, merchantName);

        extras.putString(EXTRA_TOP_ORIGIN, origin);

        extras.putString(EXTRA_PAYMENT_REQUEST_ORIGIN, iframeOrigin);

        Parcelable[] serializedCertificateChain = null;
        if (certificateChain != null && certificateChain.length > 0) {
            serializedCertificateChain = buildCertificateChain(certificateChain);
            extras.putParcelableArray(EXTRA_TOP_CERTIFICATE_CHAIN, serializedCertificateChain);
        }

        extras.putStringArrayList(EXTRA_METHOD_NAMES, new ArrayList<>(methodDataMap.keySet()));

        Bundle methodDataBundle = new Bundle();
        for (Map.Entry<String, PaymentMethodData> methodData : methodDataMap.entrySet()) {
            methodDataBundle.putString(methodData.getKey(),
                    methodData.getValue() == null ? EMPTY_JSON_DATA
                                                  : methodData.getValue().stringifiedData);
        }
        extras.putParcelable(EXTRA_METHOD_DATA, methodDataBundle);

        if (modifiers != null) {
            extras.putString(EXTRA_MODIFIERS, serializeModifiers(modifiers.values()));
        }

        if (total != null) {
            String serializedTotalAmount = serializeTotalAmount(total.amount);
            extras.putString(EXTRA_TOTAL,
                    serializedTotalAmount == null ? EMPTY_JSON_DATA : serializedTotalAmount);
        }

        return addDeprecatedExtras(id, origin, iframeOrigin, serializedCertificateChain,
                methodDataMap, methodDataBundle, total, displayItems, extras);
    }

    private static Bundle addDeprecatedExtras(@Nullable String id, String origin,
            String iframeOrigin, @Nullable Parcelable[] serializedCertificateChain,
            Map<String, PaymentMethodData> methodDataMap, Bundle methodDataBundle,
            @Nullable PaymentItem total, @Nullable List<PaymentItem> displayItems, Bundle extras) {
        if (id != null) extras.putString(EXTRA_DEPRECATED_ID, id);

        extras.putString(EXTRA_DEPRECATED_ORIGIN, origin);

        extras.putString(EXTRA_DEPRECATED_IFRAME_ORIGIN, iframeOrigin);

        if (serializedCertificateChain != null) {
            extras.putParcelableArray(
                    EXTRA_DEPRECATED_CERTIFICATE_CHAIN, serializedCertificateChain);
        }

        String methodName = methodDataMap.entrySet().iterator().next().getKey();
        extras.putString(EXTRA_DEPRECATED_METHOD_NAME, methodName);

        PaymentMethodData firstMethodData = methodDataMap.get(methodName);
        extras.putString(EXTRA_DEPRECATED_DATA,
                firstMethodData == null ? EMPTY_JSON_DATA : firstMethodData.stringifiedData);

        extras.putParcelable(EXTRA_DEPRECATED_DATA_MAP, methodDataBundle);

        String details = deprecatedSerializeDetails(total, displayItems);
        extras.putString(EXTRA_DEPRECATED_DETAILS, details == null ? EMPTY_JSON_DATA : details);

        return extras;
    }

    private static Parcelable[] buildCertificateChain(byte[][] certificateChain) {
        Parcelable[] result = new Parcelable[certificateChain.length];
        for (int i = 0; i < certificateChain.length; i++) {
            Bundle bundle = new Bundle();
            bundle.putByteArray(EXTRA_CERTIFICATE, certificateChain[i]);
            result[i] = bundle;
        }
        return result;
    }

    private void notifyErrorInvokingPaymentApp(String errorMessage) {
        mHandler.post(() -> mInstrumentDetailsCallback.onInstrumentDetailsError(errorMessage));
    }

    private static String deprecatedSerializeDetails(
            @Nullable PaymentItem total, @Nullable List<PaymentItem> displayItems) {
        StringWriter stringWriter = new StringWriter();
        JsonWriter json = new JsonWriter(stringWriter);
        try {
            // details {{{
            json.beginObject();

            if (total != null) {
                // total {{{
                json.name("total");
                serializeTotal(total, json);
                // }}} total
            }

            // displayitems {{{
            if (displayItems != null) {
                json.name("displayItems").beginArray();
                // Do not pass any display items to the payment app.
                json.endArray();
            }
            // }}} displayItems

            json.endObject();
            // }}} details
        } catch (IOException e) {
            return null;
        }

        return stringWriter.toString();
    }

    private static String serializeTotalAmount(PaymentCurrencyAmount totalAmount) {
        StringWriter stringWriter = new StringWriter();
        JsonWriter json = new JsonWriter(stringWriter);
        try {
            // {{{
            json.beginObject();
            json.name("currency").value(totalAmount.currency);
            json.name("value").value(totalAmount.value);
            json.endObject();
            // }}}
        } catch (IOException e) {
            return null;
        }
        return stringWriter.toString();
    }

    private static void serializeTotal(PaymentItem item, JsonWriter json)
            throws IOException {
        // item {{{
        json.beginObject();
        // Sanitize the total name, because the payment app does not need it to complete the
        // transaction. Matches the behavior of:
        // https://w3c.github.io/payment-handler/#total-attribute
        json.name("label").value("");

        // amount {{{
        json.name("amount").beginObject();
        json.name("currency").value(item.amount.currency);
        json.name("value").value(item.amount.value);
        json.endObject();
        // }}} amount

        json.endObject();
        // }}} item
    }

    private static String serializeModifiers(Collection<PaymentDetailsModifier> modifiers) {
        StringWriter stringWriter = new StringWriter();
        JsonWriter json = new JsonWriter(stringWriter);
        try {
            json.beginArray();
            for (PaymentDetailsModifier modifier : modifiers) {
                serializeModifier(modifier, json);
            }
            json.endArray();
        } catch (IOException e) {
            return EMPTY_JSON_DATA;
        }
        return stringWriter.toString();
    }

    private static void serializeModifier(PaymentDetailsModifier modifier, JsonWriter json)
            throws IOException {
        // {{{
        json.beginObject();

        // total {{{
        if (modifier.total != null) {
            json.name("total");
            serializeTotal(modifier.total, json);
        } else {
            json.name("total").nullValue();
        }
        // }}} total

        // TODO(https://crbug.com/754779): The supportedMethods field was already changed from array
        // to string but we should keep backward-compatibility for now.
        // supportedMethods {{{
        json.name("supportedMethods").beginArray();
        json.value(modifier.methodData.supportedMethod);
        json.endArray();
        // }}} supportedMethods

        // data {{{
        json.name("data").value(modifier.methodData.stringifiedData);
        // }}}

        json.endObject();
        // }}}
    }

    @Override
    public void onIntentCompleted(WindowAndroid window, int resultCode, Intent data) {
        ThreadUtils.assertOnUiThread();
        window.removeIntentCallback(this);
        if (data == null) {
            mInstrumentDetailsCallback.onInstrumentDetailsError(ErrorStrings.MISSING_INTENT_DATA);
        } else if (data.getExtras() == null) {
            mInstrumentDetailsCallback.onInstrumentDetailsError(ErrorStrings.MISSING_INTENT_EXTRAS);
        } else if (resultCode == Activity.RESULT_CANCELED) {
            mInstrumentDetailsCallback.onInstrumentDetailsError(ErrorStrings.RESULT_CANCELED);
        } else if (resultCode != Activity.RESULT_OK) {
            mInstrumentDetailsCallback.onInstrumentDetailsError(String.format(
                    Locale.US, ErrorStrings.UNRECOGNIZED_ACTIVITY_RESULT, resultCode));
        } else {
            String details = data.getExtras().getString(EXTRA_RESPONSE_DETAILS);
            if (details == null) {
                details = data.getExtras().getString(EXTRA_DEPRECATED_RESPONSE_INSTRUMENT_DETAILS);
            }
            if (details == null) details = EMPTY_JSON_DATA;
            String methodName = data.getExtras().getString(EXTRA_RESPONSE_METHOD_NAME);
            if (methodName == null) methodName = "";
            mInstrumentDetailsCallback.onInstrumentDetailsReady(methodName, details);
        }
        mInstrumentDetailsCallback = null;
    }

    @Override
    public void dismissInstrument() {}
}
